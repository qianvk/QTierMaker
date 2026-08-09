#include "tier/TierListLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace qtm {

namespace {
constexpr int kContentLeadingBorder = 1;
constexpr int kContentTrailingBorder = 1;
constexpr int kTileMargin = 0;
constexpr int kTileSpacing = 0;
constexpr int kMaximumTileSide = 512;

int tileSideForLineHeight(int lineHeight) {
    return std::clamp(lineHeight, 1, kMaximumTileSide);
}

QVector<int> distributeRowHeights(const QVector<int>& rowUnits, int availableHeight) {
    int totalUnits = 0;
    for (int units : rowUnits) {
        totalUnits += qMax(1, units);
    }

    const int baseUnitHeight = qMax(1, availableHeight / qMax(1, totalUnits));
    int remainingPixels = availableHeight - baseUnitHeight * totalUnits;
    QVector<int> rowHeights;
    rowHeights.reserve(rowUnits.size());
    for (int units : rowUnits) {
        units = qMax(1, units);
        const int extra = qBound(0, remainingPixels, units);
        rowHeights.append(units * baseUnitHeight + extra);
        remainingPixels -= extra;
    }
    return rowHeights;
}

int densestRow(const QVector<qreal>& rowLoads, const QVector<int>& rowUnits) {
    int bestRow = 0;
    for (int row = 1; row < rowLoads.size(); ++row) {
        const qreal left = qMax<qreal>(0.0, rowLoads.at(row)) * qMax(1, rowUnits.at(bestRow));
        const qreal right = qMax<qreal>(0.0, rowLoads.at(bestRow)) * qMax(1, rowUnits.at(row));
        if (left > right ||
            (qFuzzyCompare(left + 1.0, right + 1.0) &&
             rowLoads.at(row) > rowLoads.at(bestRow))) {
            bestRow = row;
        }
    }
    return bestRow;
}

struct MissionPlacement {
    int inputIndex{-1};
    QRectF imageRect;
};

struct MissionPackingCandidate {
    QVector<MissionPlacement> placements;
    qreal scale{0.0};
    qreal imageAreaOccupancy{0.0};
    qreal horizontalOccupancy{0.0};
    qreal verticalOccupancy{0.0};
    qreal density{0.0};
    qreal centerCompactness{0.0};
    qreal score{-std::numeric_limits<qreal>::infinity()};

    bool isValid() const {
        return !placements.isEmpty();
    }
};

bool rectContainsRect(const QRectF& outer, const QRectF& inner) {
    constexpr qreal kEpsilon = 0.25;
    const qreal outerRight = outer.left() + outer.width();
    const qreal outerBottom = outer.top() + outer.height();
    const qreal innerRight = inner.left() + inner.width();
    const qreal innerBottom = inner.top() + inner.height();
    return outer.left() <= inner.left() + kEpsilon && outer.top() <= inner.top() + kEpsilon &&
           outerRight + kEpsilon >= innerRight && outerBottom + kEpsilon >= innerBottom;
}

QSizeF normalizedSourceSize(const QSizeF& size) {
    return QSizeF(qMax<qreal>(1.0, size.width()), qMax<qreal>(1.0, size.height()));
}

qreal clampLayoutCoordinate(qreal minimum, qreal value, qreal maximum) {
    return maximum < minimum ? (minimum + maximum) / 2.0 : qBound(minimum, value, maximum);
}

QRectF rectWithMargin(const QRectF& rect, qreal margin) {
    return rect.adjusted(-margin / 2.0, -margin / 2.0, margin / 2.0, margin / 2.0);
}

bool rectsConflictWithMargin(const QRectF& first, const QRectF& second, qreal margin) {
    const QRectF overlap =
        rectWithMargin(first, margin).intersected(rectWithMargin(second, margin));
    constexpr qreal kGeometryEpsilon = 0.01;
    return overlap.width() > kGeometryEpsilon && overlap.height() > kGeometryEpsilon;
}

QVector<int> localHoverNeighbors(const QVector<QRectF>& baseRects, int hoverIndex,
                                 const QRectF& hoverTarget, qreal gap) {
    QVector<int> neighbors;
    neighbors.reserve(qMax(0, baseRects.size() - 1));
    for (int index = 0; index < baseRects.size(); ++index) {
        if (index != hoverIndex && rectsConflictWithMargin(baseRects.at(index), hoverTarget, gap)) {
            neighbors.append(index);
        }
    }
    return neighbors;
}

QRectF interpolatedRect(const QRectF& first, const QRectF& second, qreal progress) {
    return QRectF(first.topLeft() + (second.topLeft() - first.topLeft()) * progress,
                  first.size() + (second.size() - first.size()) * progress);
}

class MissionMaxRectsPacker {
public:
    explicit MissionMaxRectsPacker(const QRectF& bounds)
        : m_bounds(bounds), m_center(bounds.center()) {
        m_freeRects.append(bounds);
    }

    QRectF insert(const QSizeF& size) {
        constexpr qreal kEpsilon = 0.25;
        int bestIndex = -1;
        QRectF bestRect;
        qreal bestCenterScore = std::numeric_limits<qreal>::max();
        qreal bestShortScore = std::numeric_limits<qreal>::max();
        qreal bestAreaScore = std::numeric_limits<qreal>::max();

        for (int index = 0; index < m_freeRects.size(); ++index) {
            const QRectF freeRect = m_freeRects.at(index);
            if (size.width() > freeRect.width() || size.height() > freeRect.height()) {
                continue;
            }

            const qreal x =
                clampLayoutCoordinate(freeRect.left(), m_center.x() - size.width() / 2.0,
                                      freeRect.right() - size.width());
            const qreal y =
                clampLayoutCoordinate(freeRect.top(), m_center.y() - size.height() / 2.0,
                                      freeRect.bottom() - size.height());
            const QRectF candidate(x, y, size.width(), size.height());
            const QPointF delta = candidate.center() - m_center;
            const qreal centerScore = delta.x() * delta.x() + delta.y() * delta.y();
            const qreal shortScore =
                qMin(freeRect.width() - size.width(), freeRect.height() - size.height());
            const qreal areaScore =
                freeRect.width() * freeRect.height() - size.width() * size.height();

            if (centerScore < bestCenterScore - kEpsilon ||
                (qAbs(centerScore - bestCenterScore) <= kEpsilon &&
                 (shortScore < bestShortScore - kEpsilon ||
                  (qAbs(shortScore - bestShortScore) <= kEpsilon && areaScore < bestAreaScore)))) {
                bestIndex = index;
                bestRect = candidate;
                bestCenterScore = centerScore;
                bestShortScore = shortScore;
                bestAreaScore = areaScore;
            }
        }

        if (bestIndex < 0) {
            return {};
        }

        splitFreeRects(bestRect);
        pruneFreeRects();
        return bestRect;
    }

private:
    void splitFreeRects(const QRectF& usedRect) {
        QVector<QRectF> nextFreeRects;
        nextFreeRects.reserve(m_freeRects.size() * 2);

        for (const QRectF& freeRect : std::as_const(m_freeRects)) {
            if (!freeRect.intersects(usedRect)) {
                nextFreeRects.append(freeRect);
                continue;
            }

            const QRectF intersection = freeRect.intersected(usedRect);
            if (!intersection.isValid() || intersection.isEmpty()) {
                nextFreeRects.append(freeRect);
                continue;
            }

            appendFreeRect(nextFreeRects,
                           QRectF(freeRect.left(), freeRect.top(),
                                  usedRect.left() - freeRect.left(), freeRect.height()));
            appendFreeRect(nextFreeRects,
                           QRectF(usedRect.right(), freeRect.top(),
                                  freeRect.right() - usedRect.right(), freeRect.height()));
            appendFreeRect(nextFreeRects,
                           QRectF(freeRect.left(), freeRect.top(), freeRect.width(),
                                  usedRect.top() - freeRect.top()));
            appendFreeRect(nextFreeRects,
                           QRectF(freeRect.left(), usedRect.bottom(), freeRect.width(),
                                  freeRect.bottom() - usedRect.bottom()));
        }

        m_freeRects = std::move(nextFreeRects);
    }

