#include "tier/TierRow.h"

#include <QUuid>

namespace qtm {

TierRow TierRow::makeDefault(QString label, QColor color, int order) {
    TierRow row;
    row.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    row.label = std::move(label);
    row.color = color;
    row.order = order;
    return row;
}

} // namespace qtm

