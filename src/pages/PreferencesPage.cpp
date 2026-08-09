#include "pages/PreferencesPage.h"

#include "platform/Platform.h"
#include "theme/Theme.h"
#include "widgets/SectionHeader.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <functional>

#include <vkui/core/VkIcon.h>
#include <vkui/widgets/VkComboBox.h>
#include <vkui/widgets/controls/VkSlider.h>
#include <vkui/widgets/controls/VkSwitch.h>

#ifndef TLM_APP_VERSION
#define TLM_APP_VERSION "0.2.0"
#endif

#ifndef TLM_GIT_COMMIT
#define TLM_GIT_COMMIT "unknown"
#endif

namespace tlm {

namespace {
constexpr int kUpdateBadgeRole = Qt::UserRole + 41;
constexpr int kGeneralPageIndex = 0;
constexpr int kLiquidGlassPageIndex = 1;
constexpr int kUpdatePageIndex = 2;
constexpr int kAboutPageIndex = 3;

struct ParameterSlider final {
    QWidget* widget{nullptr};
    vkui::VkSlider* slider{nullptr};
    QLabel* valueLabel{nullptr};
    qreal maximum{1.0};
    int steps{1000};
};

template <typename Enum> int enumIndex(Enum value) {
    return static_cast<int>(value);
}

bool indexIsUnderCursor(const QStyleOptionViewItem& option, const QModelIndex& index) {
    const auto* viewport = qobject_cast<const QWidget*>(option.widget);
    const auto* view =
        qobject_cast<const QAbstractItemView*>(viewport ? viewport->parentWidget() : nullptr);
    if (!viewport || !view) {
        return option.state.testFlag(QStyle::State_MouseOver);
    }
    const QPoint localPos = viewport->mapFromGlobal(QCursor::pos());
    return viewport->rect().contains(localPos) && view->indexAt(localPos) == index;
}

class PreferencesNavDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QRect r = option.rect.adjusted(6, 3, -6, -3);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered =
            option.state.testFlag(QStyle::State_MouseOver) && indexIsUnderCursor(option, index);
        const ThemeTokens& colors = activeThemeTokens();
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? colors.selection : colors.controlFillHovered);
            painter->drawRoundedRect(r, 8, 8);
        }

        painter->setPen(colors.primaryText);
        painter->drawText(r.adjusted(14, 0, -30, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          index.data(Qt::DisplayRole).toString());

        if (index.data(kUpdateBadgeRole).toBool()) {
            const int d = 8;
            const QRect badge(r.right() - d - 12, r.center().y() - d / 2, d, d);
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.destructive);
            painter->drawEllipse(badge);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(160, 38);
    }
};

void configureAdaptiveField(QWidget* widget) {
    if (!widget) {
        return;
    }
    if (qobject_cast<vkui::VkSwitch*>(widget)) {
        widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        return;
    }
    if (auto* comboBox = qobject_cast<QComboBox*>(widget)) {
        comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        vkui::setComboBoxElideMode(*comboBox, Qt::ElideRight);
    }
    widget->setMinimumWidth(164);
    widget->setMaximumWidth(260);
    widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void setSettingRowExpanded(QWidget* row, bool expanded, bool animated) {
    if (!row) {
        return;
    }

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(row->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(row);
        row->setGraphicsEffect(effect);
    }

    const int expandedHeight = qMax(1, row->sizeHint().height());
    if (!animated) {
        row->setVisible(expanded);
        row->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : 0);
        effect->setOpacity(expanded ? 1.0 : 0.0);
        return;
    }

    row->setVisible(true);
    const qreal start = effect->opacity();
    const qreal end = expanded ? 1.0 : 0.0;
    auto* animation = new QVariantAnimation(row);
    animation->setDuration(180);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setStartValue(start);
    animation->setEndValue(end);
    QObject::connect(animation, &QVariantAnimation::valueChanged, row,
                     [row, effect, expandedHeight](const QVariant& value) {
                         const qreal progress = value.toReal();
                         effect->setOpacity(progress);
                         row->setMaximumHeight(qMax(0, qRound(expandedHeight * progress)));
                     });
    QObject::connect(animation, &QVariantAnimation::finished, row,
                     [row, effect, expanded, animation]() {
                         effect->setOpacity(expanded ? 1.0 : 0.0);
                         row->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : 0);
                         row->setVisible(expanded);
                         animation->deleteLater();
                     });
    animation->start();
}

