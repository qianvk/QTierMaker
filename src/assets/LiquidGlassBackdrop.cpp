/*
 * Rounded-rectangle SDF, refraction, dispersion, and highlight formulas are derived from
 * AndroidLiquidGlass (Backdrop), Copyright 2025 Kyant.
 *
 * Licensed under the Apache License, Version 2.0. This file is a modified C++/Qt CPU-raster
 * port of the AGSL implementation at commit b18eb0ff12c616546a68c72e7d0097f1ab286c87.
 * See docs/third-party-notices.md for attribution.
 */

#include "assets/LiquidGlassBackdrop.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace tlm {

LiquidGlassParameters normalizedLiquidGlassParameters(LiquidGlassParameters parameters) {
    const auto bounded = [](qreal value, qreal minimum, qreal maximum, qreal fallback) {
        return std::isfinite(value) ? qBound(minimum, value, maximum) : fallback;
    };
    parameters.cornerRadius = bounded(parameters.cornerRadius, 0.0, 128.0, 16.0);
    parameters.blurRadius = bounded(parameters.blurRadius, 0.0, 32.0, 0.0);
    parameters.refractionHeightFraction =
        bounded(parameters.refractionHeightFraction, 0.0, 1.0, 0.2);
    parameters.refractionAmountFraction =
        bounded(parameters.refractionAmountFraction, 0.0, 1.0, 0.2);
    parameters.chromaticAberration =
        bounded(parameters.chromaticAberration, 0.0, 1.0, 0.0);
    return parameters;
}

bool operator==(const LiquidGlassParameters& left, const LiquidGlassParameters& right) {
    return qFuzzyCompare(left.cornerRadius + 1.0, right.cornerRadius + 1.0) &&
           qFuzzyCompare(left.blurRadius + 1.0, right.blurRadius + 1.0) &&
           qFuzzyCompare(left.refractionHeightFraction + 1.0,
                         right.refractionHeightFraction + 1.0) &&
           qFuzzyCompare(left.refractionAmountFraction + 1.0,
                         right.refractionAmountFraction + 1.0) &&
           qFuzzyCompare(left.chromaticAberration + 1.0, right.chromaticAberration + 1.0);
}

bool operator!=(const LiquidGlassParameters& left, const LiquidGlassParameters& right) {
    return !(left == right);
}

