#include "window/RootWidget.h"

#include "logging/Logger.h"
#include "logging/UiPerformanceMonitor.h"
#include "navigation/SidebarView.h"
#include "pages/EditPage.h"
#include "pages/PreferencesPage.h"
#include "pages/ProjectsPage.h"
#include "preview/PreviewOverlay.h"
#include "update/AppUpdater.h"
#include "update/UpdateButton.h"
#include "window/AppDialog.h"
#include "window/AppTitleBar.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <vkui/core/VkIcon.h>
#include <vkui/widgets/controls/VSplitter.h>
#include <vkui/widgets/panels/VPanelManager.h>
#include <vkui/window/VWindowAgent.h>

#include <algorithm>

namespace qtm {

namespace {
constexpr int kSidebarInitialWidth = 236;
constexpr int kSidebarMinimumExpandedWidth = 208;
constexpr int kSidebarMaximumWidth = 360;
constexpr int kTitleBarHeight = 54;
constexpr int kSidebarButtonInset = 14;
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
constexpr int kMacTrafficLightHeight = 14;
#endif

Qt::KeyboardModifier physicalControlModifier() {
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    // Qt maps the physical macOS Control key to MetaModifier; ControlModifier is Command.
    return Qt::MetaModifier;
#else
    return Qt::ControlModifier;
#endif
}

bool sameProjectPath(const QString& left, const QString& right) {
    if (left.isEmpty() || right.isEmpty()) {
        return false;
    }
#if defined(Q_OS_WIN)
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    return QDir::cleanPath(QFileInfo(left).absoluteFilePath())
               .compare(QDir::cleanPath(QFileInfo(right).absoluteFilePath()), pathCase) == 0;
}

} // namespace

RootWidget::RootWidget(ProjectRepository* repository, RecentProjectsStore* recentProjects,
                       AssetManager* assetManager, ThumbnailCache* thumbnailCache,
                       AppSettings* settings, LanguageManager* languageManager, AppUpdater* updater,
                       vkui::VPanelManager& panelManager, QWidget* parent)
    : QWidget(parent), m_panelManager(panelManager) {
    buildUi(repository, recentProjects, assetManager, thumbnailCache, settings, languageManager,
            updater);
    setupShortcuts();
}

void RootWidget::installWindowAgent(vkui::VWindowAgent* agent) {
    m_windowAgent = agent;
    if (!m_windowAgent || !m_sidebarTitleBar || !m_titleBar) {
        Logger::error(QStringLiteral("ui.window.agent.register rejected missing-widget=1"));
        return;
    }

    if (!m_windowAgent->addTitleBar(m_sidebarTitleBar) || !m_windowAgent->addTitleBar(m_titleBar)) {
        Logger::error(QStringLiteral("ui.window.agent.titlebar.register failed backend=vkui"));
        return;
    }

    const auto registerInteractive = [this](QWidget* control) {
        if (control && !m_windowAgent->setHitTestVisible(control, true)) {
            Logger::warn(QStringLiteral("ui.window.agent.hittest.register failed control=%1")
                             .arg(control->objectName()));
        }
    };

    // Hit-test exclusions are window-scoped in current VkUI. Register each control once even when
    // it overlaps both logical title bars.
    registerInteractive(m_previewOverlay);
    const auto registerInteractiveChildren = [&registerInteractive](QWidget* titleBar) {
        for (QAbstractButton* button : titleBar->findChildren<QAbstractButton*>()) {
            registerInteractive(button);
        }
        for (QLineEdit* edit : titleBar->findChildren<QLineEdit*>()) {
            registerInteractive(edit);
        }
    };
    registerInteractiveChildren(m_sidebarTitleBar);
    for (QWidget* control : m_titleBar->interactiveWidgets()) {
        registerInteractive(control);
    }
    registerInteractive(m_newProjectButton);
    if (m_splitter) {
        registerInteractive(m_splitter->handle(1));
    }
    for (QWidget* edgeHandle : m_panelManager.windowEdgeHandles()) {
        registerInteractive(edgeHandle);
    }
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    m_windowAgent->setTrafficLightOrigin(
        QPoint(12, (kTitleBarHeight - kMacTrafficLightHeight) / 2));
#endif
    setTitleEditorHitTestVisible(m_currentPage == AppPage::Edit && hasActiveProject());
}

bool RootWidget::confirmClose() {
    return !hasActiveProject() || m_editPage->confirmSaveIfDirty();
}

void RootWidget::switchToPage(AppPage page) {
    if (page == AppPage::Preferences) {
        showPreferencesDialog();
        updateSelection(m_currentPage);
        return;
    }
    if (page == AppPage::Edit && !hasActiveProject()) {
        updateSelection(m_currentPage);
        return;
    }

    const int index = stackIndexForPage(page);
    if (index >= 0 && index < m_pages->count()) {
        m_pages->setCurrentIndex(index);
        m_currentPage = page;
        updateSelection(page);
        updatePageMargins(page);
        updateTitleBarForPage(page);
        updateUnsavedIndicators();
    }
}

void RootWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    synchronizeSidebarGeometry(true);
    if (m_previewOverlay) {
        m_previewOverlay->setGeometry(rect());
        if (m_previewOverlay->isOpen()) {
            m_previewOverlay->raise();
        }
    }
}

void RootWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_initialLayoutSynced) {
        synchronizeInitialLayout();
        return;
    }

    m_initialLayoutSynced = true;
    QTimer::singleShot(0, this, [this]() { synchronizeInitialLayout(); });
}

void RootWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        if (m_sidebarTitleBar) {
            m_sidebarTitleBar->update();
        }
        update();
    }
}

void RootWidget::buildUi(ProjectRepository* repository, RecentProjectsStore* recentProjects,
                         AssetManager* assetManager, ThumbnailCache* thumbnailCache,
                         AppSettings* settings, LanguageManager* languageManager,
                         AppUpdater* updater) {
    m_settings = settings;
    m_languageManager = languageManager;
    m_updater = updater;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_splitter = new vkui::VSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("MainSplitter"));
    m_splitter->setOpaqueResize(true);
    root->addWidget(m_splitter, 1);

    m_sidebar = createSidebar(m_splitter);
    m_sidebar->setMinimumWidth(kSidebarMinimumExpandedWidth);
    m_sidebar->setMaximumWidth(kSidebarMaximumWidth);
    m_sidebar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_content = createContent(repository, recentProjects, assetManager, thumbnailCache, settings);

    m_previewOverlay = new PreviewOverlay(this);
    m_previewOverlay->setGeometry(rect());
    m_previewOverlay->setBackgroundMode(settings ? settings->previewBackgroundMode()
                                                 : PreviewBackgroundMode::None);
    m_previewOverlay->setPreviewEffect(settings ? settings->previewEffect()
                                                : BackdropEffect::DepthSoftFocus);
    m_previewOverlay->setLiquidGlassParameters(settings ? settings->liquidGlassParameters()
                                                        : LiquidGlassParameters{});
    m_previewOverlay->setToolTipsEnabled(settings ? settings->tierListToolTipsEnabled() : true);
    connect(m_editPage, &EditPage::imagePreviewRequested, m_previewOverlay,
            &PreviewOverlay::openPreview);
    connect(m_editPage, &EditPage::imagePreviewResetRequested, m_previewOverlay,
            &PreviewOverlay::resetPreview);
    connect(m_editPage, &EditPage::projectViewResetRequested, this, [this]() {
        if (m_tierFocusMode) {
            setTierFocusMode(false);
        }
        if (m_titleBar) {
            // Clearing editability commits no text and guarantees that the incoming project's
            // title replaces any preedit or selection left by the previous project.
            m_titleBar->setTitleEditable(false);
        }
    });
    if (settings) {
        connect(settings, &AppSettings::previewBackgroundModeChanged, m_previewOverlay,
                &PreviewOverlay::setBackgroundMode);
        connect(settings, &AppSettings::previewEffectChanged, m_previewOverlay,
                &PreviewOverlay::setPreviewEffect);
        connect(settings, &AppSettings::liquidGlassParametersChanged, m_previewOverlay,
                [this, settings]() {
                    m_previewOverlay->setLiquidGlassParameters(settings->liquidGlassParameters());
                });
        connect(settings, &AppSettings::tierListToolTipsEnabledChanged, m_previewOverlay,
                &PreviewOverlay::setToolTipsEnabled);
    }
    connect(m_previewOverlay, &PreviewOverlay::opened, this, [this]() {
        if (m_windowAgent && !m_previewSystemButtonStateCaptured) {
            m_previewSavedSystemButtonsVisible = m_windowAgent->systemButtonsVisible();
            m_previewSystemButtonStateCaptured = true;
            m_windowAgent->setSystemButtonsVisible(false);
        }
        m_panelManager.closePanelChooser();
        setPanelControlsVisible(false);
        Logger::info(QStringLiteral("ui.preview.input.barrier enabled=1 scope=window"));
    });
    connect(m_previewOverlay, &PreviewOverlay::closed, this, [this]() {
        if (m_windowAgent && m_previewSystemButtonStateCaptured) {
            m_windowAgent->setSystemButtonsVisible(m_previewSavedSystemButtonsVisible);
        }
        m_previewSystemButtonStateCaptured = false;
        if (!m_tierFocusMode) {
            setPanelControlsVisible(true);
        }
        Logger::info(QStringLiteral("ui.preview.input.barrier enabled=0 scope=window"));
    });
    connect(m_previewOverlay, &PreviewOverlay::closed, m_editPage,
            &EditPage::restoreGalleryAfterPreview);

    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_content);
    m_splitter->setChildrenCollapsible(true);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    const bool panelRootRegistered = m_panelManager.setLayoutRoot(m_splitter);
    const bool navigationRegistered = m_panelManager.registerPanel(
        QStringLiteral("navigation"), tr("Navigation"), m_sidebar, 1);
    const bool contentRegistered =
        m_panelManager.registerPanel(QStringLiteral("content"), tr("Content"), m_content, 2);
    if (!panelRootRegistered || !navigationRegistered || !contentRegistered) {
        Logger::error(
            QStringLiteral("ui.panel.register failed root=%1 navigation=%2 content=%3")
                .arg(panelRootRegistered)
                .arg(navigationRegistered)
                .arg(contentRegistered));
    }
    Q_ASSERT(panelRootRegistered && navigationRegistered && contentRegistered);
    // VkUI wraps each managed panel in an animation slot. Programmatic setSizes() does not
    // emit splitterMoved, so follow the slot's geometry throughout collapse and restore.
    m_sidebarSlot = m_sidebar->parentWidget();
    if (m_sidebarSlot && m_sidebarSlot != m_splitter) {
        m_sidebarSlot->installEventFilter(this);
    }
    connect(m_sidebarView, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (!index.flags().testFlag(Qt::ItemIsEnabled)) {
            updateSelection(m_currentPage);
            return;
        }
        switchToPage(m_sidebarModel->pageForIndex(index));
    });
    connect(m_splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        synchronizeSidebarGeometry();
    });
    connect(&m_panelManager, &vkui::VPanelManager::panelExpandedChanged, this,
            [this](const QString& id, bool expanded) {
                if (id != QStringLiteral("navigation")) {
                    return;
                }
                synchronizeSidebarGeometry();
                Logger::info(QStringLiteral("ui.panel.navigation.target expanded=%1 startWidth=%2")
                                 .arg(expanded)
                                 .arg(m_currentSidebarWidth));
            });

    // Keep the action inside the managed sidebar so its animation slot clips the button
    // during collapse instead of letting it float over the content panel.
    m_newProjectButton = new QToolButton(m_sidebarTitleBar);
    m_newProjectButton->setObjectName(QStringLiteral("NewProjectButton"));
    m_newProjectButton->setToolTip(tr("New"));
    m_newProjectButton->setAccessibleName(tr("New project"));
    m_newProjectButton->setIcon(vkui::icon(vkui::VkSymbol::Plus));
    m_newProjectButton->setIconSize(QSize(20, 20));
    m_newProjectButton->setFixedSize(34, 34);
    m_newProjectButton->setCursor(Qt::ArrowCursor);
    m_newProjectButton->setFocusPolicy(Qt::NoFocus);
    m_newProjectButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_newProjectButton->setAutoRaise(true);
    connect(m_newProjectButton, &QAbstractButton::clicked, this, [this]() {
        if (m_editPage && m_editPage->newProject()) {
            setActiveProjectAvailable(true);
            switchToPage(AppPage::Edit);
        }
    });

    if (updater) {
        connect(updater, &AppUpdater::stateChanged, this,
                [this](UpdateState) { refreshUpdateButton(); });
        connect(
            updater, &AppUpdater::downloadProgress, this, [this](qint64 received, qint64 total) {
                if (!m_updateButton) {
                    return;
                }
                m_updateButton->setDownloadProgress(received, total);
                if (total > 0) {
                    const int percent = qBound(
                        0, qRound(100.0 * static_cast<qreal>(received) / static_cast<qreal>(total)),
                        100);
                    m_updateButton->setToolTip(tr("Downloading update: %1%").arg(percent));
                }
            });
        refreshUpdateButton();
    }

    connect(m_projectsPage, &ProjectsPage::openProjectRequested, this, [this](const QString& path) {
        if (sameProjectPath(m_editPage->currentProjectPath(), path)) {
            setActiveProjectAvailable(true);
            switchToPage(AppPage::Edit);
            return;
        }
        if (m_editPage->confirmSaveIfDirty() && m_editPage->openProject(path)) {
            setActiveProjectAvailable(true);
            switchToPage(AppPage::Edit);
        }
    });
    connect(m_projectsPage, &ProjectsPage::exportProjectImageRequested, m_editPage,
            &EditPage::exportProjectFromPath);
    const auto releaseProjectStorage = [this](const QString& path) {
        if (!m_editPage || !sameProjectPath(m_editPage->currentProjectPath(), path)) {
            return;
        }
        m_editPage->clearProject();
        setActiveProjectAvailable(false);
    };
    connect(m_projectsPage, &ProjectsPage::projectDeleted, this, releaseProjectStorage);
    connect(m_projectsPage, &ProjectsPage::projectAboutToBeReplaced, this, releaseProjectStorage);

    connect(m_editPage, &EditPage::titleChanged, this, [this](const QString& title) {
        if (m_pages && m_pages->currentWidget() == m_editPage && !m_tierFocusMode &&
            hasActiveProject()) {
            m_titleBar->setTitleEditable(true);
            m_titleBar->setDocumentTitle(title);
        }
    });
    connect(m_editPage, &EditPage::dirtyChanged, this, &RootWidget::updateUnsavedIndicators);
    connect(m_editPage, &EditPage::projectSaved, this, &RootWidget::updateUnsavedIndicators);
    connect(m_titleBar, &AppTitleBar::templatesRequested, m_editPage, &EditPage::showTemplateMenu);
    connect(m_titleBar, &AppTitleBar::backgroundRequested, m_editPage,
            &EditPage::configureBackground);
    connect(m_titleBar, &AppTitleBar::galleryRequested, m_editPage, &EditPage::toggleGallery);
    connect(m_titleBar, &AppTitleBar::projectTitleEdited, m_editPage, &EditPage::renameProject);
    connect(m_titleBar, &AppTitleBar::tierFocusModeRequested, this,
            [this]() { setTierFocusMode(!m_tierFocusMode); });
    connect(m_editPage, &EditPage::galleryMissionControlRequested, this, [this]() {
        if (m_editPage) {
            m_editPage->toggleGalleryMissionControlMode();
        }
    });
    connect(languageManager, &LanguageManager::languageChanged, m_sidebarModel,
            &SidebarModel::retranslate);
    connect(languageManager, &LanguageManager::languageChanged, this, [this]() {
        if (m_sidebarView && m_sidebar) {
            const int preferred = std::clamp(m_sidebarView->preferredWidth() + 22,
                                             kSidebarMinimumExpandedWidth, kSidebarMaximumWidth);
            m_sidebar->setMinimumWidth(preferred);
            if (m_panelManager.isPanelExpanded(QStringLiteral("navigation")) &&
                m_currentSidebarWidth < preferred) {
                resizeExpandedSidebar(preferred);
            }
        }
        const bool navigationRegistered = m_panelManager.registerPanel(
            QStringLiteral("navigation"), tr("Navigation"), m_sidebar, 1);
        const bool contentRegistered = m_panelManager.registerPanel(
            QStringLiteral("content"), tr("Content"), m_content, 2);
        Q_ASSERT(navigationRegistered && contentRegistered);
        if (m_titleBar) {
            m_titleBar->retranslateUi();
        }
        if (m_projectsPage) {
            m_projectsPage->retranslateUi();
        }
        if (m_pages) {
            updateTitleBarForPage(m_currentPage);
        }
        if (m_preferencesDialog) {
            m_preferencesDialog->setWindowTitle(tr("Preferences"));
        }
        if (m_newProjectButton) {
            m_newProjectButton->setToolTip(tr("New"));
            m_newProjectButton->setAccessibleName(tr("New project"));
        }
        if (m_preferencesButton) {
            m_preferencesButton->setToolTip(tr("Preferences"));
        }
        refreshUpdateButton();
    });

    m_sidebarModel->setPageEnabled(AppPage::Edit, false);
    m_sidebarModel->setPageDirty(AppPage::Edit, false);
    const int preferredSidebarWidth =
        std::clamp(m_sidebarView ? m_sidebarView->preferredWidth() + 22 : kSidebarInitialWidth,
                   kSidebarMinimumExpandedWidth, kSidebarMaximumWidth);
    m_sidebar->setMinimumWidth(preferredSidebarWidth);
    resizeExpandedSidebar(preferredSidebarWidth);
    switchToPage(AppPage::Projects);
    layoutTitleBars();
    layoutSidebarTitleButton();
    updateTitleBarLeadingReservation();
}