    void appendFreeRect(QVector<QRectF>& rects, const QRectF& rect) const {
        if (rect.width() < 1.0 || rect.height() < 1.0) {
            return;
        }
        const QRectF clipped = rect.intersected(m_bounds);
        if (clipped.width() >= 1.0 && clipped.height() >= 1.0) {
            rects.append(clipped);
        }
    }

    void pruneFreeRects() {
        QVector<QRectF> pruned;
        pruned.reserve(m_freeRects.size());
        for (int index = 0; index < m_freeRects.size(); ++index) {
            bool contained = false;
            for (int other = 0; other < m_freeRects.size(); ++other) {
                if (index != other &&
                    rectContainsRect(m_freeRects.at(other), m_freeRects.at(index))) {
                    contained = true;
                    break;
                }
            }
            if (!contained) {
                pruned.append(m_freeRects.at(index));
            }
        }
        m_freeRects = std::move(pruned);
    }

    QRectF m_bounds;
    QPointF m_center;
    QVector<QRectF> m_freeRects;
};

QVector<MissionPlacement> packMissionTilesAtScale(const QVector<QSizeF>& sourceSizes,
                                                  const QRectF& bounds, qreal gap, qreal scale) {
    QVector<int> order;
    order.reserve(sourceSizes.size());
    for (int index = 0; index < sourceSizes.size(); ++index) {
        order.append(index);
    }
    std::stable_sort(order.begin(), order.end(), [&sourceSizes](int left, int right) {
        const QSizeF leftSize = sourceSizes.at(left);
        const QSizeF rightSize = sourceSizes.at(right);
        const qreal leftArea = leftSize.width() * leftSize.height();
        const qreal rightArea = rightSize.width() * rightSize.height();
        return qFuzzyCompare(leftArea + 1.0, rightArea + 1.0) ? left < right
                                                              : leftArea > rightArea;
    });

    MissionMaxRectsPacker packer(bounds);
    QVector<MissionPlacement> placements;
    placements.reserve(sourceSizes.size());
    for (int inputIndex : std::as_const(order)) {
        const QSizeF sourceSize = normalizedSourceSize(sourceSizes.at(inputIndex));
        const QSizeF imageSize(qMax<qreal>(1.0, sourceSize.width() * scale),
                               qMax<qreal>(1.0, sourceSize.height() * scale));
        const QSizeF paddedSize(imageSize.width() + gap, imageSize.height() + gap);
        const QRectF paddedRect = packer.insert(paddedSize);
        if (!paddedRect.isValid() || paddedRect.isEmpty()) {
            return {};
        }
        placements.append(MissionPlacement{
            inputIndex,
            paddedRect.adjusted(gap / 2.0, gap / 2.0, -gap / 2.0, -gap / 2.0),
        });
    }
    return placements;
}

QRectF shrinkRectAwayFromPoint(const QRectF& base, const QPointF& point, qreal scale) {
    scale = qBound<qreal>(0.1, scale, 1.0);
    const QSizeF size = base.size() * scale;
    const QPointF center = base.center();
    const qreal horizontalDeadZone = base.width() * 0.18;
    const qreal verticalDeadZone = base.height() * 0.18;

    qreal left = center.x() - size.width() / 2.0;
    if (center.x() < point.x() - horizontalDeadZone) {
        left = base.left();
    } else if (center.x() > point.x() + horizontalDeadZone) {
        left = base.right() - size.width();
    }

    qreal top = center.y() - size.height() / 2.0;
    if (center.y() < point.y() - verticalDeadZone) {
        top = base.top();
    } else if (center.y() > point.y() + verticalDeadZone) {
        top = base.bottom() - size.height();
    }
    return QRectF(QPointF(left, top), size);
}

bool hoverLayoutIsValid(const QVector<QRectF>& rects, const QRectF& movementBounds, qreal gap) {
    for (int first = 0; first < rects.size(); ++first) {
        if (!rectContainsRect(movementBounds, rects.at(first))) {
            return false;
        }
        for (int second = first + 1; second < rects.size(); ++second) {
            if (rectsConflictWithMargin(rects.at(first), rects.at(second), gap)) {
                return false;
            }
        }
    }
    return true;
}

struct LocalHoverProjection {
    QVector<QRectF> rects;
    QVector<int> activeNeighbors;
};

LocalHoverProjection shrinkLocalHoverNeighbors(const QVector<QRectF>& baseRects, int hoverIndex,
                                               const QRectF& hoverRect,
                                               const QRectF& movementBounds, qreal minimumScale,
                                               qreal gap, const QVector<char>& active) {
    QVector<QRectF> rects = baseRects;
    QVector<int> activeNeighbors;
    rects[hoverIndex] = hoverRect;
    constexpr int kShrinkSteps = 28;

    for (int index = 0; index < baseRects.size(); ++index) {
        if (index == hoverIndex || !active.at(index)) {
            continue;
        }
        activeNeighbors.append(index);
        const QRectF base = baseRects.at(index);
        bool resolved = false;
        for (int step = 0; step <= kShrinkSteps; ++step) {
            const qreal fraction = static_cast<qreal>(step) / kShrinkSteps;
            const qreal scale = 1.0 - (1.0 - minimumScale) * fraction;
            const QRectF candidate = shrinkRectAwayFromPoint(base, hoverRect.center(), scale);
            if (!rectsConflictWithMargin(candidate, hoverRect, gap)) {
                rects[index] = candidate;
                resolved = true;
                break;
            }
        }
        if (!resolved) {
            return {};
        }
    }
    return hoverLayoutIsValid(rects, movementBounds, gap)
               ? LocalHoverProjection{std::move(rects), std::move(activeNeighbors)}
               : LocalHoverProjection{};
}

qreal anchoredHoverEnergy(const QRectF& rect, const QRectF& anchor, const QRectF& base,
                          const QRectF& hoverRect, const QRectF& hoverBase) {
    const qreal diagonal = qMax<qreal>(1.0, std::hypot(base.width(), base.height()));
    const QPointF displacement = rect.center() - anchor.center();
    const qreal movement = std::hypot(displacement.x(), displacement.y()) / diagonal;
    const qreal baseRadius = std::hypot(base.center().x() - hoverBase.center().x(),
                                        base.center().y() - hoverBase.center().y());
    const qreal displayedRadius = std::hypot(rect.center().x() - hoverRect.center().x(),
                                             rect.center().y() - hoverRect.center().y());
    const qreal radialChange = (displayedRadius - baseRadius) / diagonal;
    constexpr qreal kRadialDistanceWeight = 1.0;
    return movement * movement + kRadialDistanceWeight * radialChange * radialChange;
}

LocalHoverProjection projectLocalHoverNeighbors(const QVector<QRectF>& baseRects, int hoverIndex,
                                                const QRectF& hoverRect,
                                                const QRectF& movementBounds, qreal neighborScale,
                                                qreal gap, QVector<char> active) {
    QVector<QRectF> rects = baseRects;
    QVector<QRectF> anchors = baseRects;
    rects[hoverIndex] = hoverRect;
    anchors[hoverIndex] = hoverRect;

    const auto activate = [&](int index) {
        if (index == hoverIndex || active.at(index)) {
            return;
        }
        active[index] = 1;
        anchors[index] =
            shrinkRectAwayFromPoint(baseRects.at(index), hoverRect.center(), neighborScale);
        rects[index] = anchors.at(index);
    };
    for (int index = 0; index < active.size(); ++index) {
        if (active.at(index)) {
            active[index] = 0;
            activate(index);
        }
    }

    struct SeparationCandidate {
        QRectF first;
        QRectF second;
        qreal cost{std::numeric_limits<qreal>::max()};
        bool valid{false};
    };

    const auto hoverSeparationCandidate = [&](int movingIndex) {
        const QRectF moving = rects.at(movingIndex);
        const QRectF anchor = anchors.at(movingIndex);
        constexpr qreal kGeometryEpsilon = 0.02;
        const qreal separation = gap + kGeometryEpsilon;
        QVector<QRectF> candidates;
        candidates.reserve(4);

        const qreal preferredLeft =
            clampLayoutCoordinate(movementBounds.left(), anchor.center().x() - moving.width() / 2.0,
                                  movementBounds.right() - moving.width());
        const qreal preferredTop =
            clampLayoutCoordinate(movementBounds.top(), anchor.center().y() - moving.height() / 2.0,
                                  movementBounds.bottom() - moving.height());
        candidates.append(QRectF(hoverRect.left() - separation - moving.width(), preferredTop,
                                 moving.width(), moving.height()));
        candidates.append(
            QRectF(hoverRect.right() + separation, preferredTop, moving.width(), moving.height()));
        candidates.append(QRectF(preferredLeft, hoverRect.top() - separation - moving.height(),
                                 moving.width(), moving.height()));
        candidates.append(QRectF(preferredLeft, hoverRect.bottom() + separation, moving.width(),
                                 moving.height()));

        SeparationCandidate best;
        for (const QRectF& candidate : std::as_const(candidates)) {
            if (!rectContainsRect(movementBounds, candidate) ||
                rectsConflictWithMargin(candidate, hoverRect, gap)) {
                continue;
            }
            qreal cost = anchoredHoverEnergy(candidate, anchor, baseRects.at(movingIndex),
                                             hoverRect, baseRects.at(hoverIndex));
            int introducedConflicts = 0;
            for (int index = 0; index < rects.size(); ++index) {
                if (index != movingIndex && index != hoverIndex &&
                    rectsConflictWithMargin(candidate, rects.at(index), gap)) {
                    ++introducedConflicts;
                }
            }
            constexpr qreal kIntroducedConflictPenalty = 2.0;
            cost += kIntroducedConflictPenalty * introducedConflicts;
            if (cost < best.cost) {
                best = movingIndex < hoverIndex
                           ? SeparationCandidate{candidate, hoverRect, cost, true}
                           : SeparationCandidate{hoverRect, candidate, cost, true};
            }
        }
        return best;
    };

    const auto separationCandidate = [&](int firstIndex, int secondIndex, bool horizontal,
                                         qreal firstDirection, qreal requiredDistance) {
        const QRectF first = rects.at(firstIndex);
        const QRectF second = rects.at(secondIndex);
        const bool firstMovable = firstIndex != hoverIndex;
        const bool secondMovable = secondIndex != hoverIndex;
        const qreal secondDirection = -firstDirection;
        const auto availableDistance = [&](const QRectF& rect, qreal direction) {
            if (horizontal) {
                return direction < 0.0 ? rect.left() - movementBounds.left()
                                       : movementBounds.right() - rect.right();
            }
            return direction < 0.0 ? rect.top() - movementBounds.top()
                                   : movementBounds.bottom() - rect.bottom();
        };
        const qreal firstAvailable =
            firstMovable ? qMax<qreal>(0.0, availableDistance(first, firstDirection)) : 0.0;
        const qreal secondAvailable =
            secondMovable ? qMax<qreal>(0.0, availableDistance(second, secondDirection)) : 0.0;
        constexpr qreal kGeometryEpsilon = 0.02;
        const qreal required = requiredDistance + kGeometryEpsilon;
        if (firstAvailable + secondAvailable + kGeometryEpsilon < required) {
            return SeparationCandidate{};
        }

        const qreal minimumFirstMove = qMax<qreal>(0.0, required - secondAvailable);
        const qreal maximumFirstMove = qMin(required, firstAvailable);
        const qreal interval = maximumFirstMove - minimumFirstMove;
        const QVector<qreal> firstMoves{
            minimumFirstMove,
            maximumFirstMove,
            minimumFirstMove + interval * 0.25,
            minimumFirstMove + interval * 0.5,
            minimumFirstMove + interval * 0.75,
        };

        SeparationCandidate best;
        for (qreal firstMove : firstMoves) {
            const qreal secondMove = required - firstMove;
            QRectF candidateFirst = first;
            QRectF candidateSecond = second;
            if (horizontal) {
                candidateFirst.translate(firstDirection * firstMove, 0.0);
                candidateSecond.translate(secondDirection * secondMove, 0.0);
            } else {
                candidateFirst.translate(0.0, firstDirection * firstMove);
                candidateSecond.translate(0.0, secondDirection * secondMove);
            }
            if (rectsConflictWithMargin(candidateFirst, candidateSecond, gap)) {
                continue;
            }

            qreal cost = 0.0;
            if (firstMovable) {
                cost += anchoredHoverEnergy(candidateFirst, anchors.at(firstIndex),
                                            baseRects.at(firstIndex), hoverRect,
                                            baseRects.at(hoverIndex));
            }
            if (secondMovable) {
                cost += anchoredHoverEnergy(candidateSecond, anchors.at(secondIndex),
                                            baseRects.at(secondIndex), hoverRect,
                                            baseRects.at(hoverIndex));
            }

            int introducedConflicts = 0;
            for (int index = 0; index < rects.size(); ++index) {
                if (index == firstIndex || index == secondIndex) {
                    continue;
                }
                introducedConflicts +=
                    rectsConflictWithMargin(candidateFirst, rects.at(index), gap) ? 1 : 0;
                introducedConflicts +=
                    rectsConflictWithMargin(candidateSecond, rects.at(index), gap) ? 1 : 0;
            }
            constexpr qreal kIntroducedConflictPenalty = 2.0;
            cost += kIntroducedConflictPenalty * introducedConflicts;
            if (cost < best.cost) {
                best = {candidateFirst, candidateSecond, cost, true};
            }
        }
        return best;
    };

    const int iterationLimit = qMax(64, baseRects.size() * 16);
    for (int iteration = 0; iteration < iterationLimit; ++iteration) {
        bool hadConflict = false;
        bool changed = false;
        for (int firstIndex = 0; firstIndex < rects.size(); ++firstIndex) {
            for (int secondIndex = firstIndex + 1; secondIndex < rects.size(); ++secondIndex) {
                if (!rectsConflictWithMargin(rects.at(firstIndex), rects.at(secondIndex), gap)) {
                    continue;
                }
                hadConflict = true;
                activate(firstIndex);
                activate(secondIndex);

                const QRectF overlap = rectWithMargin(rects.at(firstIndex), gap)
                                           .intersected(rectWithMargin(rects.at(secondIndex), gap));
                if (overlap.width() <= 0.01 || overlap.height() <= 0.01) {
                    changed = true;
                    continue;
                }

                SeparationCandidate best;
                if (firstIndex == hoverIndex || secondIndex == hoverIndex) {
                    best = hoverSeparationCandidate(firstIndex == hoverIndex ? secondIndex
                                                                             : firstIndex);
                } else {
                    for (bool horizontal : {true, false}) {
                        const qreal required = horizontal ? overlap.width() : overlap.height();
                        for (qreal direction : {-1.0, 1.0}) {
                            SeparationCandidate candidate = separationCandidate(
                                firstIndex, secondIndex, horizontal, direction, required);
                            if (candidate.valid && candidate.cost < best.cost) {
                                best = std::move(candidate);
                            }
                        }
                    }
                }
                if (!best.valid) {
                    return {};
                }
                changed = best.first != rects.at(firstIndex) ||
                          best.second != rects.at(secondIndex) || changed;
                rects[firstIndex] = best.first;
                rects[secondIndex] = best.second;
            }
        }
        if (!hadConflict) {
            QVector<int> activeNeighbors;
            activeNeighbors.reserve(baseRects.size());
            for (int index = 0; index < active.size(); ++index) {
                if (index != hoverIndex && active.at(index)) {
                    activeNeighbors.append(index);
                }
            }
            return hoverLayoutIsValid(rects, movementBounds, gap)
                       ? LocalHoverProjection{std::move(rects), std::move(activeNeighbors)}
                       : LocalHoverProjection{};
        }
        if (!changed) {
            return {};
        }
    }
    return {};
}

QRectF bestExpandedHoverNeighbor(const QVector<QRectF>& rects, const QVector<QRectF>& baseRects,
                                 int index, int hoverIndex, qreal scale,
                                 const QRectF& movementBounds, qreal gap) {
    const QRectF current = rects.at(index);
    const QSizeF targetSize = baseRects.at(index).size() * scale;
    const qreal diagonal =
        qMax<qreal>(1.0, std::hypot(baseRects.at(index).width(), baseRects.at(index).height()));
    QRectF best;
    qreal bestCost = std::numeric_limits<qreal>::max();

    for (int horizontalAnchor = -1; horizontalAnchor <= 1; ++horizontalAnchor) {
        const qreal left =
            horizontalAnchor < 0
                ? current.left()
                : (horizontalAnchor > 0 ? current.right() - targetSize.width()
                                        : current.center().x() - targetSize.width() / 2.0);
        for (int verticalAnchor = -1; verticalAnchor <= 1; ++verticalAnchor) {
            const qreal top =
                verticalAnchor < 0
                    ? current.top()
                    : (verticalAnchor > 0 ? current.bottom() - targetSize.height()
                                          : current.center().y() - targetSize.height() / 2.0);
            const QRectF candidate(QPointF(left, top), targetSize);
            if (!rectContainsRect(movementBounds, candidate)) {
                continue;
            }

            bool conflict = false;
            for (int other = 0; other < rects.size(); ++other) {
                if (other != index &&
                    rectsConflictWithMargin(candidate, rects.at(other), gap)) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) {
                continue;
            }

            const qreal anchorCost =
                anchoredHoverEnergy(candidate, baseRects.at(index), baseRects.at(index),
                                    rects.at(hoverIndex), baseRects.at(hoverIndex));
            const QPointF projectedDelta = candidate.center() - current.center();
            constexpr qreal kProjectionContinuityWeight = 1.0;
            const qreal continuityCost =
                kProjectionContinuityWeight *
                (projectedDelta.x() * projectedDelta.x() +
                 projectedDelta.y() * projectedDelta.y()) /
                (diagonal * diagonal);
            const qreal cost = anchorCost + continuityCost;
            if (cost < bestCost) {
                best = candidate;
                bestCost = cost;
            }
        }
    }
    return best;
}

void maximizeProjectedHoverNeighborSizes(LocalHoverProjection* projection,
                                         const QVector<QRectF>& baseRects, int hoverIndex,
                                         const QRectF& movementBounds, qreal gap) {
    if (!projection || projection->rects.size() != baseRects.size()) {
        return;
    }

    // Restore remote collision-chain tiles first so the visible response stays near the hover.
    std::sort(projection->activeNeighbors.begin(), projection->activeNeighbors.end(),
              [&](int first, int second) {
                  const QPointF firstDelta =
                      baseRects.at(first).center() - baseRects.at(hoverIndex).center();
                  const QPointF secondDelta =
                      baseRects.at(second).center() - baseRects.at(hoverIndex).center();
                  const qreal firstDistance =
                      firstDelta.x() * firstDelta.x() + firstDelta.y() * firstDelta.y();
                  const qreal secondDistance =
                      secondDelta.x() * secondDelta.x() + secondDelta.y() * secondDelta.y();
                  return firstDistance != secondDistance ? firstDistance > secondDistance
                                                         : first < second;
              });

    constexpr int kScaleSearchIterations = 12;
    constexpr int kMaximumGrowthPasses = 8;
    // Growth is monotonic; another pass only consumes space freed by a later neighbor.
    for (int pass = 0; pass < kMaximumGrowthPasses; ++pass) {
        bool grew = false;
        for (int index : std::as_const(projection->activeNeighbors)) {
            const QRectF projected = projection->rects.at(index);
            const QRectF base = baseRects.at(index);
            const qreal originalScale =
                qBound<qreal>(0.1, projected.width() / qMax<qreal>(1.0, base.width()), 1.0);
            if (originalScale >= 0.9999) {
                continue;
            }

            qreal lowerScale = originalScale;
            qreal upperScale = 1.0;
            QRectF best = projected;
            const QRectF fullSize = bestExpandedHoverNeighbor(
                projection->rects, baseRects, index, hoverIndex, upperScale, movementBounds, gap);
            if (fullSize.isValid()) {
                lowerScale = 1.0;
                best = fullSize;
            } else {
                for (int iteration = 0; iteration < kScaleSearchIterations; ++iteration) {
                    const qreal candidateScale = (lowerScale + upperScale) / 2.0;
                    const QRectF candidate =
                        bestExpandedHoverNeighbor(projection->rects, baseRects, index, hoverIndex,
                                                  candidateScale, movementBounds, gap);
                    if (candidate.isValid()) {
                        lowerScale = candidateScale;
                        best = candidate;
                    } else {
                        upperScale = candidateScale;
                    }
                }
            }
            projection->rects[index] = best;
            grew = grew || lowerScale > originalScale + 0.0001;
        }
        if (!grew) {
            break;
        }
    }
}

QRectF missionPlacementBounds(const QVector<MissionPlacement>& placements) {
    if (placements.isEmpty()) {
        return {};
    }
    QRectF groupRect = placements.constFirst().imageRect;
    for (const MissionPlacement& placement : placements) {
        groupRect = groupRect.united(placement.imageRect);
    }
    return groupRect;
}

void centerMissionPlacements(QVector<MissionPlacement>& placements, const QRectF& bounds) {
    const QRectF groupRect = missionPlacementBounds(placements);
    if (!groupRect.isValid()) {
        return;
    }
    const QPointF delta = bounds.center() - groupRect.center();
    for (MissionPlacement& placement : placements) {
        placement.imageRect.translate(delta);
    }
}

bool intervalsNeedSeparation(qreal firstStart, qreal firstEnd, qreal secondStart,
                             qreal secondEnd, qreal gap) {
    constexpr qreal kEpsilon = 0.01;
    return firstStart < secondEnd + gap - kEpsilon &&
           secondStart < firstEnd + gap - kEpsilon;
}

qreal horizontalTravelTowardCenter(const QVector<MissionPlacement>& placements, int movingIndex,
                                   const QRectF& bounds, qreal gap, qreal desiredTravel) {
    const QRectF moving = placements.at(movingIndex).imageRect;
    qreal available = desiredTravel > 0.0 ? bounds.right() - moving.right()
                                           : moving.left() - bounds.left();
    for (int index = 0; index < placements.size(); ++index) {
        if (index == movingIndex) {
            continue;
        }
        const QRectF blocker = placements.at(index).imageRect;
        if (!intervalsNeedSeparation(moving.top(), moving.bottom(), blocker.top(),
                                     blocker.bottom(), gap)) {
            continue;
        }
        if (desiredTravel > 0.0 && blocker.left() >= moving.right()) {
            available = qMin(available, blocker.left() - gap - moving.right());
        } else if (desiredTravel < 0.0 && blocker.right() <= moving.left()) {
            available = qMin(available, moving.left() - gap - blocker.right());
        }
    }
    return qMin(qAbs(desiredTravel), qMax<qreal>(0.0, available));
}

qreal verticalTravelTowardCenter(const QVector<MissionPlacement>& placements, int movingIndex,
                                 const QRectF& bounds, qreal gap, qreal desiredTravel) {
    const QRectF moving = placements.at(movingIndex).imageRect;
    qreal available = desiredTravel > 0.0 ? bounds.bottom() - moving.bottom()
                                           : moving.top() - bounds.top();
    for (int index = 0; index < placements.size(); ++index) {
        if (index == movingIndex) {
            continue;
        }
        const QRectF blocker = placements.at(index).imageRect;
        if (!intervalsNeedSeparation(moving.left(), moving.right(), blocker.left(),
                                     blocker.right(), gap)) {
            continue;
        }
        if (desiredTravel > 0.0 && blocker.top() >= moving.bottom()) {
            available = qMin(available, blocker.top() - gap - moving.bottom());
        } else if (desiredTravel < 0.0 && blocker.bottom() <= moving.top()) {
            available = qMin(available, moving.top() - gap - blocker.bottom());
        }
    }
    return qMin(qAbs(desiredTravel), qMax<qreal>(0.0, available));
}

void compactMissionPlacementsTowardCenter(QVector<MissionPlacement>& placements,
                                          const QRectF& bounds, qreal gap) {
    centerMissionPlacements(placements, bounds);
    QVector<int> order;
    order.reserve(placements.size());
    for (int index = 0; index < placements.size(); ++index) {
        order.append(index);
    }

    // Coordinate descent monotonically reduces every rectangle's distance to the board center.
    // Collision limits apply the same gap on both axes, so no row-specific rules are needed.
    constexpr int kMaximumPasses = 8;
    for (int pass = 0; pass < kMaximumPasses; ++pass) {
        std::sort(order.begin(), order.end(), [&placements, &bounds](int left, int right) {
            const QPointF leftDelta = placements.at(left).imageRect.center() - bounds.center();
            const QPointF rightDelta = placements.at(right).imageRect.center() - bounds.center();
            return leftDelta.x() * leftDelta.x() + leftDelta.y() * leftDelta.y() <
                   rightDelta.x() * rightDelta.x() + rightDelta.y() * rightDelta.y();
        });

        qreal totalTravel = 0.0;
        for (int index : std::as_const(order)) {
            QRectF& rect = placements[index].imageRect;
            const qreal desiredX = bounds.center().x() - rect.center().x();
            const qreal horizontalTravel = horizontalTravelTowardCenter(
                placements, index, bounds, gap, desiredX);
            rect.translate(std::copysign(horizontalTravel, desiredX), 0.0);

            const qreal desiredY = bounds.center().y() - rect.center().y();
            const qreal verticalTravel = verticalTravelTowardCenter(
                placements, index, bounds, gap, desiredY);
            rect.translate(0.0, std::copysign(verticalTravel, desiredY));
            totalTravel += horizontalTravel + verticalTravel;
        }
        if (totalTravel < 0.1) {
            break;
        }
    }
    centerMissionPlacements(placements, bounds);
}

QRectF centeredBoundsForAspect(const QRectF& bounds, qreal aspect) {
    aspect = qMax<qreal>(std::numeric_limits<qreal>::epsilon(), aspect);
    QSizeF size = bounds.size();
    if (size.width() / size.height() > aspect) {
        size.setWidth(size.height() * aspect);
    } else {
        size.setHeight(size.width() / aspect);
    }
    return QRectF(bounds.center() - QPointF(size.width() / 2.0, size.height() / 2.0), size);
}

MissionPackingCandidate packMissionCandidate(const QVector<QSizeF>& sourceSizes,
                                             const QRectF& packingBounds,
                                             const QRectF& displayBounds, qreal gap) {
    MissionPackingCandidate candidate;
    qreal sourceArea = 0.0;
    for (const QSizeF& size : sourceSizes) {
        const QSizeF normalized = normalizedSourceSize(size);
        sourceArea += normalized.width() * normalized.height();
    }

    const qreal packingArea = qMax<qreal>(1.0, packingBounds.width() * packingBounds.height());
    const qreal fillTarget = sourceSizes.size() <= 2 ? 0.54 : 0.84;
    const qreal areaDrivenScale =
        std::sqrt((packingArea * fillTarget) / qMax<qreal>(1.0, sourceArea));
    qreal low = 0.0;
    qreal high = qBound<qreal>(0.08, areaDrivenScale, 2.35);
    constexpr int kSearchSteps = 26;
    for (int step = 0; step < kSearchSteps; ++step) {
        const qreal middle = (low + high) / 2.0;
        QVector<MissionPlacement> placements =
            packMissionTilesAtScale(sourceSizes, packingBounds, gap, middle);
        if (placements.size() == sourceSizes.size()) {
            candidate.placements = std::move(placements);
            candidate.scale = middle;
            low = middle;
        } else {
            high = middle;
        }
    }
    if (!candidate.isValid()) {
        return candidate;
    }

    compactMissionPlacementsTowardCenter(candidate.placements, displayBounds, gap);
    const QRectF groupRect = missionPlacementBounds(candidate.placements);
    const qreal displayArea = qMax<qreal>(1.0, displayBounds.width() * displayBounds.height());
    qreal imageArea = 0.0;
    for (const MissionPlacement& placement : candidate.placements) {
        imageArea += placement.imageRect.width() * placement.imageRect.height();
    }

    candidate.imageAreaOccupancy = qBound<qreal>(0.0, imageArea / displayArea, 1.0);
    candidate.horizontalOccupancy =
        qBound<qreal>(0.0, groupRect.width() / displayBounds.width(), 1.0);
    candidate.verticalOccupancy =
        qBound<qreal>(0.0, groupRect.height() / displayBounds.height(), 1.0);
    const qreal groupArea = qMax<qreal>(1.0, groupRect.width() * groupRect.height());
    candidate.density = qBound<qreal>(0.0, imageArea / groupArea, 1.0);

    qreal weightedDistance = 0.0;
    for (const MissionPlacement& placement : candidate.placements) {
        const qreal area = placement.imageRect.width() * placement.imageRect.height();
        const QPointF delta = placement.imageRect.center() - displayBounds.center();
        const qreal normalizedX = delta.x() / qMax<qreal>(1.0, displayBounds.width() / 2.0);
        const qreal normalizedY = delta.y() / qMax<qreal>(1.0, displayBounds.height() / 2.0);
        weightedDistance += area * (normalizedX * normalizedX + normalizedY * normalizedY);
    }
    candidate.centerCompactness =
        1.0 / (1.0 + std::sqrt(weightedDistance / qMax<qreal>(1.0, imageArea)));

    const qreal weakerAxis =
        qMin(candidate.horizontalOccupancy, candidate.verticalOccupancy);
    const qreal axisDifference =
        qAbs(candidate.horizontalOccupancy - candidate.verticalOccupancy);
    candidate.score = std::sqrt(candidate.imageAreaOccupancy) * 0.25 + weakerAxis * 0.20 +
                      (1.0 - axisDifference) * 0.15 + candidate.density * 0.20 +
                      candidate.centerCompactness * 0.20;
    return candidate;
}

MissionPackingCandidate balancedMissionPacking(const QVector<QSizeF>& sourceSizes,
                                               const QRectF& bounds, qreal gap) {
    const qreal boundsAspect = bounds.width() / bounds.height();
    MissionPackingCandidate best =
        packMissionCandidate(sourceSizes, bounds, bounds, gap);
    if (!best.isValid()) {
        return best;
    }

    const qreal axisDifference =
        qAbs(best.horizontalOccupancy - best.verticalOccupancy);
    if (axisDifference <= 0.04) {
        return best;
    }

    // Aim the packing bounds toward the under-filled axis. The geometric midpoint and bounded
    // correction capture useful MaxRects topology changes without a broad resize-time search.
    const qreal correction =
        qBound<qreal>(0.65, best.verticalOccupancy / best.horizontalOccupancy, 1.55);
    const qreal aspectMultipliers[]{std::sqrt(correction), correction};
    for (qreal multiplier : aspectMultipliers) {
        const QRectF packingBounds = centeredBoundsForAspect(bounds, boundsAspect * multiplier);
        MissionPackingCandidate alternative =
            packMissionCandidate(sourceSizes, packingBounds, bounds, gap);
        if (alternative.isValid() &&
            (alternative.score > best.score + 0.0001 ||
             (qAbs(alternative.score - best.score) <= 0.0001 &&
              alternative.imageAreaOccupancy > best.imageAreaOccupancy))) {
            best = std::move(alternative);
        }
    }
    return best;
}

} // namespace

