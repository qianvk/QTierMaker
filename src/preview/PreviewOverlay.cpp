#include "preview/PreviewOverlay.h"

#include "logging/Logger.h"
#include "settings/AppSettings.h"
#include "theme/Theme.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShortcutEvent>
#include <QWindow>

#include <algorithm>

namespace tlm {

namespace {
constexpr int kImageCornerRadius = 16;
constexpr int kApertureMargin = 18;
constexpr int kMaximumShadowSpread = 10;
constexpr qreal kImageShoulderOverlapRatio = 0.20;
constexpr qreal kMinimumLiquidGlassScale = 0.5;
constexpr qreal kMaximumLiquidGlassScale = 1.5;
constexpr qreal kLiquidGlassScaleStep = 0.1;

struct PreviewGlassMetrics final {
    int refractionHeight{0};
    int imageOverlap{0};
    int outerOutset{0};
};

qreal previewImageCornerRadius(QSizeF imageSize) {
    return std::min<qreal>(
        kImageCornerRadius,
        std::max<qreal>(3.0, std::min(imageSize.width(), imageSize.height()) * 0.18));
}

qreal scaledPreviewImageCornerRadius(QSizeF imageSize, qreal surfaceScale) {
    surfaceScale = qMax<qreal>(0.01, surfaceScale);
    return previewImageCornerRadius(imageSize / surfaceScale) * surfaceScale;
}

PreviewGlassMetrics previewGlassMetrics(QSizeF imageSize, qreal surfaceScale = 1.0) {
    const int refractionHeight =
        qCeil(scaledPreviewImageCornerRadius(imageSize, surfaceScale));
    const int imageOverlap =
        qBound(1, qRound(refractionHeight * kImageShoulderOverlapRatio), refractionHeight - 1);
    return {refractionHeight, imageOverlap, refractionHeight - imageOverlap};
}

QStringView backgroundModeName(PreviewBackgroundMode mode) {
    return mode == PreviewBackgroundMode::SelfImage ? u"self-image" : u"none";
}

QStringView backdropEffectName(BackdropEffect effect) {
    return effect == BackdropEffect::LiquidGlass ? u"liquid-glass" : u"depth-soft-focus";
}

bool isBlockingInputEvent(QEvent::Type type) {
    switch (type) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::ContextMenu:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Shortcut:
    case QEvent::ShortcutOverride:
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
    case QEvent::Drop:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::NativeGesture:
        return true;
    default:
        return false;
    }
}
} // namespace

PreviewOverlay::PreviewOverlay(QWidget* parent)
    : QWidget(parent), m_backgroundMode(PreviewBackgroundMode::None),
      m_animationGroup(new QParallelAnimationGroup(this)),
      m_geometryAnimation(new QPropertyAnimation(this, "previewGeometry", m_animationGroup)),
      m_backdropAnimation(new QPropertyAnimation(this, "backdropProgress", m_animationGroup)) {
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setToolTip(tr("Click outside to close. Double-click image to close."));
    setProperty("tlmToolTipProvider", QVariant::fromValue(static_cast<QObject*>(this)));
    setProperty("tlmToolTipsEnabled", true);

    m_animationGroup->addAnimation(m_geometryAnimation);
    m_animationGroup->addAnimation(m_backdropAnimation);
    connect(m_animationGroup, &QParallelAnimationGroup::finished, this, [this]() {
        if (!m_closing) {
            return;
        }
        m_closing = false;
        m_open = false;
        hide();
        setInputBarrierActive(false);
        m_pixmap = {};
        m_materialSource = {};
        m_backgroundCache = {};
        m_glassOverlayCache = {};
        m_projectBackgroundPath.clear();
        m_projectBackgroundVisibility = 1.0;
        m_backgroundContentOpacity = 1.0;
        m_glassRefractionHeight = 0.0;
        m_glassOutset = 0.0;
        m_backgroundMaterialCache.clear();
        m_glassMaterialCache.clear();
        emit closed();
    });
    hide();
}

