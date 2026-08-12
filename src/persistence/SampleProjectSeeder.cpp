#include "persistence/SampleProjectSeeder.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <utility>

namespace qtm {

namespace {
constexpr qsizetype kCopyBufferSize = 128 * 1024;

bool isSimpleFileName(const QString& name) {
    return !name.isEmpty() && name != QStringLiteral(".") && name != QStringLiteral("..") &&
           QFileInfo(name).fileName() == name;
}

Result<SampleProjectSeedResult> failure(const QString& message, const QString& details = {}) {
    return Result<SampleProjectSeedResult>::failure(message, details);
}

bool removePath(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return true;
    }
    if (info.isDir() && !info.isSymLink()) {
        return QDir(path).removeRecursively();
    }
    if (QFile::remove(path)) {
        return true;
    }
    QFile::setPermissions(path, info.permissions() | QFileDevice::WriteUser);
    return QFile::remove(path);
}

QString suffixedName(const QString& name, int suffix) {
    return QStringLiteral("%1 (%2)").arg(name).arg(suffix);
}

bool filesEqual(const QString& leftPath, const QString& rightPath) {
    const QFileInfo leftInfo(leftPath);
    const QFileInfo rightInfo(rightPath);
    if (!leftInfo.isFile() || !rightInfo.isFile() || leftInfo.size() != rightInfo.size()) {
        return false;
    }

    QFile left(leftPath);
    QFile right(rightPath);
    if (!left.open(QIODevice::ReadOnly) || !right.open(QIODevice::ReadOnly)) {
        return false;
    }

    while (!left.atEnd()) {
        const QByteArray leftChunk = left.read(kCopyBufferSize);
        const QByteArray rightChunk = right.read(kCopyBufferSize);
        if (leftChunk != rightChunk) {
            return false;
        }
    }
    return right.atEnd();
}

bool copyFileAtomically(const QString& sourcePath, const QString& targetPath, QString* details) {
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        *details = QStringLiteral("%1: %2").arg(sourcePath, source.errorString());
        return false;
    }

    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly)) {
        *details = QStringLiteral("%1: %2").arg(targetPath, target.errorString());
        return false;
    }

    QByteArray buffer(kCopyBufferSize, Qt::Uninitialized);
    while (true) {
        const qint64 bytesRead = source.read(buffer.data(), buffer.size());
        if (bytesRead < 0) {
            *details = QStringLiteral("%1: %2").arg(sourcePath, source.errorString());
            target.cancelWriting();
            return false;
        }
        if (bytesRead == 0) {
            break;
        }
        if (target.write(buffer.constData(), bytesRead) != bytesRead) {
            *details = QStringLiteral("%1: %2").arg(targetPath, target.errorString());
            target.cancelWriting();
            return false;
        }
    }

    if (!target.commit()) {
        *details = QStringLiteral("%1: %2").arg(targetPath, target.errorString());
        return false;
    }
    return true;
}

bool copyDirectoryTree(const QString& sourcePath, const QString& targetPath, QString* details) {
    const QDir source(sourcePath);
    if (!source.exists() || !QDir().mkpath(targetPath)) {
        *details = targetPath;
        return false;
    }

    QDirIterator iterator(source.absolutePath(),
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo entry = iterator.fileInfo();
        if (entry.isSymLink()) {
            *details = entry.absoluteFilePath();
            return false;
        }

        const QString relativePath = source.relativeFilePath(entry.absoluteFilePath());
        const QString destinationPath = QDir(targetPath).filePath(relativePath);
        if (entry.isDir()) {
            if (!QDir().mkpath(destinationPath)) {
                *details = destinationPath;
                return false;
            }
            continue;
        }
        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath()) ||
            !QFile::copy(entry.absoluteFilePath(), destinationPath)) {
            *details = QStringLiteral("%1 -> %2").arg(entry.absoluteFilePath(), destinationPath);
            return false;
        }
    }
    return true;
}

struct TreeEntry {
    QString relativePath;
    QString absolutePath;
    bool directory{false};
};