QWidget* createSettingRow(const QString& title, QWidget* control, QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto* label = new QLabel(title, row);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setMinimumWidth(120);
    layout->addWidget(label);
    layout->addStretch(1);

    configureAdaptiveField(control);
    layout->addWidget(control, 0, Qt::AlignRight);
    return row;
}

ParameterSlider createParameterSlider(qreal value, qreal maximum, int steps,
                                      const std::function<QString(qreal)>& formatter,
                                      QWidget* parent) {
    auto* field = new QWidget(parent);
    auto* layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* slider = new vkui::VkSlider(Qt::Horizontal, field);
    slider->setRange(0, steps);
    // Keep the public value synchronized with the handle while it is dragged so live
    // Liquid Glass consumers never restart from the last released position.
    slider->setTracking(true);
    slider->setValue(qRound(qBound<qreal>(0.0, value, maximum) / maximum * steps));
    slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* valueLabel = new QLabel(field);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setMinimumWidth(62);
    valueLabel->setText(formatter(value));

    const auto updateLabel = [valueLabel, formatter, maximum, steps](int sliderValue) {
        valueLabel->setText(formatter(maximum * sliderValue / steps));
    };
    QObject::connect(slider, &QSlider::valueChanged, valueLabel, updateLabel);

    layout->addWidget(slider, 1);
    layout->addWidget(valueLabel);
    return {field, slider, valueLabel, maximum, steps};
}

qreal parameterSliderValue(const ParameterSlider& control) {
    return control.maximum * control.slider->value() / control.steps;
}

void setParameterSliderValue(const ParameterSlider& control, qreal value) {
    control.slider->setValue(
        qRound(qBound<qreal>(0.0, value, control.maximum) / control.maximum * control.steps));
}
} // namespace

PreferencesPage::PreferencesPage(AppSettings* settings, LanguageManager* languageManager,
                                 AppUpdater* updater, QWidget* parent)
    : QWidget(parent), m_settings(settings), m_languageManager(languageManager),
      m_updater(updater) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(14);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(18);

    m_nav = new QListWidget(this);
    m_nav->addItems({tr("General"), tr("Liquid Glass"), tr("Updates"), tr("About")});
    m_nav->setMinimumWidth(150);
    m_nav->setMaximumWidth(300);
    m_nav->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setMouseTracking(true);
    m_nav->viewport()->setMouseTracking(true);
    m_nav->setCurrentRow(0);
    m_nav->setItemDelegate(new PreferencesNavDelegate(m_nav));
    m_nav->setStyleSheet(
        QStringLiteral("QListWidget{background:transparent;outline:0;border:none;}"));

    m_stack = new QStackedWidget(this);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stack->addWidget(createScrollablePage(createGeneralPage()));
    m_stack->addWidget(createScrollablePage(createLiquidGlassPage()));
    m_stack->addWidget(createScrollablePage(createUpdatePage()));
    m_stack->addWidget(createScrollablePage(createAboutPage()));

    body->addWidget(m_nav);
    body->addWidget(m_stack, 1);
    root->addLayout(body, 1);

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_nav, &QListWidget::currentRowChanged, this,
            [this](int) { m_nav->viewport()->update(); });
    connect(m_nav, &QListWidget::itemPressed, this,
            [this](QListWidgetItem*) { m_nav->viewport()->update(); });
    connect(m_nav, &QListWidget::itemEntered, this,
            [this](QListWidgetItem*) { m_nav->viewport()->update(); });
    if (m_updater) {
        connect(m_updater, &AppUpdater::checkingStarted, this, [this](const QUrl& url) {
            if (m_updateStatusLabel) {
                m_updateStatusLabel->setText(tr("Checking updates from:\n%1").arg(url.toString()));
            }
            if (m_checkUpdateButton) {
                m_checkUpdateButton->setEnabled(false);
            }
            if (m_openUpdateButton) {
                m_openUpdateButton->setEnabled(false);
            }
        });
        connect(m_updater, &AppUpdater::updateAvailable, this, &PreferencesPage::applyUpdateResult);
        connect(m_updater, &AppUpdater::noUpdateAvailable, this,
                &PreferencesPage::applyUpdateResult);
        connect(m_updater, &AppUpdater::updateDetailsChanged, this,
                &PreferencesPage::applyUpdateResult);
        connect(m_updater, &AppUpdater::checkFailed, this, &PreferencesPage::applyUpdateFailure);
        connect(m_updater, &AppUpdater::updateFailed, this, &PreferencesPage::applyUpdateFailure);
        connect(m_updater, &AppUpdater::updateNotificationChanged, this,
                &PreferencesPage::setUpdateNotificationVisible);
        connect(m_updater, &AppUpdater::stateChanged, this,
                [this](UpdateState) { refreshUpdateActions(); });
        connect(
            m_updater, &AppUpdater::downloadProgress, this, [this](qint64 received, qint64 total) {
                if (m_updateStatusLabel && total > 0) {
                    const int percent = qBound(
                        0, qRound(100.0 * static_cast<qreal>(received) / static_cast<qreal>(total)),
                        100);
                    m_updateStatusLabel->setText(
                        tr("Downloading update %1: %2%")
                            .arg(m_updater ? m_updater->lastResult().latestVersion : QString())
                            .arg(percent));
                }
            });
        connect(m_updater, &AppUpdater::updateReady, this, [this](const QString&) {
            if (m_updateStatusLabel && m_updater) {
                m_updateStatusLabel->setText(tr("Update %1 is ready to install.")
                                                 .arg(m_updater->lastResult().latestVersion));
            }
            refreshUpdateActions();
        });
        setUpdateNotificationVisible(m_updater->hasUpdateAvailable());
    }
    if (m_languageManager) {
        connect(m_languageManager, &LanguageManager::languageChanged, this,
                &PreferencesPage::retranslateUi);
    }
    if (m_settings) {
        connect(m_settings, &AppSettings::autoUpdateEnabledChanged, this, [this](bool) {
            setUpdateNotificationVisible(m_updater && m_updater->hasUpdateAvailable());
            refreshUpdatePolicyText();
        });
        connect(m_settings, &AppSettings::previewEffectChanged, this,
                [this](BackdropEffect) { refreshLiquidGlassAvailability(); });
        connect(m_settings, &AppSettings::overviewBackdropEffectChanged, this,
                [this](BackdropEffect) { refreshLiquidGlassAvailability(); });
    }
    refreshLiquidGlassAvailability();
    updateNavWidth();
    QTimer::singleShot(0, this, &PreferencesPage::updateNavWidth);
}

