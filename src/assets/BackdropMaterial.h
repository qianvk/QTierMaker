#pragma once

#include "assets/BackdropEffect.h"
#include "assets/LiquidGlassBackdrop.h"

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>

#include <memory>

namespace tlm {

enum class BackdropMaterialPurpose { Background, GlassOverlay };

struct BackdropMaterialStyle final {
    QColor baseColor;
    qreal sourceVisibility{1.0};
    bool darkAppearance{false};
    BackdropMaterialPurpose purpose{BackdropMaterialPurpose::Background};
    LiquidGlassParameters liquidGlass;
    bool depthEffect{true};
    bool drawHighlight{false};
};

/**
 * Reusable facade for content-derived background materials. Callers select a semantic effect;
 * the facade owns backend selection and keeps only the active material cache alive.
 */
class BackdropMaterialCache final {
public:
    BackdropMaterialCache();
    ~BackdropMaterialCache();

    BackdropMaterialCache(const BackdropMaterialCache&) = delete;
    BackdropMaterialCache& operator=(const BackdropMaterialCache&) = delete;

    const QPixmap& pixmap(BackdropEffect effect, const QString& sourceId, const QImage& source,
                          const BackdropMaterialStyle& style, QSize logicalSize,
                          qreal devicePixelRatio);
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace tlm