PreviewOverlay::~PreviewOverlay() {
    setInputBarrierActive(false);
}

void PreviewOverlay::setPreviewGeometry(const QRect& rect) {
    if (m_previewGeometry == rect) {
        return;
    }
    const auto visualBounds = [this](const QRect& imageRect) {
        const QRectF surfaceRect = m_previewEffect == BackdropEffect::LiquidGlass
                                       ? glassLayerRect(imageRect)
                                       : QRectF(imageRect);
        return surfaceRect.adjusted(-kMaximumShadowSpread, -kMaximumShadowSpread,
                                    kMaximumShadowSpread, kMaximumShadowSpread);
    };
    const QRect dirty = visualBounds(m_previewGeometry).united(visualBounds(rect)).toAlignedRect();
    m_previewGeometry = rect;
    update(dirty.intersected(this->rect()));
}

void PreviewOverlay::setBackdropProgress(qreal progress) {
    progress = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_backdropProgress, progress)) {
        return;
    }
    m_backdropProgress = progress;
    update();
}

void PreviewOverlay::setBackgroundMode(PreviewBackgroundMode mode) {
    if (m_backgroundMode == mode) {
        return;
    }
    m_backgroundMode = mode;
    rebuildBackgroundCache();
    update();
    Logger::info(
        QStringLiteral("ui.preview.background mode=%1").arg(backgroundModeName(mode).toString()));
}

void PreviewOverlay::setPreviewEffect(BackdropEffect effect) {
    if (m_previewEffect == effect) {
        return;
    }
    m_previewEffect = effect;
    m_glassMaterialCache.clear();
    rebuildMaterialCaches();
    update();
    Logger::info(
        QStringLiteral("ui.preview.effect value=%1").arg(backdropEffectName(effect).toString()));
}

void PreviewOverlay::setLiquidGlassParameters(const LiquidGlassParameters& parameters) {
    const LiquidGlassParameters normalized = normalizedLiquidGlassParameters(parameters);
    if (m_liquidGlassParameters == normalized) {
        return;
    }
    m_liquidGlassParameters = normalized;
    m_glassMaterialCache.clear();
    rebuildGlassOverlayCache();
    update();
}

void PreviewOverlay::adjustLiquidGlassScale(int steps) {
    if (steps == 0 || m_previewEffect != BackdropEffect::LiquidGlass || m_closing) {
        return;
    }
    const qreal scale =
        qBound(kMinimumLiquidGlassScale, m_liquidGlassScale + steps * kLiquidGlassScaleStep,
               kMaximumLiquidGlassScale);
    if (qFuzzyCompare(m_liquidGlassScale + 1.0, scale + 1.0)) {
        return;
    }

    m_liquidGlassScale = scale;
    if (!m_open || m_pixmap.isNull()) {
        return;
    }

    // AndroidLiquidGlass scales the complete layer. Rebuild once for the new target while
    // preserving the image/glass relationship and the current backdrop transition.
    const bool continueOpening = m_backdropProgress < 0.999;
    m_animationGroup->stop();
    rebuildGlassOverlayCache();
    const QRect target = targetRectForPixmap(m_pixmap);
    if (continueOpening) {
        animateTo(m_previewGeometry, target, m_backdropProgress, 1.0, false);
    } else {
        setPreviewGeometry(target);
    }
    Logger::debug(
        QStringLiteral("ui.preview.glass.scale value=%1").arg(m_liquidGlassScale, 0, 'f', 1));
}

PreviewBackgroundTreatment
PreviewOverlay::resolveBackgroundTreatment(BackdropEffect effect,
                                           PreviewBackgroundMode backgroundMode,
                                           bool hasPreviewImage, bool hasProjectBackground) {
    if (effect == BackdropEffect::LiquidGlass) {
        return hasProjectBackground ? PreviewBackgroundTreatment::ProjectImage
                                    : PreviewBackgroundTreatment::SolidColor;
    }
    if (backgroundMode == PreviewBackgroundMode::SelfImage && hasPreviewImage) {
        return PreviewBackgroundTreatment::DepthSoftFocus;
    }
    return PreviewBackgroundTreatment::SolidColor;
}