QFrame* RootWidget::createSidebar(QWidget* parent) {
    auto* sidebar = new QFrame(parent);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFrameShape(QFrame::NoFrame);
    sidebar->setBackgroundRole(QPalette::Window);
    sidebar->setAutoFillBackground(true);

    m_sidebarLayout = new QVBoxLayout(sidebar);
    m_sidebarLayout->setContentsMargins(12, kTitleBarHeight + 12, 10, 14);
    m_sidebarLayout->setSpacing(12);

    m_sidebarTitleBar = new QWidget(sidebar);
    m_sidebarTitleBar->setObjectName(QStringLiteral("SidebarTitleBar"));
    m_sidebarTitleBar->setFixedHeight(kTitleBarHeight);
    m_sidebarTitleBar->setAttribute(Qt::WA_StyledBackground, false);

    m_sidebarModel = new SidebarModel(this);
    m_sidebarView = new SidebarView(sidebar);
    m_sidebarView->setModel(m_sidebarModel);
    m_sidebarLayout->addWidget(m_sidebarView);
    m_sidebarLayout->addStretch();

    m_preferencesButton = new QToolButton(sidebar);
    m_preferencesButton->setObjectName(QStringLiteral("PreferencesButton"));
    m_preferencesButton->setIcon(vkui::icon(vkui::VkSymbol::Settings));
    m_preferencesButton->setToolTip(tr("Preferences"));
    m_preferencesButton->setCursor(Qt::ArrowCursor);
    m_preferencesButton->setFixedSize(34, 34);
    m_preferencesButton->setIconSize(QSize(18, 18));
    m_preferencesButton->setAutoRaise(true);
    connect(m_preferencesButton, &QToolButton::clicked, this,
            [this]() { switchToPage(AppPage::Preferences); });

    m_updateButton = new UpdateButton(sidebar);
    m_updateButton->setInstallText(tr("Install"));
    m_updateButton->setReducedMotion(m_settings && m_settings->reducedMotion());
    connect(m_updateButton, &QToolButton::clicked, this, [this]() {
        if (m_updater) {
            m_updater->startUpdate();
        }
    });
    if (m_settings) {
        connect(m_settings, &AppSettings::changed, m_updateButton, [this]() {
            if (m_updateButton && m_settings) {
                m_updateButton->setReducedMotion(m_settings->reducedMotion());
            }
        });
        connect(m_settings, &AppSettings::autoUpdateEnabledChanged, this,
                [this](bool) { refreshUpdateButton(); });
    }

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(6);
    footer->addWidget(m_preferencesButton);
    footer->addWidget(m_updateButton);
    footer->addStretch(1);
    m_sidebarLayout->addLayout(footer);
    return sidebar;
}