namespace {
constexpr int kMaximumRasterExtent = 1600;
constexpr qreal kVibrancySaturation = 1.5;
constexpr qreal kDefaultHighlightAlpha = 0.5;
constexpr qreal kPlainHighlightAlpha = 0.38;
constexpr qreal kAmbientHighlightAlpha = 0.38;
constexpr qreal kHighlightStrokeWidth = 2.0;

struct RgbaSample final {
    qreal red{0.0};
    qreal green{0.0};
    qreal blue{0.0};
    qreal alpha{0.0};
};

struct LensCoordinates final {
    bool unchanged{false};
    QPointF refracted;
    QPointF dispersed;
};

qreal sign(qreal value) {
    return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
}

QPointF normalized(QPointF value) {
    const qreal length = std::hypot(value.x(), value.y());
    return length > 0.000001 ? value / length : QPointF();
}

qreal dot(QPointF left, QPointF right) {
    return left.x() * right.x() + left.y() * right.y();
}

qreal radiusAt(QPointF coordinate, const std::array<qreal, 4>& radii) {
    if (coordinate.x() >= 0.0) {
        return coordinate.y() <= 0.0 ? radii[1] : radii[2];
    }
    return coordinate.y() <= 0.0 ? radii[0] : radii[3];
}

qreal sdRoundedRect(QPointF coordinate, QPointF halfSize, qreal radius) {
    const QPointF cornerCoordinate(std::abs(coordinate.x()) - (halfSize.x() - radius),
                                   std::abs(coordinate.y()) - (halfSize.y() - radius));
    const qreal outside =
        std::hypot(qMax<qreal>(cornerCoordinate.x(), 0.0), qMax<qreal>(cornerCoordinate.y(), 0.0)) -
        radius;
    const qreal inside = qMin(qMax(cornerCoordinate.x(), cornerCoordinate.y()), 0.0);
    return outside + inside;
}

QPointF gradSdRoundedRect(QPointF coordinate, QPointF halfSize, qreal radius) {
    const QPointF cornerCoordinate(std::abs(coordinate.x()) - (halfSize.x() - radius),
                                   std::abs(coordinate.y()) - (halfSize.y() - radius));
    if (cornerCoordinate.x() >= 0.0 || cornerCoordinate.y() >= 0.0) {
        const QPointF gradient = normalized(
            {qMax<qreal>(cornerCoordinate.x(), 0.0), qMax<qreal>(cornerCoordinate.y(), 0.0)});
        return {sign(coordinate.x()) * gradient.x(), sign(coordinate.y()) * gradient.y()};
    }

    const qreal gradientX = cornerCoordinate.x() >= cornerCoordinate.y() ? 1.0 : 0.0;
    return {sign(coordinate.x()) * gradientX, sign(coordinate.y()) * (1.0 - gradientX)};
}

qreal circleMap(qreal value) {
    value = qBound<qreal>(0.0, value, 1.0);
    return 1.0 - std::sqrt(qMax<qreal>(0.0, 1.0 - value * value));
}

QSize boundedRasterSize(QSize size) {
    size = size.expandedTo(QSize(1, 1));
    if (qMax(size.width(), size.height()) > kMaximumRasterExtent) {
        size.scale(QSize(kMaximumRasterExtent, kMaximumRasterExtent), Qt::KeepAspectRatio);
    }
    return size;
}

QSize quantizedMaterialShape(QSize logicalSize) {
    logicalSize = logicalSize.expandedTo(QSize(1, 1));
    const qreal aspect = static_cast<qreal>(logicalSize.width()) / logicalSize.height();
    const qreal quantizedAspect = qMax<qreal>(1.0 / 32.0, qRound(aspect * 32.0) / 32.0);
    const int longExtent =
        qBound(384, qCeil(qMax(logicalSize.width(), logicalSize.height()) / 128.0) * 128,
               kMaximumRasterExtent);
    if (quantizedAspect >= 1.0) {
        return QSize(longExtent, qMax(1, qRound(longExtent / quantizedAspect)));
    }
    return QSize(qMax(1, qRound(longExtent * quantizedAspect)), longExtent);
}

QRect coverCropRect(const QSize& sourceSize, const QSize& targetSize) {
    if (sourceSize.isEmpty() || targetSize.isEmpty()) {
        return {};
    }
    const qreal sourceRatio = static_cast<qreal>(sourceSize.width()) / sourceSize.height();
    const qreal targetRatio = static_cast<qreal>(targetSize.width()) / targetSize.height();
    if (sourceRatio > targetRatio) {
        const int width = qMax(1, qRound(sourceSize.height() * targetRatio));
        return QRect((sourceSize.width() - width) / 2, 0, width, sourceSize.height());
    }
    const int height = qMax(1, qRound(sourceSize.width() / targetRatio));
    return QRect(0, (sourceSize.height() - height) / 2, sourceSize.width(), height);
}

QImage flattenedSourceImage(const QImage& source, const LiquidGlassBackdropStyle& style,
                            QSize targetSize) {
    if (source.isNull() || targetSize.isEmpty()) {
        return {};
    }
    targetSize = boundedRasterSize(targetSize);
    QImage result(targetSize, QImage::Format_RGBA8888);
    QColor baseColor = style.baseColor;
    baseColor.setAlpha(255);
    result.fill(baseColor);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(qBound<qreal>(0.0, style.sourceVisibility, 1.0));
    const QRect sourceRect = style.sourceLayout == LiquidGlassSourceLayout::Cover
                                 ? coverCropRect(source.size(), targetSize)
                                 : source.rect();
    painter.drawImage(result.rect(), source, sourceRect);
    return result;
}

uchar clampChannel(qreal value) {
    return static_cast<uchar>(qBound(0, qRound(value), 255));
}

void applyVibrancy(QImage& image) {
    const qreal inverseSaturation = 1.0 - kVibrancySaturation;
    const qreal redLuminance = 0.213 * inverseSaturation;
    const qreal greenLuminance = 0.715 * inverseSaturation;
    const qreal blueLuminance = 0.072 * inverseSaturation;

    for (int y = 0; y < image.height(); ++y) {
        uchar* line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            uchar* pixel = line + x * 4;
            const qreal red = pixel[0];
            const qreal green = pixel[1];
            const qreal blue = pixel[2];
            pixel[0] = clampChannel((redLuminance + kVibrancySaturation) * red +
                                    greenLuminance * green + blueLuminance * blue);
            pixel[1] =
                clampChannel(redLuminance * red + (greenLuminance + kVibrancySaturation) * green +
                             blueLuminance * blue);
            pixel[2] = clampChannel(redLuminance * red + greenLuminance * green +
                                    (blueLuminance + kVibrancySaturation) * blue);
        }
    }
}

