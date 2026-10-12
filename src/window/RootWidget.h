#pragma once

#include "navigation/SidebarModel.h"

#include <QPoint>
#include <QPointer>
#include <QWidget>

#include <vkui/window/VWindowAgent.h>

class QFrame;
class QDialog;
class QResizeEvent;
class QShowEvent;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace vkui {
class VPanelManager;
class VSplitter;
}

namespace qtm {

class AppSettings;
class AppTitleBar;
class AssetManager;
class EditPage;
class LanguageManager;
class PreviewOverlay;
class ProjectRepository;
class ProjectsPage;
class RecentProjectsStore;
class SidebarView;
class ThumbnailCache;
class AppUpdater;
class UpdateButton;

/** Root content widget registered with VkUI windowing and responsible for page routing. */
class RootWidget : public QWidget {
    Q_OBJECT

public:
    RootWidget(ProjectRepository* repository, RecentProjectsStore* recentProjects,
               AssetManager* assetManager, ThumbnailCache* thumbnailCache, AppSettings* settings,
               LanguageManager* languageManager, AppUpdater* updater,
               vkui::VPanelManager& panelManager, QWidget* parent = nullptr);

    AppTitleBar* titleBar() const {
        return m_titleBar;
    }
    bool confirmClose();
    void installWindowAgent(vkui::VWindowAgent* agent);

public slots:
    void switchToPage(AppPage page);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void buildUi(ProjectRepository* repository, RecentProjectsStore* recentProjects,
                 AssetManager* assetManager, ThumbnailCache* thumbnailCache, AppSettings* settings,
                 LanguageManager* languageManager, AppUpdater* updater);
    void setupShortcuts();
    void updateSelection(AppPage page);
    void updateTitleBarForPage(AppPage page);
    void updatePageMargins(AppPage page);
    void setTitleEditorHitTestVisible(bool visible);
    QFrame* createSidebar(QWidget* parent);
    QFrame* createContent(ProjectRepository* repository, RecentProjectsStore* recentProjects,
                          AssetManager* assetManager, ThumbnailCache* thumbnailCache,
                          AppSettings* settings);
    void resizeExpandedSidebar(int width);
    void synchronizeInitialLayout();
    [[nodiscard]] int sidebarPanelExtent() const;
    void synchronizeSidebarGeometry(bool force = false);
    void syncSidebarPresentation(int sidebarWidth);
    void layoutTitleBars();
    void layoutSidebarTitleButton();
    void updateTitleBarLeadingReservation();
    void refreshUpdateButton();
    int minimumSidebarButtonX() const;
    void setPanelControlsVisible(bool visible);
    void setTierFocusMode(bool enabled);
    void showPreferencesDialog();
    int stackIndexForPage(AppPage page) const;
    bool hasActiveProject() const;
    void setActiveProjectAvailable(bool available);
    void updateUnsavedIndicators();

    vkui::VPanelManager& m_panelManager;
    AppTitleBar* m_titleBar{nullptr};
    QPointer<vkui::VWindowAgent> m_windowAgent;
    vkui::VSplitter* m_splitter{nullptr};
    QPointer<QWidget> m_sidebarSlot;
    QFrame* m_sidebar{nullptr};
    QFrame* m_content{nullptr};
    QWidget* m_sidebarTitleBar{nullptr};
    QVBoxLayout* m_sidebarLayout{nullptr};
    SidebarModel* m_sidebarModel{nullptr};
    SidebarView* m_sidebarView{nullptr};
    QToolButton* m_newProjectButton{nullptr};
    QToolButton* m_preferencesButton{nullptr};
    UpdateButton* m_updateButton{nullptr};
    QStackedWidget* m_pages{nullptr};
    EditPage* m_editPage{nullptr};
    ProjectsPage* m_projectsPage{nullptr};
    PreviewOverlay* m_previewOverlay{nullptr};
    QPointer<QDialog> m_preferencesDialog;
    AppSettings* m_settings{nullptr};
    LanguageManager* m_languageManager{nullptr};
    AppUpdater* m_updater{nullptr};
    AppPage m_currentPage{AppPage::Projects};
    bool m_hasActiveProject{false};
    bool m_tierFocusMode{false};
    bool m_savedSystemButtonsVisible{true};
    bool m_previewSavedSystemButtonsVisible{true};
    bool m_previewSystemButtonStateCaptured{false};
    int m_currentSidebarWidth{0};
    bool m_focusSavedNavigationExpanded{true};
    bool m_focusSavedContentExpanded{true};
    bool m_focusSavedNavigationSlotHidden{false};
    bool m_initialLayoutSynced{false};
    QPointer<QWidget> m_focusMoveWindow;
    QPoint m_focusMoveOffset;
};

} // namespace qtm
