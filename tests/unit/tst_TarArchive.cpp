#include "persistence/TarArchive.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace qtm;

namespace {
constexpr int kTarBlockSize = 512;

void writeField(QByteArray& header, int offset, int length, const QByteArray& value) {
    QVERIFY(value.size() <= length);
    header.replace(offset, value.size(), value);
}

void writeOctal(QByteArray& header, int offset, int length, quint64 value) {
    QByteArray encoded = QByteArray::number(value, 8).rightJustified(length - 1, '0') + '\0';
    writeField(header, offset, length, encoded);
}

void appendEntry(QByteArray& archive, const QByteArray& path, const QByteArray& contents,
                 char type = '0') {
    QVERIFY(path.size() <= 100);
    QByteArray header(kTarBlockSize, '\0');
    writeField(header, 0, 100, path);
    writeOctal(header, 100, 8, 0644);
    writeOctal(header, 108, 8, 0);
    writeOctal(header, 116, 8, 0);
    writeOctal(header, 124, 12, static_cast<quint64>(contents.size()));
    writeOctal(header, 136, 12, 0);
    header.replace(148, 8, QByteArray(8, ' '));
    header[156] = type;
    writeField(header, 257, 6, QByteArrayLiteral("ustar"));
    writeField(header, 263, 2, QByteArrayLiteral("00"));

    quint64 checksum = 0;
    for (const char byte : header) {
        checksum += static_cast<unsigned char>(byte);
    }
    const QByteArray encodedChecksum =
        QByteArray::number(checksum, 8).rightJustified(6, '0') + '\0' + ' ';
    header.replace(148, 8, encodedChecksum);

    archive.append(header);
    archive.append(contents);
    archive.append(
        QByteArray((kTarBlockSize - contents.size() % kTarBlockSize) % kTarBlockSize, '\0'));
}

QString writeArchive(const QString& path, QByteArray archive) {
    archive.append(QByteArray(kTarBlockSize * 2, '\0'));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(archive) != archive.size()) {
        return {};
    }
    return file.fileName();
}
} // namespace

class tst_TarArchive : public QObject {
    Q_OBJECT

private slots:
    void extractsRegularFiles() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        QByteArray archive;
        appendEntry(archive, QByteArrayLiteral("Anime Girls v5"), {}, '5');
        appendEntry(archive, QByteArrayLiteral("Anime Girls v5/assets"), {}, '5');
        appendEntry(archive, QByteArrayLiteral("Anime Girls v5/project.qtm"),
                    QByteArrayLiteral("{}"));
        appendEntry(archive, QByteArrayLiteral("Anime Girls v5/assets/image.jpg"),
                    QByteArrayLiteral("image"));

        const QString archivePath =
            writeArchive(temporary.filePath(QStringLiteral("sample.tar")), archive);
        QVERIFY(!archivePath.isEmpty());
        const QString destination = temporary.filePath(QStringLiteral("output"));
        const auto result = TarArchive::extract(archivePath, destination);
        QVERIFY2(result, qPrintable(result.error().message));

        QFile project(QDir(destination).filePath(QStringLiteral("Anime Girls v5/project.qtm")));
        QVERIFY(project.open(QIODevice::ReadOnly));
        QCOMPARE(project.readAll(), QByteArrayLiteral("{}"));
        QFile image(QDir(destination).filePath(QStringLiteral("Anime Girls v5/assets/image.jpg")));
        QVERIFY(image.open(QIODevice::ReadOnly));
        QCOMPARE(image.readAll(), QByteArrayLiteral("image"));
    }

    void rejectsTraversal() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        QByteArray archive;
        appendEntry(archive, QByteArrayLiteral("../outside.txt"), QByteArrayLiteral("unsafe"));
        const QString archivePath =
            writeArchive(temporary.filePath(QStringLiteral("traversal.tar")), archive);
        const QString destination = temporary.filePath(QStringLiteral("output"));
        QVERIFY(!TarArchive::extract(archivePath, destination));
        QVERIFY(!QFileInfo::exists(temporary.filePath(QStringLiteral("outside.txt"))));
    }

    void rejectsLinks() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        QByteArray archive;
        appendEntry(archive, QByteArrayLiteral("link"), {}, '2');
        const QString archivePath =
            writeArchive(temporary.filePath(QStringLiteral("link.tar")), archive);
        QVERIFY(!TarArchive::extract(archivePath, temporary.filePath(QStringLiteral("output"))));
    }

    void rejectsInvalidChecksum() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        QByteArray archive;
        appendEntry(archive, QByteArrayLiteral("project.qtm"), QByteArrayLiteral("{}"));
        archive[0] = 'P';
        const QString archivePath =
            writeArchive(temporary.filePath(QStringLiteral("invalid.tar")), archive);
        QVERIFY(!TarArchive::extract(archivePath, temporary.filePath(QStringLiteral("output"))));
    }
};

QTEST_MAIN(tst_TarArchive)
#include "tst_TarArchive.moc"
