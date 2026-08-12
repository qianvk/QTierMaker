#include "widgets/TransferProgressWidget.h"

#include <QHideEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QShowEvent>
#include <QTimeLine>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

#include <vkui/core/VkMotion.h>

namespace qtm {

class TransferProgressTrack final : public QWidget {
public:
    explicit TransferProgressTrack(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(8);
        setMinimumWidth(240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_activityAnimation.setDuration(1300);
        m_activityAnimation.setUpdateInterval(33);
        m_activityAnimation.setLoopCount(0);
        m_activityAnimation.setEasingCurve(QEasingCurve::Linear);
        connect(&m_activityAnimation, &QTimeLine::valueChanged, this, [this](qreal phase) {
            m_activityPhase = phase;
            update();
        });
        connect(&m_valueAnimation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& value) {
                    m_displayedProgress = value.toReal();
                    update();
                });
    }

    void setProgress(qreal progress) {
        progress = std::clamp(progress, 0.0, 1.0);
        m_indeterminate = false;
        m_activityAnimation.stop();
        m_targetProgress = progress;

        const vkui::VkMotionSpec motion = vkui::motionSpec(vkui::VkMotionRole::StateTransition);
        if (!isVisible() || motion.durationMs <= 0 ||
            std::abs(m_displayedProgress - progress) < 0.001) {
            m_valueAnimation.stop();
            m_displayedProgress = progress;
            update();
            return;
        }

        m_valueAnimation.stop();
        m_valueAnimation.setStartValue(m_displayedProgress);
        m_valueAnimation.setEndValue(progress);
        m_valueAnimation.setDuration(motion.durationMs);
        m_valueAnimation.setEasingCurve(motion.easing);
        m_valueAnimation.start();
    }

    void setIndeterminate() {
        m_valueAnimation.stop();
        m_indeterminate = true;
        if (isVisible() && m_activityAnimation.state() != QTimeLine::Running) {
            m_activityAnimation.start();
        }
        update();
    }

    void reset() {
        m_activityAnimation.stop();
        m_valueAnimation.stop();
        m_activityPhase = 0.0;
        m_displayedProgress = 0.0;
        m_targetProgress = 0.0;
        m_indeterminate = false;
        update();
    }

    [[nodiscard]] bool isIndeterminate() const {
        return m_indeterminate;
    }

    [[nodiscard]] int percentage() const {
        return qRound(m_targetProgress * 100.0);
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        if (m_indeterminate && m_activityAnimation.state() != QTimeLine::Running) {
            m_activityAnimation.start();
        }
    }

    void hideEvent(QHideEvent* event) override {
        m_activityAnimation.stop();
        m_valueAnimation.stop();
        m_displayedProgress = m_targetProgress;
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent*) override {
        if (width() <= 0 || height() <= 0) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        const QRectF track(0.5, 1.0, width() - 1.0, height() - 2.0);
        const qreal radius = track.height() * 0.5;
        QColor trackColor = palette().color(QPalette::Mid);
        const bool dark = palette().color(QPalette::Window).lightnessF() < 0.5;
        trackColor.setAlphaF(dark ? 0.34F : 0.22F);
        painter.setBrush(trackColor);
        painter.drawRoundedRect(track, radius, radius);

        QColor accent = palette().color(QPalette::Highlight);
        accent.setAlphaF(0.96F);
        painter.setClipPath(roundedRectPath(track, radius));

        if (m_indeterminate) {
            const qreal segmentWidth = std::max(track.height() * 4.0, track.width() * 0.28);
            const qreal travel = track.width() + segmentWidth * 2.0;
            const qreal left = track.left() - segmentWidth + travel * m_activityPhase;
            const QRectF segment(left, track.top(), segmentWidth, track.height());
            QLinearGradient glow(segment.left(), 0.0, segment.right(), 0.0);
            QColor edge = accent;
            edge.setAlphaF(0.12F);
            glow.setColorAt(0.0, edge);
            glow.setColorAt(0.5, accent);
            glow.setColorAt(1.0, edge);
            painter.setBrush(glow);
            painter.drawRoundedRect(segment, radius, radius);
            return;
        }

        const qreal fillWidth = track.width() * m_displayedProgress;
        if (fillWidth > 0.0) {
            const QRectF fill(track.left(), track.top(), fillWidth, track.height());
            const qreal fillRadius = std::min(radius, fillWidth * 0.5);
            painter.setBrush(accent);
            painter.drawRoundedRect(fill, fillRadius, fillRadius);
        }
    }

private:
    static QPainterPath roundedRectPath(const QRectF& rect, qreal radius) {
        QPainterPath path;
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    QTimeLine m_activityAnimation;
    QVariantAnimation m_valueAnimation;
    qreal m_activityPhase{0.0};
    qreal m_displayedProgress{0.0};
    qreal m_targetProgress{0.0};
    bool m_indeterminate{false};
};

TransferProgressWidget::TransferProgressWidget(QWidget* parent) : QWidget(parent) {
    setMaximumWidth(360);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(7);

    m_status = new QLabel(this);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setContentsMargins(4, 2, 4, 2);
    m_status->setStyleSheet(QStringLiteral("color: palette(mid);"));
    m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_track = new TransferProgressTrack(this);
    layout->addWidget(m_status);
    layout->addWidget(m_track);
}

void TransferProgressWidget::setStatusText(const QString& text) {
    m_status->setText(text);
}

void TransferProgressWidget::setProgress(qint64 received, qint64 total) {
    if (total <= 0) {
        m_track->setIndeterminate();
        return;
    }
    m_track->setProgress(
        std::clamp(static_cast<qreal>(received) / static_cast<qreal>(total), 0.0, 1.0));
}

void TransferProgressWidget::setIndeterminate() {
    m_track->setIndeterminate();
}

void TransferProgressWidget::reset() {
    m_status->clear();
    m_track->reset();
}

bool TransferProgressWidget::isIndeterminate() const {
    return m_track->isIndeterminate();
}

int TransferProgressWidget::percentage() const {
    return m_track->percentage();
}

} // namespace qtm
