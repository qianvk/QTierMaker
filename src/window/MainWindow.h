#pragma once

#include <QMainWindow>

namespace vkui {
class VkWindowAgent;
}

namespace qtm {

class AssetManager;
class AppSettings;
class LanguageManager;
class ProjectRepository;
class RecentProjectsStore;
class RootWidget;
class ThemeManager;
class ThumbnailCache;
class AppUpdater;

/** Top-level VkUI window host with native platform move, resize, and system buttons. */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ProjectRepository* repository, RecentProjectsStore* recentProjects,
               AssetManager* assetManager, ThumbnailCache* thumbnailCache, AppSettings* settings,
               LanguageManager* languageManager, AppUpdater* updater, QWidget* parent = nullptr);

    vkui::VkWindowAgent* windowAgent() const {
        return m_windowAgent;
    }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    vkui::VkWindowAgent* m_windowAgent{nullptr};
    RootWidget* m_root{nullptr};
};

} // namespace qtm
