#include "persistence/ProjectFileLayout.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using namespace qtm;

class tst_ProjectFileLayout : public QObject {
    Q_OBJECT

private slots:
    void createsManagedQtmPath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = ProjectFileLayout::projectFilePath(dir.path(), QStringLiteral("A/B"));
        QCOMPARE(path, QDir(dir.path()).filePath(QStringLiteral("A_B/A_B.qtm")));
        QVERIFY(ProjectFileLayout::hasProjectExtension(path));
        QVERIFY(ProjectFileLayout::isManagedProjectPath(path));
    }

    void derivesSiblingStorageAfterTitleRename() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString current = QDir(dir.path()).filePath(QStringLiteral("Old/Old.qtm"));
        const QString renamed =
            ProjectFileLayout::renamedProjectFilePath(current, QStringLiteral("New"));
        QCOMPARE(renamed, QDir(dir.path()).filePath(QStringLiteral("New/New.qtm")));
    }

    void preservesAnUnchangedExternalProjectPath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString current = QDir(dir.path()).filePath(QStringLiteral("Existing.qtm"));
        QCOMPARE(ProjectFileLayout::renamedProjectFilePath(current, QStringLiteral("Existing")),
                 current);
    }
};

QTEST_MAIN(tst_ProjectFileLayout)
#include "tst_ProjectFileLayout.moc"
