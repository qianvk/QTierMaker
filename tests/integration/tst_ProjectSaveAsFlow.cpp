#include "assets/AssetManager.h"
#include "persistence/ProjectFileLayout.h"
#include "persistence/ProjectRepository.h"
#include "persistence/ProjectStorage.h"
#include "persistence/RecentProjectsStore.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace qtm;

class tst_ProjectSaveAsFlow : public QObject {
    Q_OBJECT

private slots:
    void unsavedImportMigratesAssetsBeforeSave() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString source = dir.filePath(QStringLiteral("source.png"));
        QImage image(48, 48, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(source));

        TierProject project = TierProject::createUntitled();
        AssetManager assets;
        auto imported = assets.importImages(project, {source});
        QVERIFY(imported);
        QVERIFY(QFileInfo(project.images.first().importedAssetPath).isAbsolute());

        const QString projectPath = dir.filePath(QStringLiteral("saved.qtm"));
        project.filePath = QFileInfo(projectPath).absoluteFilePath();
        QVERIFY(assets.migrateSessionAssets(project, project.filePath));
        QVERIFY(!QFileInfo(project.images.first().importedAssetPath).isAbsolute());

        ProjectRepository repository;
        QVERIFY(repository.saveProject(project, project.filePath));

        RecentProjectsStore recent(dir.filePath(QStringLiteral("recent.json")));
        QVERIFY(recent.addOrUpdate(project));

        auto opened = repository.openProject(projectPath);
        QVERIFY(opened);
        QCOMPARE(opened.value().images.size(), 1);
        QVERIFY(QFileInfo::exists(
            assets.resolvedImagePath(opened.value(), opened.value().images.first())));
    }

    void existingProjectSaveAsMovesFileAssetsNameAndRecentEntry() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourceImage = dir.filePath(QStringLiteral("source.png"));
        QImage image(48, 48, QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(image.save(sourceImage));

        const QString previousPath =
            ProjectFileLayout::projectFilePath(dir.path(), QStringLiteral("Original"));
        TierProject project = TierProject::createUntitled();
        project.name = QStringLiteral("Original");
        project.filePath = previousPath;

        AssetManager assets;
        auto imported = assets.importImages(project, {sourceImage});
        QVERIFY(imported);
        QCOMPARE(project.images.size(), 1);

        ProjectRepository repository;
        QVERIFY(repository.saveProject(project, previousPath));
        RecentProjectsStore recent(dir.filePath(QStringLiteral("recent.json")));
        QVERIFY(recent.addOrUpdate(project));

        project.name = QStringLiteral("Moved Project");
        project.touch();
        const QString targetPath = ProjectFileLayout::projectFilePath(dir.path(), project.name);
        auto copied = assets.copyProjectAssets(previousPath, targetPath);
        QVERIFY(copied);
        QVERIFY(copied.value());

        project.filePath = targetPath;
        QVERIFY(repository.saveProject(project, targetPath));
        QVERIFY(recent.addOrUpdate(project, previousPath));
        QVERIFY(ProjectStorage::remove(previousPath));

        QVERIFY(!QFileInfo::exists(previousPath));
        QVERIFY(!QFileInfo::exists(QFileInfo(previousPath).absolutePath()));
        QVERIFY(QFileInfo::exists(targetPath));
        auto opened = repository.openProject(targetPath);
        QVERIFY(opened);
        QCOMPARE(opened.value().name, QStringLiteral("Moved Project"));
        QCOMPARE(opened.value().images.size(), 1);
        QVERIFY(QFileInfo::exists(
            assets.resolvedImagePath(opened.value(), opened.value().images.first())));
        QCOMPARE(recent.entries().size(), 1);
        QCOMPARE(QFileInfo(recent.entries().first().filePath).canonicalFilePath(),
                 QFileInfo(targetPath).canonicalFilePath());
    }
};

QTEST_MAIN(tst_ProjectSaveAsFlow)
#include "tst_ProjectSaveAsFlow.moc"
