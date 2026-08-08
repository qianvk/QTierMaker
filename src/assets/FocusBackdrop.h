#pragma once

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace tlm {

/** Semantic parameters for a subdued content-layer backdrop. */
struct FocusBackdropStyle final {
    QColor baseColor;
    qreal sourceVisibility{1.0};
    bool darkAppearance{false};
};

/**
 * Produces an opaque, low-frequency backdrop while retaining enough scene color for orientation.
 * The result is intentionally a standard content material, not a simulated glass control.
 */
QImage createFocusBackdropImage(const QImage& source, const FocusBackdropStyle& style);

/** Caches the semantic raster and its DPR-sized cover independently. */
class FocusBackdropCache final {
public:
    const QPixmap& pixmap(const QString& sourceId, const QImage& source,
                          const FocusBackdropStyle& style, QSize logicalSize,
                          qreal devicePixelRatio);
    void clear();

private:
    QString m_sourceId;
    qint64 m_sourceCacheKey{0};
    FocusBackdropStyle m_style;
    bool m_hasStyle{false};
    QImage m_material;
    QSize m_logicalSize;
    qreal m_devicePixelRatio{0.0};
    QPixmap m_rendered;
};

} // namespace tlm