QRect TierRowGrid::tileRect(int itemIndex) const {
    if (itemIndex < 0) {
        return {};
    }
    const int safeColumns = qMax(1, columns);
    const int line = itemIndex / safeColumns;
    const int column = itemIndex % safeColumns;
    const int step = qMax(1, tileSide + kTileSpacing);
    return QRect(contentRect.left() + kTileMargin + column * step,
                 contentRect.top() + line * qMax(1, lineHeight), tileSide, tileSide);
}

int TierRowGrid::requiredRows(int itemCount) const {
    return qMax(1, (qMax(0, itemCount) + qMax(1, columns) - 1) / qMax(1, columns));
}

int TierRowGrid::insertionIndex(const QPoint& point, int itemCount) const {
    itemCount = qMax(0, itemCount);
    const int safeColumns = qMax(1, columns);
    const int totalSlots = itemCount + 1;
    const int maxLine = qMax(0, (totalSlots - 1) / safeColumns);
    const int line = qBound(0, (point.y() - contentRect.top()) / qMax(1, lineHeight), maxLine);

    const int step = qMax(1, tileSide + kTileSpacing);
    const int relativeX = point.x() - contentRect.left() - kTileMargin;
    int column = 0;
    if (relativeX > 0) {
        column = relativeX / step;
        if (relativeX - column * step > tileSide / 2) {
            ++column;
        }
    }
    column = qBound(0, column, safeColumns);
    return qBound(0, line * safeColumns + column, itemCount);
}

