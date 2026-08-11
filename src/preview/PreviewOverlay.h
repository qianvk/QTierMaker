#pragma once

#include "assets/BackdropMaterial.h"
#include "assets/CoverImageCache.h"

#include <QEvent>
#include <QImage>
#include <QPixmap>
#include <QWidget>

class QParallelAnimationGroup;
class QPropertyAnimation;

namespace qtm {

enum class PreviewBackgroundMode;
enum class PreviewBackgroundTreatment { SolidColor, DepthSoftFocus, ProjectImage };

/** Window-level animated image preview that owns input while it is visible. */
class PreviewOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QRect previewGeometry READ previewGeometry WRITE setPreviewGeometry)
    Q_PROPERTY(qreal backdropProgress READ backdropProgress WRITE setBackdropProgress)

public:
    explicit PreviewOverlay(QWidget* parent = nullptr);
    ~PreviewOverlay() override;

    QRect previewGeometry() const {
        return m_previewGeometry;
    }
    void setPreviewGeometry(const QRect& rect);

    qreal backdropProgress() const {
        return m_backdropProgress;
    }
    void setBackdropProgress(qreal progress);

    PreviewBackgroundMode backgroundMode() const {
        return m_backgroundMode;
    }
    void setBackgroundMode(PreviewBackgroundMode mode);

    BackdropEffect previewEffect() const {
        return m_previewEffect;
    }
    void setPreviewEffect(BackdropEffect effect);
    const LiquidGlassParameters& liquidGlassParameters() const {
        return m_liquidGlassParameters;
    }
    void setLiquidGlassParameters(const LiquidGlassParameters& parameters);
    qreal liquidGlassScale() const {
        return m_liquidGlassScale;
    }
    void adjustLiquidGlassScale(int steps);

    static PreviewBackgroundTreatment
    resolveBackgroundTreatment(BackdropEffect effect, PreviewBackgroundMode backgroundMode,
                               bool hasPreviewImage, bool hasProjectBackground);
    PreviewBackgroundTreatment backgroundTreatment() const;

    bool toolTipsEnabled() const {
        return m_toolTipsEnabled;
    }
    void setToolTipsEnabled(bool enabled);

    bool isOpen() const {
        return m_open;
    }
    Q_INVOKABLE QString toolTipTextAt(QPoint position) const;
    void openPreview(const QRect& sourceRectInWindow, const QPixmap& pixmap,
                     const QString& projectBackgroundPath = {},
                     qreal projectBackgroundVisibility = 1.0);
    void closePreview();
    void resetPreview();

signals:
    void opened();
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRect targetRectForPixmap(const QPixmap& pixmap) const;
    void animateTo(const QRect& from, const QRect& to, qreal fromProgress, qreal toProgress,
                   bool closing);
    void clearPreviewState(bool notifyClosed);
    QRectF glassLayerRect(const QRectF& imageRect) const;
    void rebuildMaterialCaches();
    void rebuildBackgroundCache();
    void rebuildGlassOverlayCache();
    void setInputBarrierActive(bool active);
    void handleLeakedPointerEvent(QEvent* event);
    void requestCloseForPointer(QEvent::Type type, Qt::MouseButton button,
                                const QPoint& globalPosition);
    bool isOverlayDispatchObject(const QObject* object) const;

    QPixmap m_pixmap;
    QImage m_materialSource;
    QPixmap m_backgroundCache;
    QPixmap m_glassOverlayCache;
    BackdropMaterialCache m_backgroundMaterialCache;
    BackdropMaterialCache m_glassMaterialCache;
    CoverImageCache m_projectBackgroundCache;
    QString m_projectBackgroundPath;
    qreal m_projectBackgroundVisibility{1.0};
    qreal m_backgroundContentOpacity{1.0};
    qreal m_glassRefractionHeight{0.0};
    qreal m_glassOutset{0.0};
    LiquidGlassParameters m_liquidGlassParameters;
    qreal m_liquidGlassScale{1.0};
    QRect m_sourceGeometry;
    QRect m_previewGeometry;
    qreal m_backdropProgress{0.0};
    PreviewBackgroundMode m_backgroundMode;
    BackdropEffect m_previewEffect{BackdropEffect::DepthSoftFocus};
    QParallelAnimationGroup* m_animationGroup{nullptr};
    QPropertyAnimation* m_geometryAnimation{nullptr};
    QPropertyAnimation* m_backdropAnimation{nullptr};
    bool m_open{false};
    bool m_closing{false};
    bool m_inputBarrierActive{false};
    bool m_toolTipsEnabled{true};
};

} // namespace qtm
