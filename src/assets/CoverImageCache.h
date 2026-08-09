#pragma once

#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace qtm {

/** Caches a decoded source and its center-cropped device-pixel rendering. */
class CoverImageCache final {
public:
    const QImage& sourceImage(const QString& path);
    const QPixmap& pixmap(const QString& path, QSize logicalSize, qreal devicePixelRatio);
    void clear();

private:
    bool ensureSource(const QString& path);

    QString m_sourcePath;
    QImage m_source;
    QSize m_logicalSize;
    qreal m_devicePixelRatio{0.0};
    QPixmap m_rendered;
};

} // namespace qtm
