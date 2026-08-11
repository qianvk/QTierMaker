#pragma once

#include <QString>

namespace qtm {

/** Centralizes the on-disk naming contract for QTierMaker projects. */
class ProjectFileLayout final {
public:
    static QString extension();
    static QString sanitizedStem(const QString& projectName);
    static QString fileName(const QString& projectName);
    static QString projectFilePath(const QString& parentDirectory, const QString& projectName);
    static QString renamedProjectFilePath(const QString& currentFilePath,
                                          const QString& projectName);
    static bool hasProjectExtension(const QString& filePath);
    static bool isManagedProjectPath(const QString& filePath);
};

} // namespace qtm