void PreferencesPage::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        refreshPreferenceControlStyles();
    }
}

void PreferencesPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateNavWidth();
}

void PreferencesPage::retranslateUi() {
    if (!m_nav || !m_stack) {
        return;
    }
    int row = qBound(0, m_nav->currentRow(), kAboutPageIndex);
    m_nav->item(kGeneralPageIndex)->setText(tr("General"));
    m_nav->item(kLiquidGlassPageIndex)->setText(tr("Liquid Glass"));
    m_nav->item(kUpdatePageIndex)->setText(tr("Updates"));
    m_nav->item(kAboutPageIndex)->setText(tr("About"));
    rebuildPreferencePages();
    refreshLiquidGlassAvailability();
    if (row == kLiquidGlassPageIndex && m_nav->item(row)->isHidden()) {
        row = kGeneralPageIndex;
    }
    m_nav->setCurrentRow(row);
    m_stack->setCurrentIndex(row);
    updateNavWidth();
}

void PreferencesPage::setUpdateNotificationVisible(bool visible) {
    if (!m_nav || m_nav->count() <= kUpdatePageIndex) {
        return;
    }
    m_nav->item(kUpdatePageIndex)
        ->setData(kUpdateBadgeRole,
                  visible && (!m_settings || m_settings->autoUpdateEnabled()));
    m_nav->viewport()->update();
}

void PreferencesPage::rebuildPreferencePages() {
    if (!m_stack) {
        return;
    }
    while (m_stack->count() > 0) {
        QWidget* page = m_stack->widget(0);
        m_stack->removeWidget(page);
        page->deleteLater();
    }
    m_updateStatusLabel = nullptr;
    m_checkUpdateButton = nullptr;
    m_installUpdateButton = nullptr;
    m_openUpdateButton = nullptr;
    m_stack->addWidget(createScrollablePage(createGeneralPage()));
    m_stack->addWidget(createScrollablePage(createLiquidGlassPage()));
    m_stack->addWidget(createScrollablePage(createUpdatePage()));
    m_stack->addWidget(createScrollablePage(createAboutPage()));
}

