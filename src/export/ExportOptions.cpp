#include "export/ExportOptions.h"

namespace qtm {

ExportFormat ExportOptions::formatFromSuffix(const QString& suffix) {
    const QString normalized = suffix.toLower();
    if (normalized == QStringLiteral("jpg") || normalized == QStringLiteral("jpeg")) {
        return ExportFormat::Jpeg;
    }
    return ExportFormat::Png;
}

QString ExportOptions::suffixForFormat(ExportFormat format) {
    switch (format) {
    case ExportFormat::Jpeg:
        return QStringLiteral("jpg");
    case ExportFormat::Png:
    default:
        return QStringLiteral("png");
    }
}

} // namespace qtm
