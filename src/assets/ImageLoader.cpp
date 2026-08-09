#include "assets/ImageLoader.h"

#include <QImageReader>

namespace qtm {

Result<QImage> ImageLoader::load(const QString& filePath, QSize targetSize) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid() && !targetSize.isEmpty() &&
        (sourceSize.width() > targetSize.width() || sourceSize.height() > targetSize.height())) {
        // Scale in the image plugin so oversized sources never need a full decoded buffer.
        reader.setScaledSize(sourceSize.scaled(targetSize, Qt::KeepAspectRatio));
    }
    QImage image = reader.read();
    if (image.isNull()) {
        return Result<QImage>::failure(QObject::tr("Could not load image."), reader.errorString());
    }
    return Result<QImage>::success(image);
}

QStringList ImageLoader::supportedNameFilters() {
    return {QObject::tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)")};
}

} // namespace qtm
