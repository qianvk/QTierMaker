#include "persistence/SampleProjectSeeder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#if defined(Q_OS_WIN)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace qtm;

namespace {
#if defined(Q_OS_WIN)
class LockedDirectory final {
public:
    explicit LockedDirectory(const QString& path) {
        m_handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    }

    ~LockedDirectory() {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
        }
    }

    [[nodiscard]] bool isValid() const {
        return m_handle != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE m_handle{INVALID_HANDLE_VALUE};
};
#endif

void writeFile(const QString& path, const QByteArray& contents) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), static_cast<qint64>(contents.size()));
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
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
        writeFile(QDir(source).filePath(QStringLiteral("Anime Girls v5.qtm")),
                  QByteArrayLiteral("{}"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("sample-image"));

        auto first =
            SampleProjectSeeder::seed(source, destination, QStringLiteral("Anime Girls v5"),
                                      QStringLiteral("Anime Girls v5.qtm"));
        QVERIFY(first);
        QCOMPARE(first.value().status, SampleProjectSeedStatus::Copied);
        QVERIFY(QFileInfo::exists(first.value().projectPath));

        const QString installedImage =
            QDir(destination).filePath(QStringLiteral("Anime Girls v5/assets/image.jpg"));
        writeFile(installedImage, QByteArrayLiteral("user-edited-image"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("updated-sample-image"));

        auto second =
            SampleProjectSeeder::seed(source, destination, QStringLiteral("Anime Girls v5"),
                                      QStringLiteral("Anime Girls v5.qtm"));
        QVERIFY(second);
        QCOMPARE(second.value().status, SampleProjectSeedStatus::AlreadyExists);
        QCOMPARE(readFile(installedImage), QByteArrayLiteral("user-edited-image"));
    }

    void replacesAsACompleteProject() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QString source = temporary.filePath(QStringLiteral("source"));
        const QString destination = temporary.filePath(QStringLiteral("projects"));
        writeFile(QDir(source).filePath(QStringLiteral("Anime Girls v5.qtm")),
                  QByteArrayLiteral("{}"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("first"));
        QVERIFY(SampleProjectSeeder::seed(source, destination, QStringLiteral("Anime Girls v5"),
                                          QStringLiteral("Anime Girls v5.qtm")));

        const QString staleFile =
            QDir(destination).filePath(QStringLiteral("Anime Girls v5/assets/stale.jpg"));
        writeFile(staleFile, QByteArrayLiteral("stale"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("replacement"));
        auto replaced = SampleProjectSeeder::seed(
            source, destination, QStringLiteral("Anime Girls v5"),
            QStringLiteral("Anime Girls v5.qtm"), SampleProjectConflictPolicy::Replace);
        QVERIFY(replaced);
        QCOMPARE(replaced.value().status, SampleProjectSeedStatus::Replaced);
        QCOMPARE(
            readFile(QDir(destination).filePath(QStringLiteral("Anime Girls v5/assets/image.jpg"))),
            QByteArrayLiteral("replacement"));
        QVERIFY(!QFileInfo::exists(staleFile));
    }

    void replacesWhileDirectoryRenameIsBlocked() {
#if !defined(Q_OS_WIN)
        QSKIP("Windows directory sharing semantics are covered on Windows.");
#else
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QString source = temporary.filePath(QStringLiteral("source"));
        const QString destination = temporary.filePath(QStringLiteral("projects"));
        writeFile(QDir(source).filePath(QStringLiteral("Anime Girls v5.qtm")),
                  QByteArrayLiteral("{}"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("first"));
        QVERIFY(SampleProjectSeeder::seed(source, destination, QStringLiteral("Anime Girls v5"),
                                          QStringLiteral("Anime Girls v5.qtm")));

        const QString installedDirectory =
            QDir(destination).filePath(QStringLiteral("Anime Girls v5"));
        LockedDirectory lock(installedDirectory);
        QVERIFY(lock.isValid());

        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("replacement"));
        const auto replaced = SampleProjectSeeder::seed(
            source, destination, QStringLiteral("Anime Girls v5"),
            QStringLiteral("Anime Girls v5.qtm"), SampleProjectConflictPolicy::Replace);
        if (!replaced) {
            QFAIL(qPrintable(
                QStringLiteral("%1: %2").arg(replaced.error().message, replaced.error().details)));
        }
        QCOMPARE(replaced.value().status, SampleProjectSeedStatus::Replaced);
        QCOMPARE(
            readFile(QDir(destination).filePath(QStringLiteral("Anime Girls v5/assets/image.jpg"))),
            QByteArrayLiteral("replacement"));
#endif
    }

    void keepsBothWithMatchingNames() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QString source = temporary.filePath(QStringLiteral("source"));
        const QString destination = temporary.filePath(QStringLiteral("projects"));
        writeFile(QDir(source).filePath(QStringLiteral("Anime Girls v5.qtm")),
                  QByteArrayLiteral("{}"));
        writeFile(QDir(source).filePath(QStringLiteral("assets/image.jpg")),
                  QByteArrayLiteral("image"));
        QVERIFY(SampleProjectSeeder::seed(source, destination, QStringLiteral("Anime Girls v5"),
                                          QStringLiteral("Anime Girls v5.qtm")));

        auto second = SampleProjectSeeder::seed(
            source, destination, QStringLiteral("Anime Girls v5"),
            QStringLiteral("Anime Girls v5.qtm"), SampleProjectConflictPolicy::KeepBoth);
        QVERIFY(second);
        QCOMPARE(second.value().status, SampleProjectSeedStatus::KeptBoth);
        QCOMPARE(QFileInfo(second.value().projectPath).fileName(),
                 QStringLiteral("Anime Girls v5 (2).qtm"));
        QCOMPARE(QFileInfo(QFileInfo(second.value().projectPath).absolutePath()).fileName(),
                 QStringLiteral("Anime Girls v5 (2)"));
        QVERIFY(QFileInfo::exists(second.value().projectPath));

        auto third = SampleProjectSeeder::seed(
            source, destination, QStringLiteral("Anime Girls v5"),
            QStringLiteral("Anime Girls v5.qtm"), SampleProjectConflictPolicy::KeepBoth);
        QVERIFY(third);
        QCOMPARE(QFileInfo(third.value().projectPath).fileName(),
                 QStringLiteral("Anime Girls v5 (3).qtm"));
    }

    void rejectsIncompletePayload() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString source = temporary.filePath(QStringLiteral("source"));
        QVERIFY(QDir().mkpath(source));

        const auto result = SampleProjectSeeder::seed(
            source, temporary.filePath(QStringLiteral("projects")),
            QStringLiteral("Anime Girls v5"), QStringLiteral("Anime Girls v5.qtm"));
        QVERIFY(!result);
    }
};

QTEST_MAIN(tst_SampleProjectSeeder)
#include "tst_SampleProjectSeeder.moc"
