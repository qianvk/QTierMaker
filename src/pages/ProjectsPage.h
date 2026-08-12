#pragma once

#include "persistence/RecentProjectsStore.h"
#include "persistence/SampleProjectSeeder.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QPoint;
class QPushButton;
class QStackedLayout;
class QToolButton;
class QComboBox;

namespace qtm {

class AppSettings;
class ProjectRepository;
class RecentProjectsModel;
class SampleProjectDownloader;
class TransferProgressWidget;
class WrappingLabel;
class ThumbnailCache;

/** Recent-project browser with search, sorting, missing markers, and file actions. */
class ProjectsPage : public QWidget {
    Q_OBJECT

public:
    ProjectsPage(ProjectRepository* repository, RecentProjectsStore* recentProjects,
                 ThumbnailCache* thumbnailCache, AppSettings* settings, QWidget* parent = nullptr);

public slots:
    void refresh();
    void focusSearch();
    void openProjectFromDialog();
    void retranslateUi();

signals:
    void openProjectRequested(const QString& filePath);
    void projectDeleted(const QString& filePath);
    void projectAboutToBeReplaced(const QString& filePath);

private:
    QString selectedPath() const;
    RecentProjectEntry selectedEntry() const;
    void showProjectContextMenu(const QPoint& point);
    void openSelectedProject();
    void renameSelectedProject();
    void chooseCoverForSelectedProject();
    void revealSelectedProject();
    void removeSelectedRecord();
    void saveSelectedProjectAs();
    void deleteSelectedProject();
    void downloadSampleProject();
    void registerDownloadedSample(const QString& projectPath, SampleProjectSeedStatus status);
    void resetSampleDownloadUi();

    ProjectRepository* m_repository{nullptr};
    RecentProjectsStore* m_recentProjects{nullptr};
    ThumbnailCache* m_thumbnailCache{nullptr};
    AppSettings* m_settings{nullptr};
    SampleProjectDownloader* m_sampleDownloader{nullptr};
    QLineEdit* m_search{nullptr};
    QComboBox* m_sort{nullptr};
    QToolButton* m_openProjectButton{nullptr};
    QListView* m_view{nullptr};
    RecentProjectsModel* m_model{nullptr};
    QStackedLayout* m_contentStack{nullptr};
    QWidget* m_emptyState{nullptr};
    QLabel* m_emptyTitle{nullptr};
    WrappingLabel* m_emptyDescription{nullptr};
    QPushButton* m_downloadSampleButton{nullptr};
    TransferProgressWidget* m_sampleProgress{nullptr};
    QString m_pendingSampleReplacementPath;
    bool m_installingSample{false};
};

} // namespace qtm