const uchar* pixelAt(const QImage& image, int x, int y);

QImage boxBlurHorizontal(const QImage& source, int radius) {
    if (source.isNull() || radius <= 0) {
        return source;
    }

    QImage result(source.size(), QImage::Format_RGBA8888);
    const int sampleCount = radius * 2 + 1;
    for (int y = 0; y < source.height(); ++y) {
        std::array<qint64, 4> sums{};
        for (int offset = -radius; offset <= radius; ++offset) {
            const uchar* pixel = pixelAt(source, offset, y);
            for (int channel = 0; channel < 4; ++channel) {
                sums[channel] += pixel[channel];
            }
        }

        uchar* output = result.scanLine(y);
        for (int x = 0; x < source.width(); ++x) {
            for (int channel = 0; channel < 4; ++channel) {
                output[x * 4 + channel] =
                    static_cast<uchar>((sums[channel] + sampleCount / 2) / sampleCount);
            }
            const uchar* leaving = pixelAt(source, x - radius, y);
            const uchar* entering = pixelAt(source, x + radius + 1, y);
            for (int channel = 0; channel < 4; ++channel) {
                sums[channel] += entering[channel] - leaving[channel];
            }
        }
    }
    return result;
}

QImage boxBlurVertical(const QImage& source, int radius) {
    if (source.isNull() || radius <= 0) {
        return source;
    }

    QImage result(source.size(), QImage::Format_RGBA8888);
    const int sampleCount = radius * 2 + 1;
    for (int x = 0; x < source.width(); ++x) {
        std::array<qint64, 4> sums{};
        for (int offset = -radius; offset <= radius; ++offset) {
            const uchar* pixel = pixelAt(source, x, offset);
            for (int channel = 0; channel < 4; ++channel) {
                sums[channel] += pixel[channel];
            }
        }

        for (int y = 0; y < source.height(); ++y) {
            uchar* output = result.scanLine(y) + x * 4;
            for (int channel = 0; channel < 4; ++channel) {
                output[channel] =
                    static_cast<uchar>((sums[channel] + sampleCount / 2) / sampleCount);
            }
            const uchar* leaving = pixelAt(source, x, y - radius);
            const uchar* entering = pixelAt(source, x, y + radius + 1);
            for (int channel = 0; channel < 4; ++channel) {
                sums[channel] += entering[channel] - leaving[channel];
            }
        }
    }
    return result;
}

