#include "export/TierListExporter.h"

#include "theme/Theme.h"
#include "tier/TierBoardWidget.h"

#include <QFileInfo>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <utility>

namespace qtm {

TierListExporter::TierListExporter(QObject* parent) : QObject(parent) {}

QImage TierListExporter::renderBoard(TierBoardWidget* board, const ExportOptions& options) const {
    if (!board || board->size().isEmpty()) {
        return {};
    }
    return board->renderToImage(std::clamp(options.scale, 1, 4));
}

Result<void> TierListExporter::exportBoard(TierBoardWidget* board, const QString& filePath,
                                           const ExportOptions& options) const {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix.isEmpty()) {
        return Result<void>::failure(tr("The export path needs a file extension."));
    }
    if (suffix != QStringLiteral("png") && suffix != QStringLiteral("jpg") &&
        suffix != QStringLiteral("jpeg")) {
        return Result<void>::failure(tr("The export format is not supported."));
    }

    ExportOptions actual = options;
    actual.format = ExportOptions::formatFromSuffix(suffix);
    QImage image = renderBoard(board, actual);
    if (image.isNull()) {
        return Result<void>::failure(tr("Could not render the tier board for export."));
    }

    if (actual.format == ExportFormat::Jpeg && image.hasAlphaChannel()) {
        QImage opaque(image.size(), QImage::Format_RGB32);
        opaque.setDevicePixelRatio(image.devicePixelRatio());
        opaque.fill(activeThemeTokens().contentBackground);
        QPainter painter(&opaque);
        painter.drawImage(QPoint(0, 0), image);
        image = std::move(opaque);
    }

    const char* format = actual.format == ExportFormat::Jpeg ? "JPG" : "PNG";
    if (!image.save(filePath, format,
                    actual.format == ExportFormat::Jpeg ? actual.jpegQuality : -1)) {
        return Result<void>::failure(tr("Could not write the export file."), filePath);
    }
    return Result<void>::success();
}

} // namespace qtm
