#pragma once

#include "tier/ImagePresentationMode.h"

#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QVector>

namespace qtm {

/** Shared geometry for one tier row. Painting and hit testing must use this exact grid. */
struct TierRowGrid {
    QRect contentRect;
    int lineHeight{1};
    int tileSide{1};
    int columns{1};

    QRect tileRect(int itemIndex) const;
    int requiredRows(int itemCount) const;
    int insertionIndex(const QPoint& point, int itemCount) const;
    QVector<QRect> itemRects(const QVector<QSize>& sourceSizes,
                             ImagePresentationMode mode) const;
    int requiredRows(const QVector<QSize>& sourceSizes, ImagePresentationMode mode) const;
    int insertionIndex(const QPoint& point, const QVector<QSize>& sourceSizes,
                       ImagePresentationMode mode) const;
};

struct TierBoardLayoutMetrics {
    QVector<int> rowHeights;
    QVector<int> rowUnits;
};

/** Centered free-form packing used by Mission Control without cropping source images. */
struct MissionControlLayoutMetrics {
    QVector<QRectF> itemRects;
    qreal scale{0.0};
    qreal imageAreaOccupancy{0.0};
    qreal horizontalOccupancy{0.0};
    qreal verticalOccupancy{0.0};
};

/** Local, gap-preserving magnification result for Mission Control hover. */
struct MissionControlHoverLayoutMetrics {
    QVector<QRectF> itemRects;
    int localNeighborCount{0};
    int affectedNeighborCount{0};
    int changedNeighborCount{0};
    qreal appliedProgress{0.0};
    bool constrained{false};
};

/** Computes a non-scrolling board layout that keeps every tile inside its tier row. */
class TierListLayout final {
public:
    static TierRowGrid gridForRow(const QRect& rowRect, int rowUnits, int labelWidth);
    static int requiredRowUnits(int imageCount, int rowWidth, int lineHeight, int labelWidth);
    static TierBoardLayoutMetrics fitBoard(const QVector<int>& imageCounts,
                                           const QSize& viewportSize, int labelWidth);
    static TierBoardLayoutMetrics fitBoard(const QVector<QVector<QSize>>& imageSizes,
                                           const QSize& viewportSize, int labelWidth,
                                           ImagePresentationMode mode);
    static MissionControlLayoutMetrics fitMissionControl(const QVector<QSizeF>& sourceSizes,
                                                         const QRectF& bounds, qreal gap);
    // Animation timing remains a view concern; easedProgress is normalized to [0, 1].
    static MissionControlHoverLayoutMetrics
    applyMissionControlHover(const QVector<QRectF>& baseRects, int hoverIndex,
                             const QRectF& hoverTarget, const QRectF& movementBounds,
                             qreal easedProgress, qreal gap);
};

} // namespace qtm
