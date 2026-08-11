#include "persistence/ProjectStorage.h"

#include "persistence/ProjectFileLayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

namespace qtm {

Result<void> ProjectStorage::remove(const QString& projectFilePath) {
    if (projectFilePath.isEmpty() || !QFileInfo::exists(projectFilePath)) {
        return Result<void>::success();
    }
    if (!QFile::remove(projectFilePath)) {
        return Result<void>::failure(QObject::tr("Could not remove the previous project file."),
                                     projectFilePath);
    }
    if (!ProjectFileLayout::isManagedProjectPath(projectFilePath)) {
        return Result<void>::success();
    }

    const QString directory = QFileInfo(projectFilePath).absolutePath();
    const QString assetsDirectory = QDir(directory).filePath(QStringLiteral("assets"));
    if (QFileInfo::exists(assetsDirectory) && !QDir(assetsDirectory).removeRecursively()) {
        return Result<void>::failure(QObject::tr("Could not remove the previous project folder."),
                                     assetsDirectory);
    }

    QDir parentDirectory(directory);
    if (!parentDirectory.cdUp() || !parentDirectory.rmdir(QFileInfo(directory).fileName())) {
        // Never recursively delete unexpected user files in a managed project folder.
        return Result<void>::failure(QObject::tr("Could not remove the previous project folder."),
                                     directory);
    }
    return Result<void>::success();
}

} // namespace qtm