QFrame* RootWidget::createContent(ProjectRepository* repository,
                                  RecentProjectsStore* recentProjects, AssetManager* assetManager,
                                  ThumbnailCache* thumbnailCache, AppSettings* settings) {
    auto* content = new QFrame(m_splitter);
    content->setObjectName(QStringLiteral("Content"));
    content->setMinimumWidth(620);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    content->setBackgroundRole(QPalette::Base);
    content->setAutoFillBackground(true);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_pages = new QStackedWidget(content);
    m_pages->setObjectName(QStringLiteral("Pages"));
    m_pages->setContentsMargins(0, 0, 0, 0);
    m_editPage =
        new EditPage(repository, recentProjects, assetManager, thumbnailCache, settings, content);
    m_projectsPage =
        new ProjectsPage(repository, recentProjects, thumbnailCache, settings, content);
    m_pages->addWidget(m_editPage);
    m_pages->addWidget(m_projectsPage);
    contentLayout->addWidget(m_pages, 1);

    // The registered title bar is a transparent native hit-test geometry layer. Keep it below the
    // page surface while AppTitleBar raises its independently parented interactive controls.
    m_titleBar = new AppTitleBar(content);
    m_titleBar->setGeometry(0, 0, content->width(), kTitleBarHeight);
    m_titleBar->stackUnder(m_pages);
    m_titleBar->raiseChrome();
    return content;
}

