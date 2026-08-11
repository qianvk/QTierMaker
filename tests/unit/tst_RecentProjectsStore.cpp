#include "persistence/RecentProjectsStore.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace qtm;

class tst_RecentProjectsStore : public QObject {
    Q_OBJECT

private slots:
    void addRenameRemove() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentProjectsStore store(dir.filePath(QStringLiteral("recent.json")));

        TierProject project = TierProject::createUntitled();
        project.filePath = dir.filePath(QStringLiteral("a.qtm"));
        project.name = QStringLiteral("A");
        QVERIFY(store.addOrUpdate(project));
        QCOMPARE(store.entries().size(), 1);

        QVERIFY(store.renameDisplayName(project.filePath, QStringLiteral("Renamed")));
        QCOMPARE(store.entries().first().name, QStringLiteral("Renamed"));

        QVERIFY(store.remove(project.filePath));
        QCOMPARE(store.entries().size(), 0);
    }

    void replacesRenamedPathAndDropsUnsupportedEntries() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentProjectsStore store(dir.filePath(QStringLiteral("recent.json")));

        TierProject project = TierProject::createUntitled();
        const QString previousPath = dir.filePath(QStringLiteral("Old/Old.qtm"));
        project.filePath = previousPath;
        project.name = QStringLiteral("Old");
        QVERIFY(store.addOrUpdate(project));

        project.filePath = dir.filePath(QStringLiteral("New/New.qtm"));
        project.name = QStringLiteral("New");
        QVERIFY(store.addOrUpdate(project, previousPath));
        QCOMPARE(store.entries().size(), 1);
        QCOMPARE(store.entries().first().name, QStringLiteral("New"));
        QCOMPARE(QFileInfo(store.entries().first().filePath).absoluteFilePath(),
                 QFileInfo(project.filePath).absoluteFilePath());

        project.filePath = dir.filePath(QStringLiteral("unsupported.json"));
        QVERIFY(!store.addOrUpdate(project));
        QCOMPARE(store.entries().size(), 1);
    }
};

QTEST_MAIN(tst_RecentProjectsStore)
#include "tst_RecentProjectsStore.moc"