PreviewBackgroundTreatment PreviewOverlay::backgroundTreatment() const {
    return resolveBackgroundTreatment(m_previewEffect, m_backgroundMode, !m_materialSource.isNull(),
                                      !m_projectBackgroundPath.isEmpty());
}

void PreviewOverlay::setToolTipsEnabled(bool enabled) {
    if (m_toolTipsEnabled == enabled) {
        return;
    }
    m_toolTipsEnabled = enabled;
    setProperty("tlmToolTipsEnabled", enabled);
    setToolTip(enabled ? tr("Click outside to close. Double-click image to close.") : QString());
}

QString PreviewOverlay::toolTipTextAt(QPoint position) const {
    if (!m_toolTipsEnabled) {
        return {};
    }
    return m_previewGeometry.contains(position) ? tr("Double-click image to close")
                                                : tr("Click to close preview");
}

void PreviewOverlay::openPreview(const QRect& sourceRectInWindow, const QPixmap& pixmap,
                                 const QString& projectBackgroundPath,
                                 qreal projectBackgroundVisibility) {
    if (pixmap.isNull()) {
        return;
    }

    m_animationGroup->stop();
    m_liquidGlassScale = 1.0;
    m_pixmap = pixmap;
    m_materialSource = pixmap.toImage();
    m_projectBackgroundPath = projectBackgroundPath;
    m_projectBackgroundVisibility = qBound<qreal>(0.0, projectBackgroundVisibility, 1.0);
    setGeometry(parentWidget() ? parentWidget()->rect() : geometry());
    const QPoint sourceTopLeft =
        window() ? mapFrom(window(), sourceRectInWindow.topLeft()) : sourceRectInWindow.topLeft();
    m_sourceGeometry = QRect(sourceTopLeft, sourceRectInWindow.size());
    if (!m_sourceGeometry.isValid()) {
        m_sourceGeometry = QRect(rect().center() - QPoint(20, 20), QSize(40, 40));
    }
    rebuildMaterialCaches();

    const bool wasOpen = m_open;
    m_open = true;
    m_closing = false;
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    setInputBarrierActive(true);
    if (!wasOpen) {
        setPreviewGeometry(m_sourceGeometry);
        setBackdropProgress(0.0);
        emit opened();
    }
    const QRect target = targetRectForPixmap(m_pixmap);
    Logger::info(QStringLiteral("ui.preview.open mode=%1 effect=%2 source=(%3,%4,%5,%6) "
                                "target=(%7,%8,%9,%10) projectBackground=%11")
                     .arg(backgroundModeName(m_backgroundMode).toString())
                     .arg(backdropEffectName(m_previewEffect).toString())
                     .arg(m_sourceGeometry.x())
                     .arg(m_sourceGeometry.y())
                     .arg(m_sourceGeometry.width())
                     .arg(m_sourceGeometry.height())
                     .arg(target.x())
                     .arg(target.y())
                     .arg(target.width())
                     .arg(target.height())
                     .arg(!m_projectBackgroundPath.isEmpty()));
    animateTo(m_previewGeometry, target, m_backdropProgress, 1.0, false);
}

void PreviewOverlay::closePreview() {
    if (!m_open || m_closing) {
        return;
    }
    Logger::info(QStringLiteral("ui.preview.close target=(%1,%2,%3,%4)")
                     .arg(m_sourceGeometry.x())
                     .arg(m_sourceGeometry.y())
                     .arg(m_sourceGeometry.width())
                     .arg(m_sourceGeometry.height()));
    m_animationGroup->stop();
    animateTo(m_previewGeometry, m_sourceGeometry, m_backdropProgress, 0.0, true);
}