QWidget* PreferencesPage::createScrollablePage(QWidget* content) {
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setAutoFillBackground(false);
    scrollArea->viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    scrollArea->setWidget(content);
    return scrollArea;
}

void PreferencesPage::updateNavWidth() {
    if (!m_nav) {
        return;
    }
    QFontMetrics metrics(m_nav->font());
    int width = 150;
    for (int i = 0; i < m_nav->count(); ++i) {
        if (m_nav->item(i)->isHidden()) {
            continue;
        }
        width = qMax(width, metrics.horizontalAdvance(m_nav->item(i)->text()) + 62);
    }
    const int availableWidth = qMax(1, this->width());
    const int maximumNavWidth = qBound(190, availableWidth / 3, 300);
    const int navWidth = qBound(150, width, maximumNavWidth);
    m_nav->setFixedWidth(navWidth);
    if (m_stack) {
        const int contentWidth = qBound(320, availableWidth - navWidth - 86, 760);
        m_stack->setMinimumWidth(contentWidth);
        for (int i = 0; i < m_stack->count(); ++i) {
            if (QWidget* page = m_stack->widget(i)) {
                page->setMinimumWidth(contentWidth);
            }
        }
    }
}

void PreferencesPage::refreshPreferenceControlStyles() {
    update();
}

void PreferencesPage::refreshLiquidGlassAvailability() {
    if (!m_nav || m_nav->count() <= kLiquidGlassPageIndex) {
        return;
    }
    const bool available =
        !m_settings || m_settings->previewEffect() == BackdropEffect::LiquidGlass ||
        m_settings->overviewBackdropEffect() == BackdropEffect::LiquidGlass;
    QListWidgetItem* item = m_nav->item(kLiquidGlassPageIndex);
    item->setHidden(!available);
    if (!available && m_nav->currentRow() == kLiquidGlassPageIndex) {
        m_nav->setCurrentRow(kGeneralPageIndex);
    }
    updateNavWidth();
}

void PreferencesPage::refreshUpdateActions() {
    const UpdateState state = m_updater ? m_updater->state() : UpdateState::Idle;
    if (m_checkUpdateButton) {
        m_checkUpdateButton->setEnabled(state != UpdateState::Checking &&
                                        state != UpdateState::Downloading &&
                                        state != UpdateState::Installing);
    }
    if (m_installUpdateButton) {
        const bool ready = state == UpdateState::Ready;
        const bool available = state == UpdateState::Available;
        m_installUpdateButton->setVisible(m_updater && m_updater->hasUpdateAvailable());
        m_installUpdateButton->setEnabled(ready || available);
        m_installUpdateButton->setText(ready ? tr("Install Update") : tr("Download Update"));
        m_installUpdateButton->setIcon(
            vkui::icon(ready ? vkui::VkSymbol::Checkmark : vkui::VkSymbol::Download,
                       vkui::VkIconRole::Accent));
    }
    if (m_openUpdateButton) {
        m_openUpdateButton->setEnabled(m_updater && m_updater->hasUpdateAvailable());
    }
}

void PreferencesPage::refreshUpdatePolicyText() {
    if (!m_updateStatusLabel || (m_updater && !m_updater->lastResult().latestVersion.isEmpty())) {
        return;
    }
    m_updateStatusLabel->setText(
        m_settings && !m_settings->autoUpdateEnabled()
            ? tr("Automatic update checks are off. You can still check manually; no update is "
                 "downloaded or installed without your action.")
            : tr("Automatic checks run at most once per day. No update is downloaded or installed "
                 "without your action."));
}

void PreferencesPage::applyUpdateResult(const UpdateCheckResult& result) {
    m_lastUpdateResult = result;
    setUpdateNotificationVisible(result.updateAvailable);
    if (m_updateStatusLabel) {
        if (result.updateAvailable) {
            const QString changelog = result.changelog.trimmed();
            m_updateStatusLabel->setText(tr("Version %1 is available. Current version: %2%3")
                                             .arg(result.latestVersion, result.currentVersion,
                                                  changelog.isEmpty()
                                                      ? QString()
                                                      : QStringLiteral("\n\n%1").arg(changelog)));
        } else {
            m_updateStatusLabel->setText(
                tr("TierListMaker is up to date. Current version: %1").arg(result.currentVersion));
        }
    }
    refreshUpdateActions();
}

void PreferencesPage::applyUpdateFailure(const QString& reason) {
    if (m_updateStatusLabel) {
        m_updateStatusLabel->setText(
            tr("Update check failed: %1\nThe project may not have published update metadata yet.")
                .arg(reason));
    }
    refreshUpdateActions();
}