bool mirrorDirectoryTree(const QString& sourcePath, const QString& targetPath, QString* details) {
    const QDir source(sourcePath);
    if (!source.exists() || !QDir().mkpath(targetPath)) {
        *details = targetPath;
        return false;
    }

    QSet<QString> sourceEntries;
    QDirIterator sourceIterator(source.absolutePath(),
                                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
                                    QDir::System,
                                QDirIterator::Subdirectories);
    while (sourceIterator.hasNext()) {
        sourceIterator.next();
        const QFileInfo entry = sourceIterator.fileInfo();
        if (entry.isSymLink()) {
            *details = entry.absoluteFilePath();
            return false;
        }

        const QString relativePath =
            QDir::fromNativeSeparators(source.relativeFilePath(entry.absoluteFilePath()));
        sourceEntries.insert(relativePath);
        const QString destinationPath = QDir(targetPath).filePath(relativePath);
        const QFileInfo destinationInfo(destinationPath);

        if (entry.isDir()) {
            if ((destinationInfo.exists() || destinationInfo.isSymLink()) &&
                (!destinationInfo.isDir() || destinationInfo.isSymLink()) &&
                !removePath(destinationPath)) {
                *details = destinationPath;
                return false;
            }
            if (!QDir().mkpath(destinationPath)) {
                *details = destinationPath;
                return false;
            }
            continue;
        }

        if ((destinationInfo.exists() || destinationInfo.isSymLink()) &&
            (destinationInfo.isDir() || destinationInfo.isSymLink()) &&
            !removePath(destinationPath)) {
            *details = destinationPath;
            return false;
        }
        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())) {
            *details = destinationPath;
            return false;
        }
        if (!filesEqual(entry.absoluteFilePath(), destinationPath) &&
            !copyFileAtomically(entry.absoluteFilePath(), destinationPath, details)) {
            return false;
        }
    }

    QVector<TreeEntry> targetEntries;
    const QDir target(targetPath);
    QDirIterator targetIterator(target.absolutePath(),
                                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
                                    QDir::System,
                                QDirIterator::Subdirectories);
    while (targetIterator.hasNext()) {
        targetIterator.next();
        const QFileInfo entry = targetIterator.fileInfo();
        const QString relativePath =
            QDir::fromNativeSeparators(target.relativeFilePath(entry.absoluteFilePath()));
        targetEntries.append(
            {relativePath, entry.absoluteFilePath(), entry.isDir() && !entry.isSymLink()});
    }

    std::sort(targetEntries.begin(), targetEntries.end(),
              [](const TreeEntry& left, const TreeEntry& right) {
                  const int leftDepth = left.relativePath.count(QLatin1Char('/'));
                  const int rightDepth = right.relativePath.count(QLatin1Char('/'));
                  if (leftDepth != rightDepth) {
                      return leftDepth > rightDepth;
                  }
                  if (left.directory != right.directory) {
                      return !left.directory;
                  }
                  return left.relativePath.size() > right.relativePath.size();
              });
    for (const TreeEntry& entry : std::as_const(targetEntries)) {
        if (!sourceEntries.contains(entry.relativePath) && !removePath(entry.absolutePath)) {
            *details = entry.absolutePath;
            return false;
        }
    }
    return true;
}

Result<SampleProjectSeedResult> replaceProjectTree(const QString& stagingDirectory,
                                                   const QString& targetDirectory,
                                                   const QString& targetProject,
                                                   const QString& backupDirectory) {
    QString details;
    if (!copyDirectoryTree(targetDirectory, backupDirectory, &details)) {
        removePath(backupDirectory);
        removePath(stagingDirectory);
        return failure(QObject::tr("The existing sample project could not be backed up."), details);
    }

    if (!mirrorDirectoryTree(stagingDirectory, targetDirectory, &details)) {
        QString rollbackDetails;
        const bool rolledBack =
            mirrorDirectoryTree(backupDirectory, targetDirectory, &rollbackDetails);
        removePath(stagingDirectory);
        if (rolledBack) {
            removePath(backupDirectory);
        } else {
            details = QObject::tr("%1\nRollback copy: %2\nBackup: %3")
                          .arg(details, rollbackDetails, backupDirectory);
        }
        return failure(QObject::tr("The existing sample project could not be replaced."), details);
    }

    removePath(stagingDirectory);
    // Cleanup failure is harmless: the installed project is complete and the hidden backup remains
    // available for manual recovery.
    removePath(backupDirectory);
    return Result<SampleProjectSeedResult>::success(
        {targetProject, SampleProjectSeedStatus::Replaced});
}
} // namespace