std::array<int, 3> gaussianBoxRadii(qreal sigma) {
    constexpr int passCount = 3;
    const qreal idealWidth = std::sqrt((12.0 * sigma * sigma / passCount) + 1.0);
    int lowerWidth = qMax(1, static_cast<int>(std::floor(idealWidth)));
    if (lowerWidth % 2 == 0) {
        --lowerWidth;
    }
    const int upperWidth = lowerWidth + 2;
    const qreal numerator =
        12.0 * sigma * sigma - passCount * lowerWidth * lowerWidth -
        4.0 * passCount * lowerWidth - 3.0 * passCount;
    const int lowerPasses =
        qBound(0, qRound(numerator / (-4.0 * lowerWidth - 4.0)), passCount);

    std::array<int, passCount> radii{};
    for (int pass = 0; pass < passCount; ++pass) {
        const int width = pass < lowerPasses ? lowerWidth : upperWidth;
        radii[pass] = (width - 1) / 2;
    }
    return radii;
}

void applyGaussianBlur(QImage& image, qreal radius) {
    if (image.isNull() || radius < 0.01) {
        return;
    }

    // Android delegates BlurEffect to a GPU Gaussian. Three running-sum box passes are the
    // standard linear-time Gaussian approximation and keep the cached CPU port responsive.
    for (const int boxRadius : gaussianBoxRadii(radius)) {
        if (boxRadius <= 0) {
            continue;
        }
        image = boxBlurVertical(boxBlurHorizontal(image, boxRadius), boxRadius);
    }
}

const uchar* pixelAt(const QImage& image, int x, int y) {
    x = qBound(0, x, image.width() - 1);
    y = qBound(0, y, image.height() - 1);
    return image.constScanLine(y) + x * 4;
}

RgbaSample sampleRgba(const QImage& image, qreal x, qreal y) {
    x = qBound<qreal>(0.0, x, image.width() - 1.0);
    y = qBound<qreal>(0.0, y, image.height() - 1.0);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = qMin(x0 + 1, image.width() - 1);
    const int y1 = qMin(y0 + 1, image.height() - 1);
    const qreal horizontal = x - x0;
    const qreal vertical = y - y0;
    const uchar* topLeft = pixelAt(image, x0, y0);
    const uchar* topRight = pixelAt(image, x1, y0);
    const uchar* bottomLeft = pixelAt(image, x0, y1);
    const uchar* bottomRight = pixelAt(image, x1, y1);

    const auto channel = [&](int index) {
        const qreal top = topLeft[index] * (1.0 - horizontal) + topRight[index] * horizontal;
        const qreal bottom =
            bottomLeft[index] * (1.0 - horizontal) + bottomRight[index] * horizontal;
        return top * (1.0 - vertical) + bottom * vertical;
    };
    return {channel(0), channel(1), channel(2), channel(3)};
}

LensCoordinates mapLensCoordinate(QPointF coordinate, QPointF halfSize,
                                  const std::array<qreal, 4>& cornerRadii, qreal refractionHeight,
                                  qreal refractionAmount, bool depthEffect,
                                  bool chromaticAberration) {
    const QPointF centeredCoordinate = coordinate - halfSize;
    const qreal radius = radiusAt(coordinate, cornerRadii);
    qreal signedDistance = sdRoundedRect(centeredCoordinate, halfSize, radius);
    if (-signedDistance >= refractionHeight) {
        return {true, coordinate, {}};
    }
    signedDistance = qMin(signedDistance, 0.0);

    // AndroidLiquidGlass passes a negative refractionAmount uniform to this shader.
    const qreal displacement =
        -circleMap(1.0 - (-signedDistance / refractionHeight)) * refractionAmount;
    const qreal gradientRadius = qMin(radius * 1.5, qMin(halfSize.x(), halfSize.y()));
    QPointF gradient = gradSdRoundedRect(centeredCoordinate, halfSize, gradientRadius);
    if (depthEffect) {
        gradient += normalized(centeredCoordinate);
    }
    gradient = normalized(gradient);

    const QPointF refractedCoordinate = coordinate + displacement * gradient;
    const qreal dispersionIntensity =
        chromaticAberration
            ? (centeredCoordinate.x() * centeredCoordinate.y()) / (halfSize.x() * halfSize.y())
            : 0.0;
    return {false, refractedCoordinate, displacement * dispersionIntensity * gradient};
}