QWidget* PreferencesPage::createGeneralPage() {
    auto* page = new QWidget(this);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 0, 0, 0);
    layout->setSpacing(16);

    auto* settingsLayout = new QVBoxLayout;
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(12);

    auto* language = new QComboBox(page);
    language->addItem(tr("System"), QStringLiteral("system"));
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    language->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    language->setCurrentIndex(qMax(0, language->findData(m_settings->language())));
    connect(language, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, language](int index) {
                const QString code = language->itemData(index).toString();
                m_settings->setLanguage(code);
                if (m_languageManager) {
                    m_languageManager->setLanguage(code);
                }
            });
    settingsLayout->addWidget(createSettingRow(tr("Language"), language, page));

    auto* appearance = new QComboBox(page);
    appearance->addItems({tr("System"), tr("Light"), tr("Dark")});
    appearance->setCurrentIndex(enumIndex(m_settings->appearance()));
    connect(appearance, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { m_settings->setAppearance(static_cast<AppearanceMode>(index)); });
    settingsLayout->addWidget(createSettingRow(tr("Appearance"), appearance, page));

    const auto createBackdropEffectCombo = [this, page](BackdropEffect effect) {
        auto* combo = new QComboBox(page);
        combo->addItem(tr("Depth Soft Focus"),
                       static_cast<int>(BackdropEffect::DepthSoftFocus));
        combo->addItem(tr("Liquid Glass"), static_cast<int>(BackdropEffect::LiquidGlass));
        combo->setCurrentIndex(qMax(0, combo->findData(static_cast<int>(effect))));
        return combo;
    };

    auto* previewEffect = createBackdropEffectCombo(m_settings->previewEffect());
    settingsLayout->addWidget(createSettingRow(tr("Preview effect"), previewEffect, page));

    auto* previewBackground = new QComboBox(page);
    previewBackground->addItem(tr("None"), static_cast<int>(PreviewBackgroundMode::None));
    previewBackground->addItem(tr("Image"), static_cast<int>(PreviewBackgroundMode::SelfImage));
    previewBackground->setCurrentIndex(qMax(
        0, previewBackground->findData(static_cast<int>(m_settings->previewBackgroundMode()))));
    QWidget* previewBackgroundRow =
        createSettingRow(tr("Preview background"), previewBackground, page);
    settingsLayout->addWidget(previewBackgroundRow);
    setSettingRowExpanded(previewBackgroundRow,
                          m_settings->previewEffect() == BackdropEffect::DepthSoftFocus, false);
    connect(previewBackground, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, previewBackground](int index) {
                m_settings->setPreviewBackgroundMode(static_cast<PreviewBackgroundMode>(
                    previewBackground->itemData(index).toInt()));
            });
    connect(previewEffect, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, previewEffect, previewBackgroundRow](int index) {
                const auto effect =
                    static_cast<BackdropEffect>(previewEffect->itemData(index).toInt());
                m_settings->setPreviewEffect(effect);
                setSettingRowExpanded(previewBackgroundRow,
                                      effect == BackdropEffect::DepthSoftFocus, true);
            });

    auto* overviewBackdrop = createBackdropEffectCombo(m_settings->overviewBackdropEffect());
    connect(overviewBackdrop, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, overviewBackdrop](int index) {
                m_settings->setOverviewBackdropEffect(static_cast<BackdropEffect>(
                    overviewBackdrop->itemData(index).toInt()));
            });
    settingsLayout->addWidget(
        createSettingRow(tr("Overview background material"), overviewBackdrop, page));

    const auto addBlankAreaActionItems = [](QComboBox* combo) {
        combo->addItem(tr("Open Gallery Overview"),
                       static_cast<int>(BlankAreaAction::GalleryMissionControl));
        combo->addItem(tr("Open Tier Overview"),
                       static_cast<int>(BlankAreaAction::TierMissionControl));
        combo->addItem(tr("Do Nothing"), static_cast<int>(BlankAreaAction::None));
    };
    const auto setBlankAreaActionIndex = [](QComboBox* combo, BlankAreaAction action) {
        const int index = combo->findData(static_cast<int>(action));
        combo->setCurrentIndex(qMax(0, index));
    };

    auto* blankDoubleClick = new QComboBox(page);
    addBlankAreaActionItems(blankDoubleClick);
    setBlankAreaActionIndex(blankDoubleClick, m_settings->blankDoubleClickAction());
    connect(blankDoubleClick, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, blankDoubleClick](int index) {
                m_settings->setBlankDoubleClickAction(
                    static_cast<BlankAreaAction>(blankDoubleClick->itemData(index).toInt()));
            });
    settingsLayout->addWidget(
        createSettingRow(tr("Empty tier area double-click"), blankDoubleClick, page));

    auto* blankLongPress = new QComboBox(page);
    addBlankAreaActionItems(blankLongPress);
    setBlankAreaActionIndex(blankLongPress, m_settings->blankLongPressAction());
    connect(blankLongPress, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, blankLongPress](int index) {
                m_settings->setBlankLongPressAction(
                    static_cast<BlankAreaAction>(blankLongPress->itemData(index).toInt()));
            });
    settingsLayout->addWidget(
        createSettingRow(tr("Empty tier area long press"), blankLongPress, page));

    auto* tierListToolTips = new vkui::VkSwitch(page);
    tierListToolTips->setAccessibleName(tr("Show tier list tooltips"));
    tierListToolTips->setChecked(m_settings->tierListToolTipsEnabled());
    connect(tierListToolTips, &QAbstractButton::toggled, m_settings,
            &AppSettings::setTierListToolTipsEnabled);
    settingsLayout->addWidget(createSettingRow(tr("Tier list tooltips"), tierListToolTips, page));

    auto* autosave = new vkui::VkSwitch(page);
    autosave->setAccessibleName(tr("Enable autosave"));
    autosave->setChecked(m_settings->autosaveEnabled());
    settingsLayout->addWidget(createSettingRow(tr("Save behavior"), autosave, page));

    auto* autosaveInterval = new QSpinBox(page);
    autosaveInterval->setRange(1, 60);
    autosaveInterval->setSuffix(tr(" min"));
    autosaveInterval->setValue(m_settings->autosaveIntervalMinutes());
    connect(autosaveInterval, QOverload<int>::of(&QSpinBox::valueChanged), m_settings,
            &AppSettings::setAutosaveIntervalMinutes);
    QWidget* autosaveIntervalRow =
        createSettingRow(tr("Autosave interval"), autosaveInterval, page);
    settingsLayout->addWidget(autosaveIntervalRow);
    setSettingRowExpanded(autosaveIntervalRow, autosave->isChecked(), false);
    connect(autosave, &QAbstractButton::toggled, this, [this, autosaveIntervalRow](bool enabled) {
        m_settings->setAutosaveEnabled(enabled);
        setSettingRowExpanded(autosaveIntervalRow, enabled, true);
    });

    auto* projectFolderRow = new QWidget(page);
    auto* projectFolderLayout = new QVBoxLayout(projectFolderRow);
    projectFolderLayout->setContentsMargins(0, 0, 0, 0);
    projectFolderLayout->setSpacing(6);
    auto* projectFolderHeader = new QHBoxLayout;
    projectFolderHeader->setContentsMargins(0, 0, 0, 0);
    projectFolderHeader->setSpacing(18);
    auto* projectFolderLabel = new QLabel(tr("Default project folder"), projectFolderRow);
    projectFolderLabel->setMinimumWidth(120);
    auto* chooseProjectFolder =
        new QPushButton(vkui::icon(vkui::VkSymbol::Folder), tr("Choose"), projectFolderRow);
    chooseProjectFolder->setAutoDefault(false);
    chooseProjectFolder->setDefault(false);
    projectFolderHeader->addWidget(projectFolderLabel);
    projectFolderHeader->addStretch(1);
    projectFolderHeader->addWidget(chooseProjectFolder);
    projectFolderLayout->addLayout(projectFolderHeader);

    auto* projectFolderPath = new QLabel(projectFolderRow);
    projectFolderPath->setObjectName(QStringLiteral("DefaultProjectFolderPath"));
    projectFolderPath->setWordWrap(true);
    projectFolderPath->setTextFormat(Qt::PlainText);
    projectFolderPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    projectFolderPath->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    projectFolderPath->setStyleSheet(
        QStringLiteral("QLabel#DefaultProjectFolderPath{color:palette(mid);}"));
    const auto updateProjectFolderPath = [this, projectFolderPath]() {
        const QString path = QDir::toNativeSeparators(m_settings->defaultProjectDirectory());
        projectFolderPath->setText(path);
        projectFolderPath->setToolTip(path);
    };
    updateProjectFolderPath();
    connect(m_settings, &AppSettings::changed, projectFolderPath, updateProjectFolderPath);
    projectFolderLayout->addWidget(projectFolderPath);

    connect(chooseProjectFolder, &QPushButton::clicked, this, [this, updateProjectFolderPath]() {
        const QString directory = QFileDialog::getExistingDirectory(
            this, tr("Default Project Folder"), m_settings->defaultProjectDirectory(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!directory.isEmpty()) {
            m_settings->setDefaultProjectDirectory(directory);
            updateProjectFolderPath();
        }
    });
    settingsLayout->addWidget(projectFolderRow);

    auto* autoUpdate = new vkui::VkSwitch(page);
    autoUpdate->setAccessibleName(tr("Enable automatic update checks"));
    autoUpdate->setChecked(m_settings->autoUpdateEnabled());
    connect(autoUpdate, &QAbstractButton::toggled, this,
            [this](bool enabled) { m_settings->setAutoUpdateEnabled(enabled); });
    settingsLayout->addWidget(createSettingRow(tr("Updates"), autoUpdate, page));

    layout->addWidget(new SectionHeader(tr("General"), page));
    layout->addLayout(settingsLayout);
    layout->addStretch();
    return page;
}