Result<SampleProjectSeedResult>
SampleProjectSeeder::seed(const QString& sourceDirectory, const QString& destinationRoot,
                          const QString& projectDirectoryName, const QString& projectFileName,
                          SampleProjectConflictPolicy conflictPolicy) {
    if (!isSimpleFileName(projectDirectoryName) || !isSimpleFileName(projectFileName)) {
        return failure(QObject::tr("The sample project name is invalid."));
    }

    const QFileInfo sourceInfo(sourceDirectory);
    if (!sourceInfo.isDir()) {
        return failure(QObject::tr("The downloaded sample project is unavailable."),
                       sourceInfo.absoluteFilePath());
    }

    const QDir source(sourceInfo.absoluteFilePath());
    const QFileInfo sourceProject(source.filePath(projectFileName));
    if (!sourceProject.isFile() || sourceProject.isSymLink()) {
        return failure(QObject::tr("The downloaded sample project file is missing."),
                       source.filePath(projectFileName));
    }

    if (!QDir().mkpath(destinationRoot)) {
        return failure(QObject::tr("The sample project directory could not be created."),
                       destinationRoot);
    }

    QDir destination(destinationRoot);
    QString targetDirectoryName = projectDirectoryName;
    QString targetProjectFileName = projectFileName;
    QString targetDirectory = destination.filePath(targetDirectoryName);
    const bool targetExists = QFileInfo::exists(targetDirectory);
    bool keepingBoth = false;
    if (targetExists) {
        if (conflictPolicy == SampleProjectConflictPolicy::KeepExisting) {
            return Result<SampleProjectSeedResult>::success(
                {QDir(targetDirectory).filePath(targetProjectFileName),
                 SampleProjectSeedStatus::AlreadyExists});
        }
        if (conflictPolicy == SampleProjectConflictPolicy::KeepBoth) {
            const QFileInfo projectFile(projectFileName);
            int suffix = 2;
            do {
                targetDirectoryName = suffixedName(projectDirectoryName, suffix++);
                targetDirectory = destination.filePath(targetDirectoryName);
            } while (QFileInfo::exists(targetDirectory));
            targetProjectFileName =
                suffixedName(projectFile.completeBaseName(), suffix - 1) +
                (projectFile.suffix().isEmpty() ? QString()
                                                : QStringLiteral(".") + projectFile.suffix());
            keepingBoth = true;
        }
    }
    const bool replacing = targetExists && conflictPolicy == SampleProjectConflictPolicy::Replace;
    const QString targetProject = QDir(targetDirectory).filePath(targetProjectFileName);

    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingName =
        QStringLiteral(".%1.installing-%2").arg(targetDirectoryName, transactionId);
    const QString stagingDirectory = destination.filePath(stagingName);
    if (!QDir().mkpath(stagingDirectory)) {
        return failure(QObject::tr("The sample project staging directory could not be created."),
                       stagingDirectory);
    }

    const auto abortCopy = [&stagingDirectory](const QString& message,
                                               const QString& details = QString()) {
        QDir(stagingDirectory).removeRecursively();
        return failure(message, details);
    };

    QDirIterator iterator(source.absolutePath(),
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo entry = iterator.fileInfo();
        if (entry.isSymLink()) {
            return abortCopy(QObject::tr("The sample project contains an unsupported link."),
                             entry.absoluteFilePath());
        }

        const QString relativePath = source.relativeFilePath(entry.absoluteFilePath());
        const QString stagedRelativePath =
            relativePath == projectFileName ? targetProjectFileName : relativePath;
        const QString targetPath = QDir(stagingDirectory).filePath(stagedRelativePath);
        if (entry.isDir()) {
            if (!QDir().mkpath(targetPath)) {
                return abortCopy(QObject::tr("A sample project folder could not be copied."),
                                 targetPath);
            }
            continue;
        }

        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath()) ||
            !QFile::copy(entry.absoluteFilePath(), targetPath)) {
            return abortCopy(QObject::tr("A sample project file could not be copied."),
                             entry.absoluteFilePath());
        }
    }

    if (replacing) {
        const QString backupDirectory = destination.filePath(
            QStringLiteral(".%1.replaced-%2").arg(targetDirectoryName, transactionId));
        return replaceProjectTree(stagingDirectory, targetDirectory, targetProject,
                                  backupDirectory);
    }

    if (!destination.rename(stagingName, targetDirectoryName)) {
        return abortCopy(QObject::tr("The sample project could not be published."),
                         targetDirectory);
    }

    const SampleProjectSeedStatus status =
        keepingBoth ? SampleProjectSeedStatus::KeptBoth : SampleProjectSeedStatus::Copied;
    return Result<SampleProjectSeedResult>::success({targetProject, status});
}

} // namespace qtm
