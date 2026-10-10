#pragma once

#include <QMainWindow>

#include <vkui/widgets/panels/VPanelManager.h>
#include <vkui/window/VWindowAgent.h>

namespace qtm {

class AssetManager;
class AppSettings;
class LanguageManager;
class ProjectRepository;
class RecentProjectsStore;
class RootWidget;
class ThumbnailCache;
class AppUpdater;

/** Top-level VkUI window host with native platform move, resize, and system buttons. */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ProjectRepository* repository, RecentProjectsStore* recentProjects,
               AssetManager* assetManager, ThumbnailCache* thumbnailCache, AppSettings* settings,
               LanguageManager* languageManager, AppUpdater* updater, QWidget* parent = nullptr);

    vkui::VWindowAgent* windowAgent() { return &m_windowAgent; }
    const vkui::VWindowAgent* windowAgent() const { return &m_windowAgent; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    RootWidget* m_root{nullptr};
    vkui::VPanelManager m_panelManager;
    // Declared last so native teardown precedes QMainWindow base destruction.
    vkui::VWindowAgent m_windowAgent;
};

} // namespace qtm