void RootWidget::resizeExpandedSidebar(int width) {
    if (!m_splitter || !m_sidebar ||
        !m_panelManager.isPanelExpanded(QStringLiteral("navigation"))) {
        return;
    }

    const int sidebarWidth =
        std::clamp(width, m_sidebar->minimumWidth(), kSidebarMaximumWidth);
    const int contentMinimumWidth = m_content ? m_content->minimumWidth() : 620;
    const int totalWidth =
        std::max(m_splitter->width() - m_splitter->handleWidth(),
                 sidebarWidth + contentMinimumWidth);
    m_splitter->setSizes({sidebarWidth, std::max(1, totalWidth - sidebarWidth)});
    synchronizeSidebarGeometry(true);
}

void RootWidget::synchronizeInitialLayout() {
    if (!m_splitter) {
        return;
    }

    if (!m_tierFocusMode &&
        m_panelManager.isPanelExpanded(QStringLiteral("navigation"))) {
        resizeExpandedSidebar(std::max(m_currentSidebarWidth, m_sidebar->minimumWidth()));
    } else {
        synchronizeSidebarGeometry(true);
    }
    if (m_pages) {
        updatePageMargins(m_currentPage);
        updateTitleBarForPage(m_currentPage);
    }
}

void RootWidget::setPanelControlsVisible(bool visible) {
    if (m_splitter && m_splitter->handle(1)) {
        m_splitter->handle(1)->setVisible(visible);
        m_splitter->handle(1)->setEnabled(visible);
    }
    for (QWidget* edgeHandle : m_panelManager.windowEdgeHandles()) {
        edgeHandle->setVisible(visible);
        edgeHandle->setEnabled(visible);
        if (visible) {
            edgeHandle->raise();
        }
    }
}

void RootWidget::setTierFocusMode(bool enabled) {
    if (m_tierFocusMode == enabled) {
        return;
    }

    m_tierFocusMode = enabled;
    if (enabled) {
        qApp->installEventFilter(this);
    } else {
        qApp->removeEventFilter(this);
        m_focusMoveWindow = nullptr;
    }
    UiPerformanceMonitor::setTierFocusMode(enabled);
    Logger::info(QStringLiteral("ui.tier.focus.mode enabled=%1").arg(enabled));

    if (enabled) {
        if (m_windowAgent) {
            m_savedSystemButtonsVisible = m_windowAgent->systemButtonsVisible();
            m_windowAgent->setSystemButtonsVisible(false);
        }
        m_focusSavedNavigationExpanded =
            m_panelManager.isPanelExpanded(QStringLiteral("navigation"));
        m_focusSavedContentExpanded =
            m_panelManager.isPanelExpanded(QStringLiteral("content"));
        m_panelManager.closePanelChooser();
        m_panelManager.setPanelExpanded(QStringLiteral("content"), true);
        m_panelManager.setPanelExpanded(QStringLiteral("navigation"), false);
        setPanelControlsVisible(false);
        if (m_pages) {
            m_pages->setContentsMargins(0, 0, 0, 0);
        }
        if (m_newProjectButton) {
            m_newProjectButton->hide();
        }
        if (m_titleBar) {
            m_titleBar->setTierFocusMode(true);
            m_titleBar->hide();
        }
        if (m_editPage) {
            m_editPage->setTierFocusMode(true);
        }
    } else {
        if (m_windowAgent) {
            m_windowAgent->setSystemButtonsVisible(m_savedSystemButtonsVisible);
        }
        if (m_focusSavedNavigationExpanded) {
            m_panelManager.setPanelExpanded(QStringLiteral("navigation"), true);
        }
        if (m_focusSavedContentExpanded) {
            m_panelManager.setPanelExpanded(QStringLiteral("content"), true);
        }
        if (!m_focusSavedNavigationExpanded) {
            m_panelManager.setPanelExpanded(QStringLiteral("navigation"), false);
        }
        if (!m_focusSavedContentExpanded) {
            m_panelManager.setPanelExpanded(QStringLiteral("content"), false);
        }
        setPanelControlsVisible(true);
        if (m_pages) {
            updatePageMargins(m_currentPage);
        }
        if (m_titleBar) {
            m_titleBar->show();
            m_titleBar->setTierFocusMode(false);
        }
        if (m_editPage) {
            m_editPage->setTierFocusMode(false);
        }
        if (m_newProjectButton) {
            m_newProjectButton->show();
        }
        if (m_pages) {
            updateTitleBarForPage(m_currentPage);
        }
    }

    layoutTitleBars();
    layoutSidebarTitleButton();
}

