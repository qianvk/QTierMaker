#include "persistence/TarArchive.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QSet>

#include <limits>

namespace qtm {

namespace {
constexpr qsizetype kBlockSize = 512;
constexpr int kNameOffset = 0;
constexpr int kNameLength = 100;
constexpr int kSizeOffset = 124;
constexpr int kSizeLength = 12;
constexpr int kChecksumOffset = 148;
constexpr int kChecksumLength = 8;
constexpr int kTypeOffset = 156;
constexpr int kPrefixOffset = 345;
constexpr int kPrefixLength = 155;
constexpr quint64 kMaxEntrySize = 128ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaxArchivePayload = 512ULL * 1024ULL * 1024ULL;
constexpr int kMaxEntryCount = 4096;

QByteArray field(const QByteArray& block, int offset, int length) {
    QByteArray value = block.mid(offset, length);
    const qsizetype terminator = value.indexOf('\0');
    if (terminator >= 0) {
        value.truncate(terminator);
    }
    return value;
}

bool parseOctal(QByteArray value, quint64* result) {
    while (!value.isEmpty() && (value.front() == '\0' || value.front() == ' ')) {
        value.removeFirst();
    }
    while (!value.isEmpty() && (value.back() == '\0' || value.back() == ' ')) {
        value.removeLast();
    }
    quint64 parsed = 0;
    for (const char character : value) {
        if (character < '0' || character > '7') {
            return false;
        }
        const quint64 digit = static_cast<quint64>(character - '0');
        if (parsed > (std::numeric_limits<quint64>::max() - digit) / 8ULL) {
            return false;
        }
        parsed = parsed * 8ULL + digit;
    }
    *result = parsed;
    return true;
}

bool isZeroBlock(const QByteArray& block) {
    for (const char byte : block) {
        if (byte != '\0') {
            return false;
        }
    }
    return true;
}

bool hasValidChecksum(const QByteArray& block) {
    quint64 expected = 0;
    if (!parseOctal(block.mid(kChecksumOffset, kChecksumLength), &expected)) {
        return false;
    }
    quint64 actual = 0;
    for (int index = 0; index < block.size(); ++index) {
        actual += index >= kChecksumOffset && index < kChecksumOffset + kChecksumLength
                      ? static_cast<unsigned char>(' ')
                      : static_cast<unsigned char>(block.at(index));
    }
    return actual == expected;
}

Result<QString> safeRelativePath(const QByteArray& block) {
    const QByteArray nameBytes = field(block, kNameOffset, kNameLength);
    const QByteArray prefixBytes = field(block, kPrefixOffset, kPrefixLength);
    if (nameBytes.isEmpty()) {
        return Result<QString>::failure(QObject::tr("The sample archive contains an empty path."));
    }

    QString path = QString::fromUtf8(nameBytes);
    if (!prefixBytes.isEmpty()) {
        path.prepend(QString::fromUtf8(prefixBytes) + QLatin1Char('/'));
    }
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }

    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty() || clean == QStringLiteral(".") || QDir::isAbsolutePath(clean) ||
        clean == QStringLiteral("..") || clean.startsWith(QStringLiteral("../")) ||
        clean.contains(QStringLiteral("/../"))) {
        return Result<QString>::failure(QObject::tr("The sample archive contains an unsafe path."),
                                        path);
    }
    const QStringList components = clean.split(QLatin1Char('/'));
    for (const QString& component : components) {
        if (component.isEmpty() || component == QStringLiteral(".") ||
            component == QStringLiteral("..") || component.contains(QLatin1Char(':'))) {
            return Result<QString>::failure(
                QObject::tr("The sample archive contains an unsafe path."), path);
        }
    }
    return Result<QString>::success(clean);
}

Result<void> archiveFailure(const QString& message, const QString& details = {}) {
    return Result<void>::failure(message, details);
}
} // namespace

