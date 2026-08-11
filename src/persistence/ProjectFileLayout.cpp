#include "persistence/ProjectFileLayout.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace qtm {

QString ProjectFileLayout::extension() {
    return QStringLiteral(".qtm");
}

QString ProjectFileLayout::sanitizedStem(const QString& projectName) {
    QString stem = projectName.trimmed();
    if (stem.endsWith(extension(), Qt::CaseInsensitive)) {
        stem.chop(extension().size());
    }
    stem.replace(QRegularExpression(QStringLiteral(R"([<>:"/\\|?*\x00-\x1f])")),
                 QStringLiteral("_"));
    stem.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    stem = stem.trimmed();
    while (stem.endsWith(u'.') || stem.endsWith(u' ')) {
        stem.chop(1);
    }
    if (stem.isEmpty()) {
        stem = QStringLiteral("Untitled Tier List");
    }

    static const QSet<QString> reservedWindowsNames = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),  QStringLiteral("AUX"),
        QStringLiteral("NUL"),  QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
        QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"),
        QStringLiteral("LPT3"), QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9")};
    if (reservedWindowsNames.contains(stem.section(u'.', 0, 0).toUpper())) {
        stem += QStringLiteral(" Project");
    }

    constexpr qsizetype kMaximumStemLength = 120;
    if (stem.size() > kMaximumStemLength) {
        stem = stem.left(kMaximumStemLength).trimmed();
    }
    return stem;
}

QString ProjectFileLayout::fileName(const QString& projectName) {
    return sanitizedStem(projectName) + extension();
}

QString ProjectFileLayout::projectFilePath(const QString& parentDirectory,
                                           const QString& projectName) {
    const QString stem = sanitizedStem(projectName);
    return QDir(QDir(parentDirectory).filePath(stem)).filePath(stem + extension());
}

QString ProjectFileLayout::renamedProjectFilePath(const QString& currentFilePath,
                                                  const QString& projectName) {
    const QFileInfo currentFile(currentFilePath);
    const QString desiredStem = sanitizedStem(projectName);
    if (hasProjectExtension(currentFilePath) && currentFile.completeBaseName() == desiredStem) {
        return currentFile.absoluteFilePath();
    }
    QString parentDirectory = currentFile.absolutePath();
    if (isManagedProjectPath(currentFilePath)) {
        QDir managedDirectory(parentDirectory);
        if (managedDirectory.cdUp()) {
            parentDirectory = managedDirectory.absolutePath();
        }
    }
    return QFileInfo(projectFilePath(parentDirectory, desiredStem)).absoluteFilePath();
}

bool ProjectFileLayout::hasProjectExtension(const QString& filePath) {
    return QFileInfo(filePath).suffix().compare(extension().sliced(1), Qt::CaseInsensitive) == 0;
}

bool ProjectFileLayout::isManagedProjectPath(const QString& filePath) {
    if (filePath.isEmpty()) {
        return false;
    }
    const QFileInfo projectFile(filePath);
    const QFileInfo projectDirectory(projectFile.absolutePath());
    return projectDirectory.fileName().compare(projectFile.completeBaseName(),
                                               Qt::CaseInsensitive) == 0;
}

} // namespace qtm