bool PreviewOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (!m_open || !event || !isBlockingInputEvent(event->type()) ||
        isOverlayDispatchObject(watched)) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Shortcut) {
        auto* shortcutEvent = static_cast<QShortcutEvent*>(event);
        const QKeySequence key = shortcutEvent->key();
        if (key.matches(QKeySequence(Qt::Key_Escape)) == QKeySequence::ExactMatch ||
            key.matches(QKeySequence(Qt::Key_Space)) == QKeySequence::ExactMatch) {
            closePreview();
        } else if (key.matches(QKeySequence(Qt::Key_Plus)) == QKeySequence::ExactMatch) {
            adjustLiquidGlassScale(1);
        } else if (key.matches(QKeySequence(Qt::Key_Minus)) == QKeySequence::ExactMatch) {
            adjustLiquidGlassScale(-1);
        }
    } else if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape || keyEvent->key() == Qt::Key_Space) {
            closePreview();
        } else if (keyEvent->key() == Qt::Key_Plus) {
            adjustLiquidGlassScale(1);
        } else if (keyEvent->key() == Qt::Key_Minus) {
            adjustLiquidGlassScale(-1);
        }
    }

    event->accept();
    return true;
}

void PreviewOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const bool liquidGlass = m_previewEffect == BackdropEffect::LiquidGlass;

    if (!m_backgroundCache.isNull()) {
        painter.save();
        painter.setOpacity(m_backdropProgress * m_backgroundContentOpacity);
        painter.drawPixmap(rect(), m_backgroundCache);
        painter.restore();

        if (!liquidGlass) {
            painter.fillRect(rect(), QColor(0, 0, 0, qRound(72.0 * m_backdropProgress)));
            QPainterPath outsideAperture;
            outsideAperture.setFillRule(Qt::OddEvenFill);
            outsideAperture.addRect(rect());
            const QRectF aperture =
                QRectF(m_previewGeometry)
                    .adjusted(-kApertureMargin, -kApertureMargin, kApertureMargin, kApertureMargin);
            outsideAperture.addRoundedRect(aperture, kImageCornerRadius + kApertureMargin,
                                           kImageCornerRadius + kApertureMargin);
            painter.fillPath(outsideAperture, QColor(0, 0, 0, qRound(70.0 * m_backdropProgress)));
        }
    } else if (!liquidGlass) {
        painter.fillRect(rect(), QColor(0, 0, 0, qRound(87.0 * m_backdropProgress)));
    }

    if (m_pixmap.isNull() || m_previewGeometry.isEmpty()) {
        return;
    }

    const QRectF imageRect(m_previewGeometry);
    const QRectF surfaceRect = liquidGlass ? glassLayerRect(imageRect) : imageRect;
    const qreal radius =
        liquidGlass ? scaledPreviewImageCornerRadius(m_previewGeometry.size(),
                                                     m_liquidGlassScale)
                    : previewImageCornerRadius(m_previewGeometry.size());
    const qreal glassOutset = qMax<qreal>(0.0, (surfaceRect.width() - imageRect.width()) * 0.5);
    const qreal surfaceRadius = radius + glassOutset;
    QColor shadow = activeThemeTokens().shadow;
    for (int spread = kMaximumShadowSpread; spread >= 2; spread -= 2) {
        const QRectF shadowRect = surfaceRect.adjusted(-spread, -spread, spread, spread);
        shadow.setAlpha(qRound((12.0 - spread) * 2.2 * m_backdropProgress));
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(shadowRect, surfaceRadius + spread, surfaceRadius + spread);
        painter.fillPath(shadowPath, shadow);
    }

    QPainterPath imagePath;
    imagePath.addRoundedRect(m_previewGeometry, radius, radius);
    painter.save();
    painter.setClipPath(imagePath);
    painter.fillRect(m_previewGeometry, activeThemeTokens().elevatedBackground);
    painter.drawPixmap(m_previewGeometry, m_pixmap);
    painter.restore();

    if (liquidGlass && !m_glassOverlayCache.isNull()) {
        QPainterPath glassSurface;
        glassSurface.addRoundedRect(surfaceRect, surfaceRadius, surfaceRadius);

        // Materialize the cached lens over the unchanged image without regenerating the
        // seven-sample refraction during animation.
        painter.save();
        painter.setClipPath(glassSurface);
        painter.setOpacity(m_backdropProgress);
        painter.drawPixmap(surfaceRect.toAlignedRect(), m_glassOverlayCache);
        painter.restore();
    }
}

void PreviewOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_previewGeometry.contains(event->pos())) {
        Logger::info(QStringLiteral("ui.preview.close.request source=outside-click pos=(%1,%2)")
                         .arg(event->position().x())
                         .arg(event->position().y()));
        closePreview();
    }
    event->accept();
}

void PreviewOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (!m_toolTipsEnabled) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QString hint = toolTipTextAt(event->pos());
    if (toolTip() != hint) {
        setToolTip(hint);
    }
    QWidget::mouseMoveEvent(event);
}

void PreviewOverlay::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_previewGeometry.contains(event->pos())) {
        Logger::info(QStringLiteral("ui.preview.close.request source=image-double-click "
                                    "pos=(%1,%2)")
                         .arg(event->position().x())
                         .arg(event->position().y()));
        closePreview();
    }
    event->accept();
}

void PreviewOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space) {
        closePreview();
    } else if (event->key() == Qt::Key_Plus) {
        adjustLiquidGlassScale(1);
    } else if (event->key() == Qt::Key_Minus) {
        adjustLiquidGlassScale(-1);
    }
    event->accept();
}

void PreviewOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuildMaterialCaches();
    if (m_open && !m_pixmap.isNull()) {
        const bool continueClosing = m_closing;
        m_animationGroup->stop();
        if (continueClosing) {
            animateTo(m_previewGeometry, m_sourceGeometry, m_backdropProgress, 0.0, true);
        } else {
            m_closing = false;
            setPreviewGeometry(targetRectForPixmap(m_pixmap));
        }
    }
}