RgbaSample dispersedSample(const QImage& content, const LensCoordinates& lens) {
    const QPointF pixelCenter(0.5, 0.5);
    const auto sample = [&](qreal dispersionScale) {
        const QPointF coordinate = lens.refracted + lens.dispersed * dispersionScale - pixelCenter;
        return sampleRgba(content, coordinate.x(), coordinate.y());
    };

    const RgbaSample red = sample(1.0);
    const RgbaSample orange = sample(2.0 / 3.0);
    const RgbaSample yellow = sample(1.0 / 3.0);
    const RgbaSample green = sample(0.0);
    const RgbaSample cyan = sample(-1.0 / 3.0);
    const RgbaSample blue = sample(-2.0 / 3.0);
    const RgbaSample purple = sample(-1.0);

    // These weights intentionally mirror the seven AGSL content.eval calls.
    return {red.red / 3.5 + orange.red / 3.5 + yellow.red / 3.5 + purple.red / 7.0,
            orange.green / 7.0 + yellow.green / 3.5 + green.green / 3.5 + cyan.green / 3.5,
            cyan.blue / 3.0 + blue.blue / 3.0 + purple.blue / 3.0,
            (red.alpha + orange.alpha + yellow.alpha + green.alpha + cyan.alpha + blue.alpha +
             purple.alpha) /
                7.0};
}

QImage createHighlightMask(QSize size, qreal radius) {
    QImage mask(size, QImage::Format_RGBA8888);
    mask.fill(Qt::transparent);

    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath outline;
    outline.addRoundedRect(QRectF(QPointF(), QSizeF(size)), radius, radius);
    painter.setClipPath(outline);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, kHighlightStrokeWidth));
    painter.drawPath(outline);
    return mask;
}

void applyHighlight(QImage& image, qreal radius, LiquidGlassHighlightStyle style) {
    if (style == LiquidGlassHighlightStyle::None || image.isNull()) {
        return;
    }

    const QImage coverage = createHighlightMask(image.size(), radius);
    const QPointF halfSize(image.width() * 0.5, image.height() * 0.5);
    const std::array<qreal, 4> radii{radius, radius, radius, radius};
    const QPointF lightNormal(std::cos(std::numbers::pi_v<qreal> * 0.25),
                              std::sin(std::numbers::pi_v<qreal> * 0.25));

    for (int y = 0; y < image.height(); ++y) {
        uchar* output = image.scanLine(y);
        const uchar* coverageLine = coverage.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const qreal strokeCoverage = coverageLine[x * 4 + 3] / 255.0;
            if (strokeCoverage <= 0.0) {
                continue;
            }

            const QPointF coordinate(x + 0.5, y + 0.5);
            const QPointF centeredCoordinate = coordinate - halfSize;
            const qreal cornerRadius = radiusAt(coordinate, radii);
            const qreal gradientRadius = qMin(cornerRadius * 1.5, qMin(halfSize.x(), halfSize.y()));
            const QPointF gradient =
                gradSdRoundedRect(centeredCoordinate, halfSize, gradientRadius);
            const qreal directionalIntensity = std::abs(dot(gradient, lightNormal));
            uchar* pixel = output + x * 4;

            if (style == LiquidGlassHighlightStyle::Ambient) {
                const qreal alpha = kAmbientHighlightAlpha * directionalIntensity * strokeCoverage;
                const qreal value = dot(gradient, lightNormal) >= 0.0 ? 255.0 : 0.0;
                pixel[0] = clampChannel(value * alpha + pixel[0] * (1.0 - alpha));
                pixel[1] = clampChannel(value * alpha + pixel[1] * (1.0 - alpha));
                pixel[2] = clampChannel(value * alpha + pixel[2] * (1.0 - alpha));
                continue;
            }

            const qreal highlightAlpha = style == LiquidGlassHighlightStyle::Plain
                                             ? kPlainHighlightAlpha
                                             : kDefaultHighlightAlpha * directionalIntensity;
            const qreal additive = 255.0 * highlightAlpha * strokeCoverage;
            pixel[0] = clampChannel(pixel[0] + additive);
            pixel[1] = clampChannel(pixel[1] + additive);
            pixel[2] = clampChannel(pixel[2] + additive);
        }
    }
}

