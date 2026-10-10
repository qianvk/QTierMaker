#include "window/MainWindow.h"

#include "window/RootWidget.h"

#include "logging/Logger.h"

#include <QCloseEvent>

#include <vkui/window/VSystemButton.h>

namespace qtm {

MainWindow::MainWindow(ProjectRepository* repository, RecentProjectsStore* recentProjects,
                       AssetManager* assetManager, ThumbnailCache* thumbnailCache,
                       AppSettings* settings, LanguageManager* languageManager, AppUpdater* updater,
                       QWidget* parent)
    : QMainWindow(parent), m_panelManager(*this), m_windowAgent(*this) {
    m_panelManager.setObjectName(QStringLiteral("MainPanelManager"));
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    m_windowAgent.setResizable(true);
    m_windowAgent.setSystemButtons(vkui::VStandardSystemButtons);
    m_windowAgent.setSystemButtonsVisible(true);

    setWindowTitle(QStringLiteral("QTierMaker"));
    resize(1180, 780);
    setMinimumSize(920, 620);

    m_root = new RootWidget(repository, recentProjects, assetManager, thumbnailCache, settings,
                            languageManager, updater, m_panelManager, this);
    setCentralWidget(m_root);
    m_root->installWindowAgent(&m_windowAgent);
    Logger::info(QStringLiteral("ui.window.agent.ready backend=vkui-native titleBars=2"));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_root && !m_root->confirmClose()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

} // namespace qtm
