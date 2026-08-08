#include "persistence/SampleProjectSeeder.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace tlm {

namespace {
bool isSimpleFileName(const QString& name) {
    return !name.isEmpty() && name != QStringLiteral(".") && name != QStringLiteral("..") &&
           QFileInfo(name).fileName() == name;
}

Result<SampleProjectSeedResult> failure(const QString& message, const QString& details = {}) {
    return Result<SampleProjectSeedResult>::failure(message, details);
}
} // namespace

Result<SampleProjectSeedResult>
SampleProjectSeeder::seed(const QString& sourceDirectory, const QString& destinationRoot,
                          const QString& projectDirectoryName, const QString& projectFileName) {
    if (!isSimpleFileName(projectDirectoryName) || !isSimpleFileName(projectFileName)) {
        return failure(QStringLiteral("The bundled sample project name is invalid."));
    }

    const QFileInfo sourceInfo(sourceDirectory);
    if (!sourceInfo.isDir()) {
        return failure(QStringLiteral("The bundled sample project is unavailable."),
                       sourceInfo.absoluteFilePath());
    }

    const QDir source(sourceInfo.absoluteFilePath());
    if (!QFileInfo::exists(source.filePath(projectFileName))) {
        return failure(QStringLiteral("The bundled sample project file is missing."),
                       source.filePath(projectFileName));
    }

    if (!QDir().mkpath(destinationRoot)) {
        return failure(QStringLiteral("The sample project directory could not be created."),
                       destinationRoot);
    }

    QDir destination(destinationRoot);
    const QString targetDirectory = destination.filePath(projectDirectoryName);
    const QString targetProject = QDir(targetDirectory).filePath(projectFileName);
    if (QFileInfo::exists(targetDirectory)) {
        return Result<SampleProjectSeedResult>::success(
            {targetProject, SampleProjectSeedStatus::AlreadyExists});
    }

    const QString stagingName =
        QStringLiteral(".%1.installing-%2")
            .arg(projectDirectoryName, QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString stagingDirectory = destination.filePath(stagingName);
    if (!QDir().mkpath(stagingDirectory)) {
        return failure(QStringLiteral("The sample project staging directory could not be created."),
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
            return abortCopy(QStringLiteral("The bundled sample contains an unsupported link."),
                             entry.absoluteFilePath());
        }

        const QString relativePath = source.relativeFilePath(entry.absoluteFilePath());
        const QString targetPath = QDir(stagingDirectory).filePath(relativePath);
        if (entry.isDir()) {
            if (!QDir().mkpath(targetPath)) {
                return abortCopy(QStringLiteral("A sample project folder could not be copied."),
                                 targetPath);
            }
            continue;
        }

        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath()) ||
            !QFile::copy(entry.absoluteFilePath(), targetPath)) {
            return abortCopy(QStringLiteral("A sample project file could not be copied."),
                             entry.absoluteFilePath());
        }
    }

    if (!destination.rename(stagingName, projectDirectoryName)) {
        return abortCopy(QStringLiteral("The sample project could not be published."),
                         targetDirectory);
    }

    return Result<SampleProjectSeedResult>::success(
        {targetProject, SampleProjectSeedStatus::Copied});
}

} // namespace tlm