QWidget* PreferencesPage::createLiquidGlassPage() {
    auto* page = new QWidget(this);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 0, 0, 0);
    layout->setSpacing(16);

    auto* settingsLayout = new QVBoxLayout;
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(12);

    const LiquidGlassParameters parameters =
        m_settings ? m_settings->liquidGlassParameters() : LiquidGlassParameters{};
    const auto fractionFormatter = [](qreal value) {
        return QStringLiteral("%1%").arg(value * 100.0, 0, 'f', 1);
    };
    const auto pixelFormatter = [this](qreal value) {
        return tr("%1 px").arg(value, 0, 'f', 1);
    };

    const ParameterSlider cornerRadius =
        createParameterSlider(parameters.cornerRadius, 128.0, 1280, pixelFormatter, page);
    const ParameterSlider blurRadius =
        createParameterSlider(parameters.blurRadius, 32.0, 3200, pixelFormatter, page);
    const ParameterSlider refractionHeight = createParameterSlider(
        parameters.refractionHeightFraction, 1.0, 1000, fractionFormatter, page);
    const ParameterSlider refractionAmount = createParameterSlider(
        parameters.refractionAmountFraction, 1.0, 1000, fractionFormatter, page);
    const ParameterSlider chromaticAberration = createParameterSlider(
        parameters.chromaticAberration, 1.0, 1000, fractionFormatter, page);

    settingsLayout->addWidget(createSettingRow(tr("Corner radius"), cornerRadius.widget, page));
    settingsLayout->addWidget(createSettingRow(tr("Blur radius"), blurRadius.widget, page));
    settingsLayout->addWidget(
        createSettingRow(tr("Refraction height"), refractionHeight.widget, page));
    settingsLayout->addWidget(
        createSettingRow(tr("Refraction amount"), refractionAmount.widget, page));
    settingsLayout->addWidget(
        createSettingRow(tr("Chromatic aberration"), chromaticAberration.widget, page));

    const auto bindParameter = [this](const ParameterSlider& control,
                                      qreal LiquidGlassParameters::*member) {
        connect(control.slider, &QSlider::valueChanged, this, [this, control, member]() {
            if (!m_settings) {
                return;
            }
            LiquidGlassParameters updated = m_settings->liquidGlassParameters();
            updated.*member = parameterSliderValue(control);
            if (control.slider->isSliderDown()) {
                m_settings->previewLiquidGlassParameters(updated);
            } else {
                m_settings->setLiquidGlassParameters(updated);
            }
        });
        connect(control.slider, &QSlider::sliderReleased, this, [this]() {
            if (m_settings) {
                m_settings->setLiquidGlassParameters(m_settings->liquidGlassParameters());
            }
        });
    };
    bindParameter(cornerRadius, &LiquidGlassParameters::cornerRadius);
    bindParameter(blurRadius, &LiquidGlassParameters::blurRadius);
    bindParameter(refractionHeight, &LiquidGlassParameters::refractionHeightFraction);
    bindParameter(refractionAmount, &LiquidGlassParameters::refractionAmountFraction);
    bindParameter(chromaticAberration, &LiquidGlassParameters::chromaticAberration);

    if (m_settings) {
        connect(m_settings, &AppSettings::liquidGlassParametersChanged, page,
                [this, cornerRadius, blurRadius, refractionHeight, refractionAmount,
                 chromaticAberration]() {
                    const LiquidGlassParameters current = m_settings->liquidGlassParameters();
                    setParameterSliderValue(cornerRadius, current.cornerRadius);
                    setParameterSliderValue(blurRadius, current.blurRadius);
                    setParameterSliderValue(refractionHeight,
                                            current.refractionHeightFraction);
                    setParameterSliderValue(refractionAmount,
                                            current.refractionAmountFraction);
                    setParameterSliderValue(chromaticAberration,
                                            current.chromaticAberration);
                });
    }

    auto* reset =
        new QPushButton(vkui::icon(vkui::VkSymbol::Reset), tr("Reset"), page);
    reset->setAutoDefault(false);
    reset->setDefault(false);
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (m_settings) {
            m_settings->setLiquidGlassParameters(LiquidGlassParameters{});
        }
    });

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 4, 0, 0);
    actions->addStretch(1);
    actions->addWidget(reset);

    layout->addWidget(new SectionHeader(tr("Liquid Glass"), page));
    layout->addLayout(settingsLayout);
    layout->addLayout(actions);
    layout->addStretch();
    return page;
}