QRect PreviewOverlay::targetRectForPixmap(const QPixmap& pixmap) const {
    const int shortestEdge = std::max(1, std::min(width(), height()));
    const int margin = std::clamp(qRound(shortestEdge * 0.08), 44, 84);
    const auto maximumImageSize = [this, margin](int glassOutset) {
        const int totalMargin = margin + glassOutset;
        return QSize(std::max(1, width() - totalMargin * 2),
                     std::max(1, height() - totalMargin * 2));
    };

    QSize scaled = pixmap.size().scaled(maximumImageSize(0), Qt::KeepAspectRatio);
    if (m_previewEffect == BackdropEffect::LiquidGlass) {
        // Find the smallest integer margin containing the part of the shoulder outside the
        // image. The remaining shoulder overlaps the image, so its flat glass region is smaller.
        const int maximumOutset = previewGlassMetrics(scaled).outerOutset;
        for (int candidateOutset = 0; candidateOutset <= maximumOutset; ++candidateOutset) {
            const QSize candidate =
                pixmap.size().scaled(maximumImageSize(candidateOutset), Qt::KeepAspectRatio);
            if (candidateOutset >= previewGlassMetrics(candidate).outerOutset) {
                scaled = candidate;
                break;
            }
        }
        scaled = QSize(qMax(1, qRound(scaled.width() * m_liquidGlassScale)),
                       qMax(1, qRound(scaled.height() * m_liquidGlassScale)));
    }
    return QRect(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
}

QRectF PreviewOverlay::glassLayerRect(const QRectF& imageRect) const {
    if (imageRect.isEmpty() || m_pixmap.isNull()) {
        return imageRect;
    }
    const QSize finalImageSize = targetRectForPixmap(m_pixmap).size();
    if (finalImageSize.isEmpty()) {
        return imageRect;
    }
    const qreal animationScale = qBound<qreal>(0.0,
                                               qMin(imageRect.width() / finalImageSize.width(),
                                                    imageRect.height() / finalImageSize.height()),
                                               1.0);
    const qreal finalGlassOutset =
        m_glassOutset > 0.0
            ? m_glassOutset
            : previewGlassMetrics(finalImageSize, m_liquidGlassScale).outerOutset;
    const qreal glassOutset = finalGlassOutset * animationScale;
    return imageRect.adjusted(-glassOutset, -glassOutset, glassOutset, glassOutset);
}

void PreviewOverlay::animateTo(const QRect& from, const QRect& to, qreal fromProgress,
                               qreal toProgress, bool closing) {
    m_closing = closing;
    const int geometryDuration = closing ? 220 : 280;
    const int backdropDuration = closing ? 190 : 240;
    const QEasingCurve easing = closing ? QEasingCurve::InOutCubic : QEasingCurve::OutQuart;

    m_geometryAnimation->setStartValue(from);
    m_geometryAnimation->setEndValue(to);
    m_geometryAnimation->setDuration(geometryDuration);
    m_geometryAnimation->setEasingCurve(easing);
    m_backdropAnimation->setStartValue(fromProgress);
    m_backdropAnimation->setEndValue(toProgress);
    m_backdropAnimation->setDuration(backdropDuration);
    m_backdropAnimation->setEasingCurve(easing);
    m_animationGroup->start();
}

void PreviewOverlay::rebuildMaterialCaches() {
    rebuildBackgroundCache();
    rebuildGlassOverlayCache();
}

void PreviewOverlay::rebuildBackgroundCache() {
    m_backgroundCache = {};
    m_backgroundContentOpacity = 1.0;
    if (size().isEmpty()) {
        m_backgroundMaterialCache.clear();
        return;
    }

    const PreviewBackgroundTreatment treatment = backgroundTreatment();
    if (treatment == PreviewBackgroundTreatment::ProjectImage) {
        // Project art remains optically sharp; only the preview image owns a glass surface.
        m_backgroundMaterialCache.clear();
        m_backgroundContentOpacity = m_projectBackgroundVisibility;
        m_backgroundCache =
            m_projectBackgroundCache.pixmap(m_projectBackgroundPath, size(), devicePixelRatioF());
        return;
    }
    if (treatment != PreviewBackgroundTreatment::DepthSoftFocus) {
        m_backgroundMaterialCache.clear();
        return;
    }

    const BackdropMaterialStyle style{activeThemeTokens().elevatedBackground, 1.0,
                                      activeThemeIsDark()};
    const QString sourceId = QStringLiteral("preview-background:%1").arg(m_pixmap.cacheKey());
    m_backgroundCache =
        m_backgroundMaterialCache.pixmap(BackdropEffect::DepthSoftFocus, sourceId, m_materialSource,
                                         style, size(), devicePixelRatioF());
}

void PreviewOverlay::rebuildGlassOverlayCache() {
    m_glassOverlayCache = {};
    m_glassRefractionHeight = 0.0;
    m_glassOutset = 0.0;
    if (m_previewEffect != BackdropEffect::LiquidGlass || m_materialSource.isNull() ||
        size().isEmpty()) {
        m_glassMaterialCache.clear();
        return;
    }

    const QRect imageRect = targetRectForPixmap(m_pixmap);
    if (imageRect.isEmpty()) {
        m_glassMaterialCache.clear();
        return;
    }
    const qreal imageRadius =
        scaledPreviewImageCornerRadius(imageRect.size(), m_liquidGlassScale);
    const PreviewGlassMetrics glass =
        previewGlassMetrics(imageRect.size(), m_liquidGlassScale);
    m_glassRefractionHeight = glass.refractionHeight;
    m_glassOutset = glass.outerOutset;

    const QRect glassRect = imageRect.adjusted(-glass.outerOutset, -glass.outerOutset,
                                               glass.outerOutset, glass.outerOutset);
    QImage layerSource(glassRect.size(), QImage::Format_RGBA8888);
    layerSource.fill(activeThemeTokens().contentBackground);

    QPainter sourcePainter(&layerSource);
    sourcePainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (!m_backgroundCache.isNull()) {
        sourcePainter.save();
        sourcePainter.setOpacity(m_backgroundContentOpacity);
        sourcePainter.drawPixmap(QRect(-glassRect.x(), -glassRect.y(), width(), height()),
                                 m_backgroundCache);
        sourcePainter.restore();
    }

    const QRect localImageRect(QPoint(glass.outerOutset, glass.outerOutset), imageRect.size());
    QPainterPath imagePath;
    imagePath.addRoundedRect(localImageRect, imageRadius, imageRadius);
    sourcePainter.setClipPath(imagePath);
    sourcePainter.fillRect(localImageRect, activeThemeTokens().elevatedBackground);
    sourcePainter.drawPixmap(localImageRect, m_pixmap);
    sourcePainter.end();

    const QSize layerSize = layerSource.size();
    const int minimumEdge = qMax(1, qMin(layerSize.width(), layerSize.height()));
    BackdropMaterialStyle style{activeThemeTokens().elevatedBackground, 1.0, activeThemeIsDark(),
                                BackdropMaterialPurpose::GlassOverlay};
    style.liquidGlass = m_liquidGlassParameters;
    style.depthEffect = true;
    style.drawHighlight = true;
    m_glassRefractionHeight =
        minimumEdge * m_liquidGlassParameters.refractionHeightFraction * 0.5;
    const QString sourceId = QStringLiteral("preview-glass:%1:%2:%3")
                                 .arg(m_pixmap.cacheKey())
                                 .arg(m_projectBackgroundPath)
                                 .arg(qRound(m_backgroundContentOpacity * 1000.0));
    m_glassOverlayCache = m_glassMaterialCache.pixmap(
        BackdropEffect::LiquidGlass, sourceId, layerSource, style, layerSize, devicePixelRatioF());
    Logger::debug(QStringLiteral("ui.preview.glass.cache size=(%1,%2) image=(%3,%4) "
                                 "refractionHeight=%5 refractionAmount=%6 "
                                 "outset=%7 overlap=%8 scale=%9")
                      .arg(layerSize.width())
                      .arg(layerSize.height())
                      .arg(imageRect.width())
                      .arg(imageRect.height())
                      .arg(m_glassRefractionHeight, 0, 'f', 3)
                      .arg(minimumEdge * m_liquidGlassParameters.refractionAmountFraction, 0, 'f',
                           3)
                      .arg(m_glassOutset, 0, 'f', 3)
                      .arg(glass.imageOverlap)
                      .arg(m_liquidGlassScale, 0, 'f', 1));
}

void PreviewOverlay::setInputBarrierActive(bool active) {
    if (m_inputBarrierActive == active) {
        return;
    }
    m_inputBarrierActive = active;
    if (!qApp) {
        return;
    }
    if (active) {
        qApp->installEventFilter(this);
        grabMouse();
        grabKeyboard();
    } else {
        if (QWidget::mouseGrabber() == this) {
            releaseMouse();
        }
        if (QWidget::keyboardGrabber() == this) {
            releaseKeyboard();
        }
        qApp->removeEventFilter(this);
    }
}

bool PreviewOverlay::isOverlayDispatchObject(const QObject* object) const {
    // Native pointer input reaches the top-level QWindow before Qt dispatches the translated
    // QWidget event. Blocking that first stage prevents this overlay from ever receiving real
    // mouse input, even though direct QTest delivery still appears to work.
    const QWidget* topLevel = window();
    if (topLevel && object == topLevel->windowHandle()) {
        return true;
    }
    for (const QObject* current = object; current; current = current->parent()) {
        if (current == this) {
            return true;
        }
    }
    return false;
}

} // namespace tlm
