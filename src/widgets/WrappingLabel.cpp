#include "widgets/WrappingLabel.h"

#include <QEvent>
#include <QFontMetrics>
#include <QResizeEvent>

#include <algorithm>
#include <limits>

namespace qtm {

WrappingLabel::WrappingLabel(QWidget* parent) : QLabel(parent) {
    setWordWrap(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

WrappingLabel::WrappingLabel(const QString& text, QWidget* parent) : WrappingLabel(parent) {
    setText(text);
}

void WrappingLabel::setText(const QString& text) {
    QLabel::setText(text);
    updateRequiredHeight();
}

bool WrappingLabel::hasHeightForWidth() const {
    return true;
}

int WrappingLabel::heightForWidth(int width) const {
    const QMargins margins = contentsMargins();
    const int frameInsets = frameWidth() * 2;
    const int horizontalInsets = margins.left() + margins.right() + frameInsets + margin() * 2;
    const int verticalInsets = margins.top() + margins.bottom() + frameInsets + margin() * 2;
    const int textWidth = std::max(1, width - horizontalInsets);
    const int flags = alignment() | Qt::TextWordWrap;
    const QRect bounds = fontMetrics().boundingRect(
        QRect(0, 0, textWidth, std::numeric_limits<int>::max()), flags, text());
    return std::max(fontMetrics().height(), bounds.height()) + verticalInsets;
}

QSize WrappingLabel::sizeHint() const {
    QSize hint = QLabel::sizeHint();
    hint.setHeight(heightForWidth(std::max(1, hint.width())));
    return hint;
}

QSize WrappingLabel::minimumSizeHint() const {
    return QSize(0, heightForWidth(std::max(1, width())));
}

void WrappingLabel::changeEvent(QEvent* event) {
    QLabel::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::ContentsRectChange:
        updateRequiredHeight();
        break;
    default:
        break;
    }
}

void WrappingLabel::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    updateRequiredHeight();
}

void WrappingLabel::updateRequiredHeight() {
    const int requiredHeight = heightForWidth(std::max(1, width()));
    if (minimumHeight() != requiredHeight) {
        setMinimumHeight(requiredHeight);
    }
    updateGeometry();
}

} // namespace qtm