QVector<QRect> TierRowGrid::itemRects(const QVector<QSize>& sourceSizes,
                                      ImagePresentationMode mode) const {
    QVector<QRect> rects;
    rects.reserve(sourceSizes.size());
    if (sourceSizes.isEmpty()) {
        return rects;
    }

    const int availableWidth = qMax(1, contentRect.width() - kTileMargin * 2);
    const int leading = contentRect.left() + kTileMargin;
    const int trailing = leading + availableWidth;
    int x = leading;
    int y = contentRect.top();
    for (const QSize& sourceSize : sourceSizes) {
        int width = tileSide;
        int height = tileSide;
        if (mode == ImagePresentationMode::NoCrop) {
            const qreal aspect = sourceSize.isValid()
                                     ? static_cast<qreal>(sourceSize.width()) /
                                           qMax(1, sourceSize.height())
                                     : 1.0;
            width = qMax(1, qRound(lineHeight * qMax<qreal>(0.001, aspect)));
            height = lineHeight;

            // A pathological panorama can remain wider than the board even at a one-pixel line
            // height. Preserve its aspect ratio and center the shorter item in the shared line.
            if (width > availableWidth) {
                width = availableWidth;
                height = qMax(1, qRound(width / qMax<qreal>(0.001, aspect)));
            }
        }

        if (x > leading && x + width > trailing) {
            x = leading;
            y += qMax(1, lineHeight);
        }
        const int top = y + qMax(0, (lineHeight - height) / 2);
        rects.append(QRect(x, top, width, height));
        x += width + kTileSpacing;
    }
    return rects;
}

