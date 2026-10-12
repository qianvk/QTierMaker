#pragma once

#include <QPainterPath>
#include <QRectF>
#include <algorithm>
#include <vkui/core/VkThemeManager.h>

namespace qtm {

inline qreal tierBoardCornerRadius() {
    return std::max<qreal>(0.0,
                           vkui::VkThemeManager::instance()->theme().metrics().windowCornerRadius);
}

inline QPainterPath tierBoardRowClipPath(const QRectF& bounds, qreal radius, bool firstRow = true,
                                         bool lastRow = true) {
    QPainterPath path;
    if (radius <= 0.0 || (!firstRow && !lastRow)) {
        path.addRect(bounds);
        return path;
    }
    path.addRoundedRect(bounds, radius, radius);
    QPainterPath squareMask;
    if (!firstRow) {
        squareMask.addRect(QRectF(bounds.left(), bounds.top(), bounds.width(), radius + 1));
    }
    if (!lastRow) {
        squareMask.addRect(
            QRectF(bounds.left(), bounds.bottom() - radius - 1, bounds.width(), radius + 1));
    }
    return path.united(squareMask);
}

} // namespace qtm