bool sameStyle(const LiquidGlassBackdropStyle& left, const LiquidGlassBackdropStyle& right) {
    return left.baseColor == right.baseColor && left.contentTreatment == right.contentTreatment &&
           left.sourceLayout == right.sourceLayout && left.depthEffect == right.depthEffect &&
           left.highlightStyle == right.highlightStyle &&
           qFuzzyCompare(left.sourceVisibility + 1.0, right.sourceVisibility + 1.0) &&
           qFuzzyCompare(left.parameters.cornerRadius + 1.0,
                         right.parameters.cornerRadius + 1.0) &&
           qFuzzyCompare(left.parameters.blurRadius + 1.0,
                         right.parameters.blurRadius + 1.0) &&
           qFuzzyCompare(left.parameters.refractionHeightFraction + 1.0,
                         right.parameters.refractionHeightFraction + 1.0) &&
           qFuzzyCompare(left.parameters.refractionAmountFraction + 1.0,
                         right.parameters.refractionAmountFraction + 1.0) &&
           (left.parameters.chromaticAberration > 0.0) ==
               (right.parameters.chromaticAberration > 0.0);
}
} // namespace

QImage createLiquidGlassBackdropImage(const QImage& source, const LiquidGlassBackdropStyle& style,
                                      QSize logicalSize) {
    QImage content = flattenedSourceImage(source, style, logicalSize);
    if (content.isNull()) {
        return {};
    }
    if (style.contentTreatment == LiquidGlassContentTreatment::TonedBackdrop) {
        applyVibrancy(content);
    }
    const qreal rasterBlurRadius = std::isfinite(style.parameters.blurRadius)
                                       ? qMax<qreal>(0.0, style.parameters.blurRadius)
                                       : 0.0;
    LiquidGlassParameters parameters =
        normalizedLiquidGlassParameters(style.parameters);
    // Cache rasterization scales a logical 0..32 dp blur to its quantized material size.
    parameters.blurRadius = rasterBlurRadius;
    applyGaussianBlur(content, parameters.blurRadius);

    const qreal width = content.width();
    const qreal height = content.height();
    const qreal minimumDimension = qMin(width, height);
    const QPointF halfSize(width * 0.5, height * 0.5);
    const qreal radius = qMin(parameters.cornerRadius, minimumDimension * 0.5);
    const std::array<qreal, 4> cornerRadii{radius, radius, radius, radius};
    const qreal refractionHeight =
        minimumDimension * parameters.refractionHeightFraction * 0.5;
    const qreal refractionAmount = minimumDimension * parameters.refractionAmountFraction;
    const bool chromaticAberration = parameters.chromaticAberration > 0.0;
    if (refractionHeight <= 0.0 || refractionAmount <= 0.0) {
        applyHighlight(content, radius, style.highlightStyle);
        return content;
    }

    QImage result(content.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < content.height(); ++y) {
        uchar* output = result.scanLine(y);
        for (int x = 0; x < content.width(); ++x) {
            const QPointF coordinate(x + 0.5, y + 0.5);
            const LensCoordinates lens =
                mapLensCoordinate(coordinate, halfSize, cornerRadii, refractionHeight,
                                  refractionAmount, style.depthEffect, chromaticAberration);
            RgbaSample color;
            if (lens.unchanged) {
                const uchar* sourcePixel = pixelAt(content, x, y);
                color = {static_cast<qreal>(sourcePixel[0]), static_cast<qreal>(sourcePixel[1]),
                         static_cast<qreal>(sourcePixel[2]), static_cast<qreal>(sourcePixel[3])};
            } else if (chromaticAberration) {
                color = dispersedSample(content, lens);
            } else {
                const QPointF sampleCoordinate = lens.refracted - QPointF(0.5, 0.5);
                color = sampleRgba(content, sampleCoordinate.x(), sampleCoordinate.y());
            }

            uchar* pixel = output + x * 4;
            pixel[0] = clampChannel(color.red);
            pixel[1] = clampChannel(color.green);
            pixel[2] = clampChannel(color.blue);
            pixel[3] = clampChannel(color.alpha);
        }
    }

    applyHighlight(result, radius, style.highlightStyle);
    return result;
}