int TierRowGrid::requiredRows(const QVector<QSize>& sourceSizes,
                              ImagePresentationMode mode) const {
    if (sourceSizes.isEmpty()) {
        return 1;
    }
    const QVector<QRect> rects = itemRects(sourceSizes, mode);
    if (rects.isEmpty()) {
        return 1;
    }
    return qMax(1, (rects.constLast().top() - contentRect.top()) / qMax(1, lineHeight) + 1);
}

int TierRowGrid::insertionIndex(const QPoint& point, const QVector<QSize>& sourceSizes,
                                ImagePresentationMode mode) const {
    const QVector<QRect> rects = itemRects(sourceSizes, mode);
    if (rects.isEmpty()) {
        return 0;
    }

    const int requestedLine =
        qMax(0, (point.y() - contentRect.top()) / qMax(1, lineHeight));
    int firstInLine = -1;
    int lastInLine = -1;
    for (int index = 0; index < rects.size(); ++index) {
        const int line =
            qMax(0, (rects.at(index).top() - contentRect.top()) / qMax(1, lineHeight));
        if (line == requestedLine) {
            if (firstInLine < 0) {
                firstInLine = index;
            }
            lastInLine = index;
        } else if (line > requestedLine) {
            break;
        }
    }

    if (firstInLine < 0) {
        const int firstLine = qMax(
            0, (rects.constFirst().top() - contentRect.top()) / qMax(1, lineHeight));
        return requestedLine < firstLine ? 0 : static_cast<int>(rects.size());
    }
    for (int index = firstInLine; index <= lastInLine; ++index) {
        if (point.x() < rects.at(index).center().x()) {
            return index;
        }
    }
    return lastInLine + 1;
}

