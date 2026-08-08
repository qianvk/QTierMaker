#include "assets/BackdropMaterial.h"

#include "assets/FocusBackdrop.h"
#include "assets/LiquidGlassBackdrop.h"

namespace tlm {

struct BackdropMaterialCache::Impl final {
    void selectEffect(BackdropEffect selectedEffect) {
        if (hasEffect && effect == selectedEffect) {
            return;
        }
        effect = selectedEffect;
        hasEffect = true;

        // A view presents one material at a time. Releasing the inactive raster bounds memory
        // when large high-DPI windows switch effects.
        if (effect == BackdropEffect::LiquidGlass) {
            focusCache.clear();
        } else {
            liquidGlassCache.clear();
        }
    }

    BackdropEffect effect{BackdropEffect::DepthSoftFocus};
    bool hasEffect{false};
    FocusBackdropCache focusCache;
    LiquidGlassBackdropCache liquidGlassCache;
};

BackdropMaterialCache::BackdropMaterialCache() : m_impl(std::make_unique<Impl>()) {}

BackdropMaterialCache::~BackdropMaterialCache() = default;

const QPixmap& BackdropMaterialCache::pixmap(BackdropEffect effect, const QString& sourceId,
                                             const QImage& source,
                                             const BackdropMaterialStyle& style, QSize logicalSize,
                                             qreal devicePixelRatio) {
    m_impl->selectEffect(effect);
    if (effect == BackdropEffect::LiquidGlass) {
        const bool glassOverlay = style.purpose == BackdropMaterialPurpose::GlassOverlay;
        LiquidGlassBackdropStyle liquidStyle;
        liquidStyle.baseColor = style.baseColor;
        liquidStyle.sourceVisibility = style.sourceVisibility;
        liquidStyle.contentTreatment = glassOverlay ? LiquidGlassContentTreatment::ClearSurface
                                                    : LiquidGlassContentTreatment::TonedBackdrop;
        liquidStyle.sourceLayout =
            glassOverlay ? LiquidGlassSourceLayout::Stretch : LiquidGlassSourceLayout::Cover;
        liquidStyle.parameters = style.liquidGlass;
        liquidStyle.depthEffect = style.depthEffect;
        liquidStyle.highlightStyle = style.drawHighlight ? LiquidGlassHighlightStyle::Plain
                                                         : LiquidGlassHighlightStyle::None;
        return m_impl->liquidGlassCache.pixmap(sourceId, source, liquidStyle, logicalSize,
                                               devicePixelRatio);
    }
    return m_impl->focusCache.pixmap(
        sourceId, source, {style.baseColor, style.sourceVisibility, style.darkAppearance},
        logicalSize, devicePixelRatio);
}

void BackdropMaterialCache::clear() {
    m_impl->focusCache.clear();
    m_impl->liquidGlassCache.clear();
    m_impl->hasEffect = false;
}

} // namespace tlm