bool RootWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_sidebarSlot && event &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
         event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        synchronizeSidebarGeometry(true);
    }
    if (!m_tierFocusMode || !event) {
        return QWidget::eventFilter(watched, event);
    }

    if (m_focusMoveWindow && event->type() == QEvent::MouseMove) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->buttons().testFlag(Qt::LeftButton)) {
            m_focusMoveWindow->move(mouseEvent->globalPosition().toPoint() - m_focusMoveOffset);
            mouseEvent->accept();
            return true;
        }
        m_focusMoveWindow = nullptr;
    } else if (m_focusMoveWindow && event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_focusMoveWindow = nullptr;
            mouseEvent->accept();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        auto* target = qobject_cast<QWidget*>(watched);
        if (mouseEvent->button() == Qt::LeftButton &&
            mouseEvent->modifiers().testFlag(Qt::AltModifier) && target &&
            target->window() == window()) {
            // RootWidget is the central widget, so its own windowHandle() is normally null.
            // QWindow::startSystemMove() must be invoked on the top-level native window.
            QWidget* topLevel = target->window();
            QWindow* nativeWindow = topLevel ? topLevel->windowHandle() : nullptr;
            if (nativeWindow && nativeWindow->startSystemMove()) {
                Logger::debug(QStringLiteral("ui.tier.focus.window.move"));
                mouseEvent->accept();
                return true;
            }
            // Some compositors reject native system move for custom full-content windows. Keep a
            // small cross-platform fallback so Option/Alt-drag remains available everywhere.
            if (topLevel) {
                m_focusMoveWindow = topLevel;
                m_focusMoveOffset =
                    mouseEvent->globalPosition().toPoint() - topLevel->frameGeometry().topLeft();
                Logger::warn(QStringLiteral("ui.tier.focus.window.move fallback=manual"));
                mouseEvent->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

int RootWidget::sidebarPanelExtent() const {
    if (m_sidebarSlot) {
        return std::max(0, m_sidebarSlot->width());
    }
    if (m_splitter) {
        const QList<int> sizes = m_splitter->sizes();
        if (!sizes.isEmpty()) {
            return std::max(0, sizes.constFirst());
        }
    }
    return m_sidebar ? std::max(0, m_sidebar->width()) : 0;
}

void RootWidget::synchronizeSidebarGeometry(bool force) {
    const int sidebarWidth = sidebarPanelExtent();
    if (!force && sidebarWidth == m_currentSidebarWidth) {
        return;
    }
    m_currentSidebarWidth = sidebarWidth;
    syncSidebarPresentation(m_currentSidebarWidth);
    layoutTitleBars();
    layoutSidebarTitleButton();
}

void RootWidget::syncSidebarPresentation(int sidebarWidth) {
    if (!m_sidebarView) {
        return;
    }
    // VPanelManager clips the registered panel while animating below its application minimum.
    // Keep the navigation layout stable in that range and let the animation slot reveal it.
    const int layoutWidth = sidebarWidth > 0 && m_sidebar
                                ? std::max(sidebarWidth, m_sidebar->minimumWidth())
                                : sidebarWidth;
    m_sidebarView->setAvailableWidth(qMax(0, layoutWidth - 22));
    // The panel slot clips its content at zero width. Hiding the list here leaves it explicitly
    // hidden after a programmatic splitter animation, which need not emit splitterMoved.
}

void RootWidget::layoutTitleBars() {
    if (m_sidebarTitleBar) {
        m_sidebarTitleBar->setGeometry(0, 0, std::max(0, m_currentSidebarWidth), kTitleBarHeight);
        m_sidebarTitleBar->raise();
    }
    if (m_titleBar) {
        const int contentWidth = m_content ? std::max(0, m_content->width()) : width();
        if (m_tierFocusMode) {
            const int revealWidth =
                std::min(contentWidth, m_titleBar->focusRevealSizeHint().width());
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
            m_titleBar->setGeometry(std::max(0, contentWidth - revealWidth), 0, revealWidth,
                                    kTitleBarHeight);
#else
            m_titleBar->setGeometry(0, 0, revealWidth, kTitleBarHeight);
#endif
        } else {
            m_titleBar->setGeometry(0, 0, contentWidth, kTitleBarHeight);
        }
        if (m_pages) {
            m_titleBar->stackUnder(m_pages);
        }
        m_titleBar->raiseChrome();
    }
    if (m_newProjectButton) {
        m_newProjectButton->raise();
    }
    updateTitleBarLeadingReservation();
    if (m_previewOverlay && m_previewOverlay->isOpen()) {
        m_previewOverlay->raise();
    }
}

void RootWidget::layoutSidebarTitleButton() {
    if (!m_newProjectButton) {
        return;
    }

    const int minimumX = minimumSidebarButtonX();
    const bool hasRoom =
        m_currentSidebarWidth >= minimumX + m_newProjectButton->width() + kSidebarButtonInset;
    m_newProjectButton->setVisible(!m_tierFocusMode && hasRoom);
    if (!hasRoom) {
        updateTitleBarLeadingReservation();
        return;
    }
    const int titleBarWidth = m_sidebarTitleBar ? m_sidebarTitleBar->width() : width();
    const int maximumX =
        std::max(minimumX, titleBarWidth - m_newProjectButton->width() - kSidebarButtonInset);
    const int naturalX = m_currentSidebarWidth > 0
                             ? m_currentSidebarWidth - m_newProjectButton->width() -
                                   kSidebarButtonInset
                             : minimumX;
    const int x = std::clamp(std::max(naturalX, minimumX), minimumX, maximumX);
    const int y = std::max(0, (kTitleBarHeight - m_newProjectButton->height()) / 2);
    m_newProjectButton->move(x, y);
    m_newProjectButton->raise();
    updateTitleBarLeadingReservation();
}

void RootWidget::refreshUpdateButton() {
    if (!m_updateButton || !m_updater) {
        return;
    }
    m_updateButton->setInstallText(tr("Install"));
    // Automatic checks are a scheduling preference. Once an update has been discovered, its
    // download and install state remains actionable even when automatic checking is disabled.
    const UpdateState state = m_updater->state();
    m_updateButton->setUpdateState(state);
    const QString version = m_updater->lastResult().latestVersion;
    switch (state) {
    case UpdateState::Available:
        m_updateButton->setToolTip(version.isEmpty() ? tr("Download update")
                                                     : tr("Download update %1").arg(version));
        break;
    case UpdateState::Downloading:
        m_updateButton->setToolTip(tr("Downloading update"));
        break;
    case UpdateState::Ready:
        m_updateButton->setToolTip(version.isEmpty() ? tr("Install update")
                                                     : tr("Install update %1").arg(version));
        break;
    case UpdateState::Installing:
        m_updateButton->setToolTip(tr("Opening update installer"));
        break;
    default:
        m_updateButton->setToolTip(tr("Updates"));
        break;
    }
}

void RootWidget::showPreferencesDialog() {
    if (m_preferencesDialog) {
        m_preferencesDialog->show();
        m_preferencesDialog->raise();
        m_preferencesDialog->activateWindow();
        return;
    }

    auto* dialog = new AppDialog(tr("Preferences"), window());
    dialog->setObjectName(QStringLiteral("PreferencesDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    // Preferences is a long-lived utility window, not a modal sheet. On macOS a window-modal
    // child is presented with sheet semantics and intentionally has no traffic-light close.
    dialog->setWindowModality(Qt::NonModal);
    dialog->setCloseButtonPlacement(AppDialog::CloseButtonPlacement::Platform);
    dialog->setResizable(true);
    dialog->setMinimumSize(720, 480);
    dialog->resize(860, 560);

    auto* page = new PreferencesPage(m_settings, m_languageManager, m_updater, dialog);
    dialog->contentLayout()->setContentsMargins(0, 0, 0, 0);
    dialog->contentLayout()->addWidget(page, 1);

    m_preferencesDialog = dialog;
    connect(dialog, &QObject::destroyed, this, [this]() { m_preferencesDialog = nullptr; });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

int RootWidget::stackIndexForPage(AppPage page) const {
    switch (page) {
    case AppPage::Edit:
        return 0;
    case AppPage::Projects:
        return 1;
    case AppPage::Preferences:
        return -1;
    }
    return -1;
}

void RootWidget::updateTitleBarLeadingReservation() {
    if (!m_titleBar) {
        return;
    }
    // The New button belongs to the sidebar title bar and cannot overlap content chrome.
    m_titleBar->setLeadingReservedWidth(0);
}

int RootWidget::minimumSidebarButtonX() const {
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    if (m_windowAgent && m_windowAgent->systemButtonsVisible()) {
        const auto origin = m_windowAgent->trafficLightOrigin();
        if (origin.has_value()) {
            return origin->x() + 81;
        }
    }
#endif
    return kSidebarButtonInset;
}

void RootWidget::setupShortcuts() {
    auto addShortcut = [this](const QKeySequence& sequence, const QObject* receiver,
                              const char* slot) {
        auto* shortcut = new QShortcut(sequence, this);
        connect(shortcut, SIGNAL(activated()), receiver, slot);
    };
    auto* newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, [this]() {
        if (m_editPage && m_editPage->newProject()) {
            setActiveProjectAvailable(true);
            switchToPage(AppPage::Edit);
        }
    });
    addShortcut(QKeySequence::Open, m_projectsPage, SLOT(openProjectFromDialog()));
    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setContext(Qt::ApplicationShortcut);
    connect(saveShortcut, &QShortcut::activated, this, [this]() {
        if (m_editPage && hasActiveProject() && m_editPage->isDirty()) {
            m_editPage->saveProject();
        }
    });
    const auto dispatchHistory = [this](bool redo) {
        QWidget* focus = QApplication::focusWidget();
        if (auto* lineEdit = qobject_cast<QLineEdit*>(focus); lineEdit && !lineEdit->isReadOnly()) {
            redo ? lineEdit->redo() : lineEdit->undo();
            return;
        }
        if (auto* textEdit = qobject_cast<QTextEdit*>(focus); textEdit && !textEdit->isReadOnly()) {
            redo ? textEdit->redo() : textEdit->undo();
            return;
        }
        if (!m_editPage || !hasActiveProject() || m_currentPage != AppPage::Edit) {
            return;
        }
        redo ? m_editPage->redo() : m_editPage->undo();
    };
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WindowShortcut);
    connect(undoShortcut, &QShortcut::activated, this,
            [dispatchHistory]() { dispatchHistory(false); });
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WindowShortcut);
    connect(redoShortcut, &QShortcut::activated, this,
            [dispatchHistory]() { dispatchHistory(true); });
    auto* missionShortcut =
        new QShortcut(QKeySequence(QKeyCombination(physicalControlModifier(), Qt::Key_I)), this);
    connect(missionShortcut, &QShortcut::activated, m_editPage,
            &EditPage::toggleMissionControlMode);
    auto* galleryMissionShortcut =
        new QShortcut(QKeySequence(QKeyCombination(physicalControlModifier(), Qt::Key_P)), this);
    connect(galleryMissionShortcut, &QShortcut::activated, this, [this]() {
        if (m_editPage) {
            m_editPage->toggleGalleryMissionControlMode();
        }
    });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), m_editPage, SLOT(exportProjectFromDialog()));

    auto* preferencesShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(preferencesShortcut, &QShortcut::activated, this,
            [this]() { switchToPage(AppPage::Preferences); });
    auto* previewShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(previewShortcut, &QShortcut::activated, m_editPage, &EditPage::previewSelectedImage);
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, m_editPage, &EditPage::deleteSelectedImage);
    auto* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escapeShortcut, &QShortcut::activated, this, [this]() {
        if (m_tierFocusMode) {
            setTierFocusMode(false);
        }
    });
}

