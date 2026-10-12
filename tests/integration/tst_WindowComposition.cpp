#include "assets/AssetManager.h"
#include "assets/ThumbnailCache.h"
#include "i18n/LanguageManager.h"
#include "pages/EditPage.h"
#include "pages/ProjectsPage.h"
#include "persistence/ProjectRepository.h"
#include "persistence/RecentProjectsStore.h"
#include "settings/AppSettings.h"
#include "tier/TierBoardWidget.h"
#include "tier/TierListModel.h"
#include "tier/TierListView.h"
#include "update/AppUpdater.h"
#include "window/AppTitleBar.h"
#include "window/MainWindow.h"
#include "window/RootWidget.h"

#include <QAbstractItemDelegate>
#include <QFrame>
#include <QListView>
#include <QSplitterHandle>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <vkui/Widgets.h>
#include <vkui/core/VkThemeManager.h>
#include <vkui/widgets/controls/VSplitter.h>
#include <vkui/widgets/panels/VPanelManager.h>
#if defined(Q_OS_WIN)
#include <dwmapi.h>
#include <qt_windows.h>
#endif

class WindowCompositionTest final : public QObject {
    Q_OBJECT
private slots:
    void projectRowsFitThreeTextLines() {
        QTemporaryDir storage;
        qtm::ProjectRepository repository;
        qtm::RecentProjectsStore recent(storage.filePath("recent.json"));
        qtm::ThumbnailCache thumbnails;
        qtm::AppSettings settings;
        auto project = qtm::TierProject::createUntitled();
        project.name = QStringLiteral("Project with a full path");
        project.filePath = storage.filePath("gjpqy-project.qtm");
        QVERIFY(recent.addOrUpdate(project));
        qtm::ProjectsPage page(&repository, &recent, &thumbnails, &settings);
        auto* view = page.findChild<QListView*>(QStringLiteral("ProjectsList"));
        QVERIFY(view);
        QCOMPARE(view->model()->rowCount(), 1);
        int previousHeight = 0;
        for (int points : {9, 11, 16, 24}) {
            QStyleOptionViewItem option;
            option.font = view->font();
            option.font.setPointSize(points);
            const int height =
                view->itemDelegate()->sizeHint(option, view->model()->index(0, 0)).height();
            // Three complete lines plus space separating text from the card edges.
            QVERIFY2(height >= 3 * QFontMetrics(option.font).height() + 24,
                     qPrintable(
                         QStringLiteral("%1 pt row only reserves %2 px").arg(points).arg(height)));
            QVERIFY(height >= previousHeight);
            previousHeight = height;
        }
    }
    void fillsWindowAndRestoresSplitter_data() {
        QTest::addColumn<bool>("navigationExpanded");
        QTest::addColumn<bool>("maximized");
        for (bool navigation : {false, true}) {
            for (bool maximized : {false, true}) {
                const QByteArray name = QStringLiteral("navigation-%1-%2")
                                            .arg(navigation ? "expanded" : "collapsed",
                                                 maximized ? "maximized" : "normal")
                                            .toLatin1();
                QTest::newRow(name.constData()) << navigation << maximized;
            }
        }
    }
    void fillsWindowAndRestoresSplitter() {
        QFETCH(bool, navigationExpanded);
        QFETCH(bool, maximized);
        QStandardPaths::setTestModeEnabled(true);
        vkui::installVkUi(*qApp);
        QTemporaryDir storage;
        qtm::ProjectRepository repository;
        qtm::RecentProjectsStore recent(storage.filePath("recent.json"));
        qtm::AssetManager assets;
        qtm::ThumbnailCache thumbnails;
        qtm::AppSettings settings;
        qtm::LanguageManager language(qApp);
        qtm::AppUpdater updater;
        qtm::MainWindow window(&repository, &recent, &assets, &thumbnails, &settings, &language,
                               &updater);
        if (maximized)
            window.showMaximized();
        else
            window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* root = qobject_cast<qtm::RootWidget*>(window.centralWidget());
        QVERIFY(root);
        auto* splitter = root->findChild<vkui::VSplitter*>(QStringLiteral("MainSplitter"));
        auto* content = root->findChild<QFrame*>(QStringLiteral("Content"));
        QVERIFY(splitter);
        QVERIFY(content);
        auto* edit = root->findChild<qtm::EditPage*>();
        auto* panels = window.findChild<vkui::VPanelManager*>(QStringLiteral("MainPanelManager"));
        QVERIFY(edit);
        QVERIFY(panels);
        auto project = qtm::TierProject::createUntitled();
        const QString projectPath = storage.filePath("focus.qtm");
        QVERIFY(repository.saveProject(project, projectPath));
        auto* projectsPage = root->findChild<qtm::ProjectsPage*>();
        QVERIFY(projectsPage);
        projectsPage->openProjectRequested(projectPath);
        QCOMPARE(edit->currentProjectPath(), projectPath);
        QVERIFY(edit->isVisible());
        auto* view = edit->findChild<qtm::TierListView*>();
        QVERIFY(view);
        QVERIFY(panels->setPanelExpanded(QStringLiteral("navigation"), navigationExpanded));
        QTRY_VERIFY(navigationExpanded ? splitter->sizes().first() > 0
                                       : splitter->sizes().first() == 0);
#if defined(Q_OS_WIN)
        COLORREF captionColor{};
        const auto hwnd = reinterpret_cast<HWND>(window.winId());
        const bool captionColorSupported = SUCCEEDED(::DwmGetWindowAttribute(
            hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor)));
        const auto captionMatches = [&] {
            ::DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
            const QColor background = content->palette().color(content->backgroundRole());
            return captionColor == RGB(background.red(), background.green(), background.blue());
        };
        if (captionColorSupported) {
            QVERIFY(captionMatches());
            for (auto appearance :
                 {vkui::VkAppearance::Dark, vkui::VkAppearance::Light, vkui::VkAppearance::Auto}) {
                vkui::VkThemeManager::instance()->setAppearance(appearance);
                QTRY_VERIFY(captionMatches());
            }
        }
#endif
        const int normalHandleWidth = splitter->handleWidth();
        QVERIFY(normalHandleWidth > 0);
        for (int cycle = 0; cycle < 2; ++cycle) {
            root->titleBar()->tierFocusModeRequested();
            QCOMPARE(splitter->handleWidth(), normalHandleWidth);
            QVERIFY(splitter->widget(0)->isHidden());
            QTRY_COMPARE(content->mapTo(root, QPoint()).x(), 0);
            QTRY_COMPARE(content->width(), root->width());
            QVERIFY(!window.windowAgent()->systemButtonsVisible());
#if defined(Q_OS_WIN)
            RECT visibleFrame{};
            POINT clientOrigin{};
            ::ClientToScreen(hwnd, &clientOrigin);
            if (SUCCEEDED(::DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visibleFrame,
                                                  sizeof(visibleFrame)))) {
                UINT visibleBorder = qCeil(window.devicePixelRatioF());
                ::DwmGetWindowAttribute(hwnd, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &visibleBorder,
                                        sizeof(visibleBorder));
                QVERIFY2(qAbs(clientOrigin.x - visibleFrame.left) <=
                             static_cast<int>(visibleBorder),
                         "Focus Mode exposes the normally invisible native resize border");
            }
#endif
            if (!maximized) {
                window.resize(window.width() + 20, window.height());
            }
            QTRY_COMPARE(content->width(), root->width());
            // Qt recalculates handle visibility on layout/style changes even when
            // its requested width is zero. No handle may paint over Focus content.
            QEvent layoutRequest(QEvent::LayoutRequest);
            QApplication::sendEvent(splitter, &layoutRequest);
            QCoreApplication::processEvents();
            QVERIFY2(!splitter->handle(1)->isVisible(),
                     "A splitter handle overlays the left edge of Focus Mode");
            QEvent styleChange(QEvent::StyleChange);
            QApplication::sendEvent(splitter, &styleChange);
            QFont font = window.font();
            font.setPointSize(11 + cycle * 2);
            window.setFont(font);
            vkui::VkThemeManager::instance()->setAppearance(cycle ? vkui::VkAppearance::Light
                                                                  : vkui::VkAppearance::Dark);
            QCoreApplication::processEvents();
            QVERIFY(!splitter->handle(1)->isVisible());
            QTRY_COMPARE(view->viewport()->mapTo(root, QPoint()), QPoint());
            QTRY_COMPARE(view->viewport()->size(), root->size());
            QCOMPARE(root->childAt(QPoint(1, root->height() / 2)), view->viewport());
            // Geometry alone cannot detect a subpixel inset in the row painter.
            // Sample the actual rendered left edge away from rounded corners and text.
            const QImage rendered = view->viewport()->grab().toImage();
            const qreal dpr = rendered.devicePixelRatio();
            for (int row = 0; row < view->model()->rowCount(); ++row) {
                const QModelIndex index = view->model()->index(row, 0);
                const QRect rowRect = view->visualRect(index);
                const int y = qFloor(rowRect.center().y() * dpr);
                const QColor color = index.data(qtm::TierListModel::ColorRole).value<QColor>();
                for (int x = 0; x < qCeil(4 * dpr); ++x) {
                    QCOMPARE(rendered.pixelColor(x, y), color);
                }
            }
            root->titleBar()->tierFocusModeRequested();
            QCOMPARE(splitter->handleWidth(), normalHandleWidth);
            QCOMPARE(panels->isPanelExpanded(QStringLiteral("navigation")), navigationExpanded);
            QVERIFY(!splitter->widget(0)->isHidden());
            QTRY_VERIFY(navigationExpanded ? splitter->sizes().first() > 0
                                           : splitter->sizes().first() == 0);
            QVERIFY(window.windowAgent()->systemButtonsVisible());
#if defined(Q_OS_WIN)
            if (captionColorSupported) {
                QTRY_VERIFY(captionMatches());
            }
#endif
        }
        vkui::VkThemeManager::instance()->setAppearance(vkui::VkAppearance::Auto);
    }
};

QTEST_MAIN(WindowCompositionTest)
#include "tst_WindowComposition.moc"