TierRowGrid TierListLayout::gridForRow(const QRect& rowRect, int rowUnits, int labelWidth) {
    rowUnits = qMax(1, rowUnits);
    labelWidth = qMax(0, labelWidth);
    const int lineHeight = qMax(1, rowRect.height() / rowUnits);
    const QRect contentRect(rowRect.left() + labelWidth + kContentLeadingBorder, rowRect.top(),
                            qMax(1, rowRect.width() - labelWidth - kContentLeadingBorder -
                                        kContentTrailingBorder),
                            rowRect.height());
    const int availableWidth = qMax(1, contentRect.width() - kTileMargin * 2);
    const int tileSide = qMax(1, qMin(tileSideForLineHeight(lineHeight), availableWidth));
    const int columns =
        qMax(1, (availableWidth + kTileSpacing) / (tileSide + kTileSpacing));
    return TierRowGrid{contentRect, lineHeight, tileSide, columns};
}

int TierListLayout::requiredRowUnits(int imageCount, int rowWidth, int lineHeight,
                                     int labelWidth) {
    const TierRowGrid grid = gridForRow(QRect(0, 0, qMax(1, rowWidth), qMax(1, lineHeight)), 1,
                                            labelWidth);
    return grid.requiredRows(imageCount);
}

TierBoardLayoutMetrics TierListLayout::fitBoard(const QVector<int>& imageCounts,
                                                 const QSize& viewportSize, int labelWidth) {
    QVector<QVector<QSize>> imageSizes;
    imageSizes.reserve(imageCounts.size());
    for (int count : imageCounts) {
        imageSizes.append(QVector<QSize>(qMax(0, count), QSize(1, 1)));
    }
    return fitBoard(imageSizes, viewportSize, labelWidth, ImagePresentationMode::Square);
}