void RootWidget::updateSelection(AppPage page) {
    const QModelIndex index = m_sidebarModel->indexForPage(page);
    if (index.isValid()) {
        m_sidebarView->setCurrentIndex(index);
    } else {
        m_sidebarView->clearSelection();
    }
}

void RootWidget::updateTitleBarForPage(AppPage page) {
    if (!m_titleBar || m_tierFocusMode) {
        return;
    }

    switch (page) {
    case AppPage::Edit:
        m_titleBar->setEditorActionsVisible(true);
        m_titleBar->setTitleEditable(hasActiveProject());
        setTitleEditorHitTestVisible(hasActiveProject());
        m_titleBar->setDocumentTitle(m_editPage ? m_editPage->displayTitle() : QString());
        break;
    case AppPage::Projects:
        m_titleBar->setEditorActionsVisible(false);
        m_titleBar->setTitleEditable(false);
        setTitleEditorHitTestVisible(false);
        m_titleBar->setDocumentTitle(tr("Projects"));
        break;
    case AppPage::Preferences:
        m_titleBar->setEditorActionsVisible(false);
        m_titleBar->setTitleEditable(false);
        setTitleEditorHitTestVisible(false);
        m_titleBar->setDocumentTitle(tr("Preferences"));
        break;
    }
    updateUnsavedIndicators();
}

