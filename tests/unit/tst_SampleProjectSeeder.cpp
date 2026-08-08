#include "persistence/SampleProjectSeeder.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QtTest>

using namespace tlm;

namespace {
void writeFile(const QString& path, const QByteArray& contents) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), static_cast<qint64>(contents.size()));
}
} // namespace

class tst_SampleProjectSeeder : public QObject {
    Q_OBJECT

private slots:
    void copiesOnceWithoutOverwritingUserChanges() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QString source = temporary.filePath(QStringLiteral("source"));
        const QString destination = temporary.filePath(QStringLiteral("projects"));
        writeFile(QDir(source).filePath(QStringLiteral("Anime Girls v5.tlmproject")),
                  QByteArrayLiteral("{}"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("sample-image"));

        auto first = SampleProjectSeeder::seed(source, destination,
                                               QStringLiteral("Anime Girls v5"),
                                               QStringLiteral("Anime Girls v5.tlmproject"));
        QVERIFY(first);
        QCOMPARE(first.value().status, SampleProjectSeedStatus::Copied);
        QVERIFY(QFileInfo::exists(first.value().projectPath));

        const QString installedImage =
            QDir(destination).filePath(QStringLiteral("Anime Girls v5/assets/image.jpg"));
        writeFile(installedImage, QByteArrayLiteral("user-edited-image"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("updated-sample-image"));

        auto second = SampleProjectSeeder::seed(source, destination,
                                                QStringLiteral("Anime Girls v5"),
                                                QStringLiteral("Anime Girls v5.tlmproject"));
        QVERIFY(second);
        QCOMPARE(second.value().status, SampleProjectSeedStatus::AlreadyExists);

        QFile preserved(installedImage);
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), QByteArrayLiteral("user-edited-image"));
    }

    void rejectsIncompletePayload() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString source = temporary.filePath(QStringLiteral("source"));
        QVERIFY(QDir().mkpath(source));

        const auto result = SampleProjectSeeder::seed(
            source, temporary.filePath(QStringLiteral("projects")),
            QStringLiteral("Anime Girls v5"), QStringLiteral("Anime Girls v5.tlmproject"));
        QVERIFY(!result);
    }
};

QTEST_MAIN(tst_SampleProjectSeeder)
#include "tst_SampleProjectSeeder.moc"