TierBoardLayoutMetrics TierListLayout::fitBoard(const QVector<QVector<QSize>>& imageSizes,
                                                 const QSize& viewportSize, int labelWidth,
                                                 ImagePresentationMode mode) {
    TierBoardLayoutMetrics result;
    if (imageSizes.isEmpty()) {
        return result;
    }

    const int rowWidth = qMax(1, viewportSize.width());
    const int availableHeight = qMax(1, viewportSize.height());
    QVector<qreal> rowLoads;
    rowLoads.reserve(imageSizes.size());
    int imageCountTotal = 0;
    for (const QVector<QSize>& rowSizes : imageSizes) {
        imageCountTotal += qMax(1, static_cast<int>(rowSizes.size()));
        qreal load = 0.0;
        for (const QSize& size : rowSizes) {
            load += mode == ImagePresentationMode::NoCrop && size.isValid()
                        ? static_cast<qreal>(size.width()) / qMax(1, size.height())
                        : 1.0;
        }
        rowLoads.append(qMax<qreal>(1.0, load));
    }

    // No-crop rows may need more vertical units than their image count to keep a very wide image
    // inside the board. Searching at most one unit per pixel is bounded and remains cheap.
    const int maximumTotalUnits = qMax(imageCountTotal, availableHeight);
    const int minimumTotalUnits = static_cast<int>(imageSizes.size());
    for (int totalUnits = minimumTotalUnits; totalUnits <= maximumTotalUnits; ++totalUnits) {
        // A distributed row can receive one remainder pixel per unit, so use the ceiling as the
        // worst (largest) tile side when proving that every row fits.
        const int maximumLineHeight =
            qMax(1, (availableHeight + totalUnits - 1) / totalUnits);
        QVector<int> rowUnits;
        rowUnits.reserve(imageSizes.size());
        int requiredTotalUnits = 0;
        for (int row = 0; row < imageSizes.size(); ++row) {
            const TierRowGrid grid = gridForRow(
                QRect(0, 0, rowWidth, maximumLineHeight), 1, labelWidth);
            const int required = grid.requiredRows(imageSizes.at(row), mode);
            rowUnits.append(required);
            requiredTotalUnits += required;
        }
        if (requiredTotalUnits > totalUnits) {
            continue;
        }

        // Spare vertical units go to the densest rows. This keeps the board balanced without
        // changing the fit proof, because the total unit count and maximum line height stay fixed.
        while (requiredTotalUnits < totalUnits) {
            ++rowUnits[densestRow(rowLoads, rowUnits)];
            ++requiredTotalUnits;
        }
        result.rowUnits = std::move(rowUnits);
        result.rowHeights = distributeRowHeights(result.rowUnits, availableHeight);
        return result;
    }

    result.rowUnits = QVector<int>(imageSizes.size(), 1);
    result.rowHeights = distributeRowHeights(result.rowUnits, availableHeight);
    return result;
}

