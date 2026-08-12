#include "update/UpdateButton.h"

#include "theme/Theme.h"
#include "widgets/TransferProgressWidget.h"

#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

#include <vkui/core/VkIcon.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace qtm {

UpdateButton::UpdateButton(QWidget* parent) : QToolButton(parent), m_attentionAnimation(this) {
    setObjectName(QStringLiteral("UpdateButton"));
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(34, 34);
    setIconSize(QSize(18, 18));
    setAutoRaise(true);

    m_progressIndicator = new TransferProgressIndicator(this);
    m_progressIndicator->setObjectName(QStringLiteral("UpdateTransferProgressIndicator"));
    m_progressIndicator->hide();
    hide();

    m_attentionAnimation.setDuration(720);
    m_attentionAnimation.setStartValue(0.0);
    m_attentionAnimation.setEndValue(1.0);
    m_attentionAnimation.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_attentionAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                m_attention = value.toReal();
                update();
            });
    connect(&m_attentionAnimation, &QVariantAnimation::finished, this, [this]() {
        m_attention = 0.0;
        update();
    });
    refreshPresentation();
}

void UpdateButton::setUpdateState(UpdateState state) {
    if (m_state == state) {
        return;
    }
    const UpdateState previous = m_state;
    m_state = state;
    if (state == UpdateState::Downloading && previous != UpdateState::Downloading) {
        m_bytesReceived = 0;
        m_bytesTotal = -1;
    }
    const bool actionable = state == UpdateState::Available || state == UpdateState::Ready;
    const bool visible =
        actionable || state == UpdateState::Downloading || state == UpdateState::Installing;
    setVisible(visible);
    setEnabled(actionable);
    refreshPresentation();
    refreshProgressIndicator();
    if (state == UpdateState::Available && previous != UpdateState::Available) {
        playAttentionAnimation();
    }
    update();
}

void UpdateButton::setInstallText(const QString& text) {
    if (m_installText == text) {
        return;
    }
    m_installText = text;
    refreshPresentation();
}

void UpdateButton::setDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (m_bytesReceived == bytesReceived && m_bytesTotal == bytesTotal) {
        return;
    }
    m_bytesReceived = qMax<qint64>(0, bytesReceived);
    m_bytesTotal = bytesTotal;
    if (m_state == UpdateState::Downloading) {
        refreshProgressIndicator();
    }
}

void UpdateButton::setReducedMotion(bool reduced) {
    if (m_reducedMotion == reduced) {
        return;
    }
    m_reducedMotion = reduced;
    if (reduced) {
        m_attentionAnimation.stop();
        m_attention = 0.0;
        update();
    }
}

void UpdateButton::paintEvent(QPaintEvent* event) {
    QToolButton::paintEvent(event);
    if (m_attention <= 0.0) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor accent = activeThemeTokens().accent;
    accent.setAlphaF(
        static_cast<float>(0.18 + 0.48 * std::sin(m_attention * std::numbers::pi)));
    QPen pen(accent, 2.0, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(pen);
    const QRectF progressRect = QRectF(rect()).adjusted(3.0, 3.0, -3.0, -3.0);

    painter.drawEllipse(progressRect);
}

void UpdateButton::resizeEvent(QResizeEvent* event) {
    QToolButton::resizeEvent(event);
    if (m_progressIndicator) {
        constexpr int kHorizontalInset = 6;
        constexpr int kBottomInset = 2;
        m_progressIndicator->setGeometry(kHorizontalInset,
                                         height() - m_progressIndicator->height() - kBottomInset,
                                         std::max(0, width() - 2 * kHorizontalInset),
                                         m_progressIndicator->height());
        m_progressIndicator->raise();
    }
}

void UpdateButton::refreshPresentation() {
    const bool installAction = m_state == UpdateState::Ready || m_state == UpdateState::Installing;
    setToolButtonStyle(installAction ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    setText(installAction ? m_installText : QString());
    const int textWidth = installAction ? fontMetrics().horizontalAdvance(m_installText) : 0;
    const int width = installAction ? qBound(72, textWidth + iconSize().width() + 30, 116)
                                    : (m_state == UpdateState::Downloading ? 72 : 34);
    setFixedSize(width, 34);
    setAccessibleName(installAction ? m_installText : toolTip());
    setIcon(vkui::icon(installAction ? vkui::VkSymbol::Install : vkui::VkSymbol::Download,
                       vkui::VkIconRole::Accent));
    updateGeometry();
}

void UpdateButton::refreshProgressIndicator() {
    if (!m_progressIndicator) {
        return;
    }
    if (m_state != UpdateState::Downloading) {
        m_progressIndicator->hide();
        m_progressIndicator->reset();
        return;
    }

    m_progressIndicator->show();
    m_progressIndicator->setProgress(m_bytesReceived, m_bytesTotal);
    m_progressIndicator->raise();
}

void UpdateButton::playAttentionAnimation() {
    if (m_reducedMotion) {
        return;
    }
    m_attentionAnimation.stop();
    m_attentionAnimation.start();
}

} // namespace qtm