const QPixmap& LiquidGlassBackdropCache::pixmap(const QString& sourceId, const QImage& source,
                                                const LiquidGlassBackdropStyle& style,
                                                QSize logicalSize, qreal devicePixelRatio) {
    if (source.isNull()) {
        clear();
        return m_rendered;
    }
    logicalSize = logicalSize.expandedTo(QSize(1, 1));
    devicePixelRatio = qMax<qreal>(1.0, devicePixelRatio);
    const QSize materialShape = quantizedMaterialShape(logicalSize);
    const qreal logicalMinimum = qMax(1, qMin(logicalSize.width(), logicalSize.height()));
    const qreal materialMinimum = qMin(materialShape.width(), materialShape.height());
    const qreal rasterBlurRadius =
        normalizedLiquidGlassParameters(style.parameters).blurRadius * materialMinimum /
        logicalMinimum;
    const bool materialMatches = m_sourceId == sourceId && m_sourceCacheKey == source.cacheKey() &&
                                 m_hasStyle && sameStyle(m_style, style) &&
                                 m_materialShape == materialShape &&
                                 qFuzzyCompare(m_rasterBlurRadius + 1.0,
                                               rasterBlurRadius + 1.0);
    if (!materialMatches) {
        LiquidGlassBackdropStyle rasterStyle = style;
        rasterStyle.parameters.blurRadius = rasterBlurRadius;
        m_material = createLiquidGlassBackdropImage(source, rasterStyle, materialShape);
        m_sourceId = sourceId;
        m_sourceCacheKey = source.cacheKey();
        m_style = style;
        m_hasStyle = true;
        m_materialShape = materialShape;
        m_rasterBlurRadius = rasterBlurRadius;
        m_logicalSize = {};
        m_devicePixelRatio = 0.0;
        m_rendered = {};
    }
    if (!m_rendered.isNull() && m_logicalSize == logicalSize &&
        qFuzzyCompare(m_devicePixelRatio, devicePixelRatio)) {
        return m_rendered;
    }
    if (m_material.isNull()) {
        return m_rendered;
    }

    const QSize pixelSize(qMax(1, qCeil(logicalSize.width() * devicePixelRatio)),
                          qMax(1, qCeil(logicalSize.height() * devicePixelRatio)));
    m_rendered = QPixmap::fromImage(
        m_material.scaled(pixelSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    m_rendered.setDevicePixelRatio(devicePixelRatio);
    m_logicalSize = logicalSize;
    m_devicePixelRatio = devicePixelRatio;
    return m_rendered;
}

void LiquidGlassBackdropCache::clear() {
    m_sourceId.clear();
    m_sourceCacheKey = 0;
    m_style = {};
    m_hasStyle = false;
    m_materialShape = {};
    m_rasterBlurRadius = -1.0;
    m_material = {};
    m_logicalSize = {};
    m_devicePixelRatio = 0.0;
    m_rendered = {};
}

} // namespace tlm