MissionControlLayoutMetrics TierListLayout::fitMissionControl(
    const QVector<QSizeF>& sourceSizes, const QRectF& bounds, qreal gap) {
    MissionControlLayoutMetrics result;
    if (sourceSizes.isEmpty() || !bounds.isValid() || bounds.isEmpty()) {
        return result;
    }

    QVector<QSizeF> normalizedSizes;
    normalizedSizes.reserve(sourceSizes.size());
    for (const QSizeF& size : sourceSizes) {
        normalizedSizes.append(normalizedSourceSize(size));
    }

    const MissionPackingCandidate packing =
        balancedMissionPacking(normalizedSizes, bounds, qMax<qreal>(0.0, gap));
    if (!packing.isValid() || packing.placements.size() != normalizedSizes.size()) {
        return result;
    }

    result.itemRects.resize(normalizedSizes.size());
    for (const MissionPlacement& placement : packing.placements) {
        if (placement.inputIndex < 0 || placement.inputIndex >= result.itemRects.size()) {
            return {};
        }
        result.itemRects[placement.inputIndex] = placement.imageRect;
    }
    result.scale = packing.scale;
    result.imageAreaOccupancy = packing.imageAreaOccupancy;
    result.horizontalOccupancy = packing.horizontalOccupancy;
    result.verticalOccupancy = packing.verticalOccupancy;
    return result;
}

MissionControlHoverLayoutMetrics
TierListLayout::applyMissionControlHover(const QVector<QRectF>& baseRects, int hoverIndex,
                                         const QRectF& hoverTarget, const QRectF& movementBounds,
                                         qreal easedProgress, qreal gap) {
    MissionControlHoverLayoutMetrics result;
    result.itemRects = baseRects;
    if (hoverIndex < 0 || hoverIndex >= baseRects.size() || !hoverTarget.isValid() ||
        hoverTarget.isEmpty() || !movementBounds.isValid() || movementBounds.isEmpty() ||
        easedProgress <= 0.001) {
        return result;
    }

    easedProgress = qBound<qreal>(0.0, easedProgress, 1.0);
    gap = qMax<qreal>(0.0, gap);

    const QRectF requestedHoverRect =
        interpolatedRect(baseRects.at(hoverIndex), hoverTarget, easedProgress);
    if (!rectContainsRect(movementBounds, requestedHoverRect)) {
        result.appliedProgress = easedProgress;
        result.constrained = true;
        return result;
    }

    const QVector<int> localNeighbors =
        localHoverNeighbors(baseRects, hoverIndex, requestedHoverRect, gap);
    result.localNeighborCount = localNeighbors.size();
    QVector<char> active(baseRects.size(), 0);
    for (int index : localNeighbors) {
        active[index] = 1;
    }

    constexpr qreal kMaximumNeighborShrink = 0.42;
    const qreal projectionScale =
        qBound<qreal>(1.0 - kMaximumNeighborShrink, 1.0 - kMaximumNeighborShrink * easedProgress,
                      1.0);
    LocalHoverProjection projected =
        shrinkLocalHoverNeighbors(baseRects, hoverIndex, requestedHoverRect, movementBounds,
                                  projectionScale, gap, active);
    if (projected.rects.isEmpty()) {
        projected =
            projectLocalHoverNeighbors(baseRects, hoverIndex, requestedHoverRect, movementBounds,
                                       projectionScale, gap, active);
    }
    maximizeProjectedHoverNeighborSizes(&projected, baseRects, hoverIndex, movementBounds, gap);
    QVector<QRectF> solvedRects = std::move(projected.rects);
    result.affectedNeighborCount = projected.activeNeighbors.size();

    const bool layoutResolved = !solvedRects.isEmpty();
    if (layoutResolved) {
        result.itemRects = std::move(solvedRects);
    } else {
        // The hovered tile remains the hard interaction invariant. A constrained result is
        // observable in tests instead of silently weakening the requested magnification.
        result.itemRects[hoverIndex] = requestedHoverRect;
    }
    result.appliedProgress = easedProgress;
    result.constrained = !layoutResolved;
    for (int index = 0; index < baseRects.size(); ++index) {
        if (index == hoverIndex) {
            continue;
        }
        const QRectF displayed = result.itemRects.at(index);
        const QRectF base = baseRects.at(index);
        if (qAbs(displayed.x() - base.x()) > 0.01 || qAbs(displayed.y() - base.y()) > 0.01 ||
            qAbs(displayed.width() - base.width()) > 0.01 ||
            qAbs(displayed.height() - base.height()) > 0.01) {
            ++result.changedNeighborCount;
        }
    }
    return result;
}

} // namespace qtm