Result<void> TarArchive::extract(const QString& archivePath, const QString& destinationDirectory) {
    QFile archive(archivePath);
    if (!archive.open(QIODevice::ReadOnly)) {
        return archiveFailure(QObject::tr("The sample archive could not be opened."),
                              archive.errorString());
    }
    if (!QDir().mkpath(destinationDirectory)) {
        return archiveFailure(QObject::tr("The sample archive destination could not be created."),
                              destinationDirectory);
    }

    const QString root = QDir::fromNativeSeparators(
        QDir::cleanPath(QFileInfo(destinationDirectory).absoluteFilePath()));
    const QString rootPrefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
#if defined(Q_OS_WIN)
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif

    QSet<QString> extractedPaths;
    quint64 totalPayload = 0;
    int entryCount = 0;
    int zeroBlockCount = 0;
    while (!archive.atEnd()) {
        const QByteArray block = archive.read(kBlockSize);
        if (block.size() != kBlockSize) {
            return archiveFailure(QObject::tr("The sample archive is truncated."));
        }
        if (isZeroBlock(block)) {
            if (++zeroBlockCount == 2) {
                return Result<void>::success();
            }
            continue;
        }
        zeroBlockCount = 0;

        if (++entryCount > kMaxEntryCount || !hasValidChecksum(block)) {
            return archiveFailure(QObject::tr("The sample archive is invalid."));
        }

        auto relativeResult = safeRelativePath(block);
        if (!relativeResult) {
            return archiveFailure(relativeResult.error().message, relativeResult.error().details);
        }
        const QString relativePath = relativeResult.takeValue();
        QString pathKey = relativePath;
#if defined(Q_OS_WIN)
        pathKey = pathKey.toCaseFolded();
#endif
        if (extractedPaths.contains(pathKey)) {
            return archiveFailure(QObject::tr("The sample archive contains duplicate paths."),
                                  relativePath);
        }
        extractedPaths.insert(pathKey);

        quint64 entrySize = 0;
        if (!parseOctal(block.mid(kSizeOffset, kSizeLength), &entrySize) ||
            entrySize > kMaxEntrySize || totalPayload > kMaxArchivePayload - entrySize) {
            return archiveFailure(QObject::tr("The sample archive contains an invalid file size."),
                                  relativePath);
        }
        totalPayload += entrySize;

        const QString targetPath =
            QDir::fromNativeSeparators(QDir::cleanPath(QDir(root).absoluteFilePath(relativePath)));
        if (!targetPath.startsWith(rootPrefix, pathCase)) {
            return archiveFailure(QObject::tr("The sample archive contains an unsafe path."),
                                  relativePath);
        }

        const char type = block.at(kTypeOffset);
        if (type == '5') {
            if (entrySize != 0 || !QDir().mkpath(targetPath)) {
                return archiveFailure(
                    QObject::tr("A sample project folder could not be extracted."), relativePath);
            }
        } else if (type == '\0' || type == '0') {
            if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
                return archiveFailure(
                    QObject::tr("A sample project folder could not be extracted."), relativePath);
            }
            QSaveFile output(targetPath);
            if (!output.open(QIODevice::WriteOnly)) {
                return archiveFailure(QObject::tr("A sample project file could not be extracted."),
                                      output.errorString());
            }

            quint64 remaining = entrySize;
            while (remaining > 0) {
                const qint64 request =
                    static_cast<qint64>(qMin<quint64>(remaining, 64ULL * 1024ULL));
                const QByteArray chunk = archive.read(request);
                if (chunk.size() != request || output.write(chunk) != request) {
                    output.cancelWriting();
                    return archiveFailure(QObject::tr("The sample archive is truncated."),
                                          relativePath);
                }
                remaining -= static_cast<quint64>(request);
            }
            if (!output.commit()) {
                return archiveFailure(QObject::tr("A sample project file could not be extracted."),
                                      output.errorString());
            }
        } else {
            return archiveFailure(QObject::tr("The sample archive contains an unsupported entry."),
                                  relativePath);
        }

        const quint64 padding =
            (static_cast<quint64>(kBlockSize) - entrySize % kBlockSize) % kBlockSize;
        if (padding > 0 &&
            archive.read(static_cast<qint64>(padding)).size() != static_cast<qint64>(padding)) {
            return archiveFailure(QObject::tr("The sample archive is truncated."));
        }
    }

    return archiveFailure(QObject::tr("The sample archive has no valid end marker."));
}

} // namespace qtm
