#pragma once

/*
 * The rounded-rectangle lens and highlight semantics exposed here are a C++/Qt port of
 * AndroidLiquidGlass (Backdrop), Copyright 2025 Kyant, licensed under Apache-2.0.
 * This port is modified for cached CPU rasterization.
 */

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace qtm {

enum class LiquidGlassContentTreatment { TonedBackdrop, ClearSurface };
enum class LiquidGlassSourceLayout { Cover, Stretch };
enum class LiquidGlassHighlightStyle { None, Default, Plain, Ambient };

/**
 * User-facing AndroidLiquidGlass Playground parameters.
 *
 * The corner radius uses logical pixels so differently sized surfaces share a visual language.
 * Refraction values retain the reference implementation's dimension-relative fractions.
 */
struct LiquidGlassParameters final {
    qreal cornerRadius{16.0};
    qreal blurRadius{0.0};
    qreal refractionHeightFraction{0.2};
    qreal refractionAmountFraction{0.2};
    qreal chromaticAberration{0.0};
};

bool operator==(const LiquidGlassParameters& left, const LiquidGlassParameters& right);
bool operator!=(const LiquidGlassParameters& left, const LiquidGlassParameters& right);
LiquidGlassParameters normalizedLiquidGlassParameters(LiquidGlassParameters parameters);

/** Semantic parameters for a reusable refractive surface. */
struct LiquidGlassBackdropStyle final {
    QColor baseColor;
    qreal sourceVisibility{1.0};
    LiquidGlassContentTreatment contentTreatment{LiquidGlassContentTreatment::TonedBackdrop};
    LiquidGlassSourceLayout sourceLayout{LiquidGlassSourceLayout::Cover};
    LiquidGlassParameters parameters;
    bool depthEffect{true};
    LiquidGlassHighlightStyle highlightStyle{LiquidGlassHighlightStyle::Default};
};

/**
 * Applies AndroidLiquidGlass-compatible rounded-rectangle refraction to a cached source image.
 * Application-specific source layout and surface preparation happen before the ported lens pass.
 */
QImage createLiquidGlassBackdropImage(const QImage& source, const LiquidGlassBackdropStyle& style,
                                      QSize logicalSize);

/** Caches the expensive optical raster independently from its DPR-sized presentation pixmap. */
class LiquidGlassBackdropCache final {
public:
    const QPixmap& pixmap(const QString& sourceId, const QImage& source,
                          const LiquidGlassBackdropStyle& style, QSize logicalSize,
                          qreal devicePixelRatio);
    void clear();

private:
    QString m_sourceId;
    qint64 m_sourceCacheKey{0};
    LiquidGlassBackdropStyle m_style;
    bool m_hasStyle{false};
    QSize m_materialShape;
    qreal m_rasterBlurRadius{-1.0};
    QImage m_material;
    QSize m_logicalSize;
    qreal m_devicePixelRatio{0.0};
    QPixmap m_rendered;
};

} // namespace qtm