void RootWidget::setTitleEditorHitTestVisible(bool visible) {
    if (!m_windowAgent || !m_titleBar || !m_titleBar->titleEditor()) {
        return;
    }
    if (!m_windowAgent->setHitTestVisible(m_titleBar->titleEditor(), visible)) {
        Logger::warn(
            QStringLiteral("ui.window.agent.hittest.title-editor failed visible=%1").arg(visible));
    }
}

void RootWidget::updatePageMargins(AppPage page) {
    if (!m_pages) {
        return;
    }
    if (m_tierFocusMode || page == AppPage::Edit) {
        m_pages->setContentsMargins(0, 0, 0, 0);
        return;
    }
    m_pages->setContentsMargins(0, kTitleBarHeight, 0, 0);
}

bool RootWidget::hasActiveProject() const {
    return m_hasActiveProject;
}

void RootWidget::setActiveProjectAvailable(bool available) {
    if (m_hasActiveProject == available) {
        updateUnsavedIndicators();
        return;
    }

    m_hasActiveProject = available;
    if (m_sidebarModel) {
        m_sidebarModel->setPageEnabled(AppPage::Edit, available);
    }
    if (!available && m_currentPage == AppPage::Edit) {
        switchToPage(AppPage::Projects);
    }
    updateTitleBarForPage(m_currentPage);
    updateUnsavedIndicators();
}

void RootWidget::updateUnsavedIndicators() {
    const bool dirty = hasActiveProject() && m_editPage && m_editPage->isDirty();
    if (m_sidebarModel) {
        m_sidebarModel->setPageDirty(AppPage::Edit, dirty);
    }
    if (m_titleBar) {
        m_titleBar->setUnsavedIndicatorVisible(dirty && m_currentPage == AppPage::Edit);
    }
}

} // namespace qtm