QWidget* PreferencesPage::createUpdatePage() {
    auto* page = new QWidget(this);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(new SectionHeader(tr("Updates"), page));
    auto* endpoint = new QLabel(
        tr("Update definition:\n%1").arg(AppUpdater::defaultUpdateDefinitionUrl().toString()),
        page);
    endpoint->setWordWrap(true);
    endpoint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(endpoint);

    m_updateStatusLabel = new QLabel(page);
    refreshUpdatePolicyText();
    m_updateStatusLabel->setWordWrap(true);
    m_updateStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_updateStatusLabel);

    auto* actions = new QHBoxLayout;
    m_checkUpdateButton =
        new QPushButton(vkui::icon(vkui::VkSymbol::Download), tr("Check Now"), page);
    m_installUpdateButton =
        new QPushButton(vkui::icon(vkui::VkSymbol::Download, vkui::VkIconRole::Accent),
                        tr("Download Update"), page);
    m_openUpdateButton =
        new QPushButton(vkui::icon(vkui::VkSymbol::Share), tr("Open Release Page"), page);
    actions->addWidget(m_checkUpdateButton);
    actions->addWidget(m_installUpdateButton);
    actions->addWidget(m_openUpdateButton);
    actions->addStretch();
    layout->addLayout(actions);

    connect(m_checkUpdateButton, &QPushButton::clicked, this, [this]() {
        if (m_updater) {
            m_updater->checkForUpdates();
        }
    });
    connect(m_installUpdateButton, &QPushButton::clicked, this, [this]() {
        if (m_updater) {
            m_updater->startUpdate();
        }
    });
    connect(m_openUpdateButton, &QPushButton::clicked, this, [this]() {
        if (m_updater) {
            m_updater->openUpdatePage(m_lastUpdateResult);
        }
    });

    if (m_updater && m_updater->lastResult().latestVersion.size() > 0) {
        applyUpdateResult(m_updater->lastResult());
    }
    refreshUpdateActions();

    layout->addStretch();
    return page;
}

QWidget* PreferencesPage::createAboutPage() {
    auto* page = new QWidget(this);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(new SectionHeader(tr("About"), page));
    auto* text =
        new QLabel(tr("TierListMaker\nVersion: %1\nLicense: MIT\nQt: %2\nQWindowKit: 1.5.1\n"
                      "VkUI: 0.1.0\nBuild: %3\nGit: "
                      "%4\nPlatform: %5\nCopyright: 2026 TierListMaker "
                      "contributors\n\nLinks:\nhttps://github.com/"
                      "qianvk/TierListMaker")
                       .arg(QStringLiteral(TLM_APP_VERSION), QString::fromLatin1(qVersion()),
#ifdef NDEBUG
                            QStringLiteral("Release"),
#else
                            QStringLiteral("Debug"),
#endif
                            QStringLiteral(TLM_GIT_COMMIT), platform::platformName()),
                   page);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setWordWrap(true);
    layout->addWidget(text);
    layout->addStretch();
    return page;
}

} // namespace tlm
