#include "assets/FocusBackdrop.h"

#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>

namespace qtm {

namespace {
constexpr int kMaximumRasterExtent = 1280;
constexpr int kLowPassLevels = 3;
constexpr qreal kBackdropSaturation = 0.50;
constexpr qreal kBackdropContrast = 0.58;
constexpr qreal kMaximumSourcePresence = 0.84;
constexpr qreal kLightMaximumMeanLuminance = 0.34;
constexpr qreal kDarkMaximumMeanLuminance = 0.13;

struct ColorTransferTables final {
    std::array<qreal, 256> toLinear{};
    std::array<uchar, 4097> toSrgb{};

    ColorTransferTables() {
        for (int value = 0; value < static_cast<int>(toLinear.size()); ++value) {
            const qreal channel = static_cast<qreal>(value) / 255.0;
            toLinear[static_cast<std::size_t>(value)] =
                channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }
        for (int value = 0; value < static_cast<int>(toSrgb.size()); ++value) {
            const qreal channel = static_cast<qreal>(value) / static_cast<qreal>(toSrgb.size() - 1);
            const qreal encoded = channel <= 0.0031308
                                      ? channel * 12.92
                                      : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
            toSrgb[static_cast<std::size_t>(value)] =
                static_cast<uchar>(qBound(0, qRound(encoded * 255.0), 255));
        }
    }
};

const ColorTransferTables& colorTransferTables() {
    static const ColorTransferTables tables;
    return tables;
}

qreal relativeLuminance(qreal red, qreal green, qreal blue) {
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

uchar encodeLinear(qreal value) {
    const auto& table = colorTransferTables().toSrgb;
    const int index = qBound(0, qRound(value * static_cast<qreal>(table.size() - 1)),
                             static_cast<int>(table.size() - 1));
    return table[static_cast<std::size_t>(index)];
}

QImage flattenedWorkingImage(const QImage& source, QColor baseColor) {
    if (source.isNull()) {
        return {};
    }

    QSize workingSize = source.size();
    if (qMax(workingSize.width(), workingSize.height()) > kMaximumRasterExtent) {
        workingSize.scale(QSize(kMaximumRasterExtent, kMaximumRasterExtent), Qt::KeepAspectRatio);
    }

    const QImage scaled =
        source.scaled(workingSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage flattened(workingSize, QImage::Format_RGBA8888);
    baseColor.setAlpha(255);
    flattened.fill(baseColor);
    QPainter painter(&flattened);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(flattened.rect(), scaled);
    return flattened;
}

QImage lowFrequencyImage(QImage image) {
    if (image.isNull()) {
        return {};
    }

    // Repeated smooth reduction and reconstruction is a compact Gaussian-like low-pass.
    // Qt executes the expensive resampling in optimized raster code, and this runs only
    // when the source or semantic material parameters change.
    std::array<QSize, kLowPassLevels> expansionSizes{};
    int levelCount = 0;
    while (levelCount < kLowPassLevels) {
        const QSize reducedSize(qMax(1, image.width() / 2), qMax(1, image.height() / 2));
        if (reducedSize == image.size() || qMin(reducedSize.width(), reducedSize.height()) < 16) {
            break;
        }
        expansionSizes[static_cast<std::size_t>(levelCount)] = image.size();
        image = image.scaled(reducedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        ++levelCount;
    }
    while (levelCount > 0) {
        --levelCount;
        image = image.scaled(expansionSizes[static_cast<std::size_t>(levelCount)],
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

void applyAdaptiveTone(QImage* image, const FocusBackdropStyle& style) {
    if (!image || image->isNull()) {
        return;
    }

    const auto& decode = colorTransferTables().toLinear;
    qreal luminanceSum = 0.0;
    for (int y = 0; y < image->height(); ++y) {
        const uchar* line = image->constScanLine(y);
        for (int x = 0; x < image->width(); ++x) {
            const uchar* pixel = line + x * 4;
            luminanceSum += relativeLuminance(decode[static_cast<std::size_t>(pixel[0])],
                                              decode[static_cast<std::size_t>(pixel[1])],
                                              decode[static_cast<std::size_t>(pixel[2])]);
        }
    }

    const qreal pixelCount = static_cast<qreal>(image->width()) * image->height();
    const qreal meanLuminance = pixelCount > 0.0 ? luminanceSum / pixelCount : 0.0;
    const qreal maximumMean =
        style.darkAppearance ? kDarkMaximumMeanLuminance : kLightMaximumMeanLuminance;
    const qreal luminanceGain =
        meanLuminance > maximumMean && meanLuminance > 0.0 ? maximumMean / meanLuminance : 1.0;

    QColor baseColor = style.baseColor;
    baseColor.setAlpha(255);
    qreal baseRed = decode[static_cast<std::size_t>(baseColor.red())];
    qreal baseGreen = decode[static_cast<std::size_t>(baseColor.green())];
    qreal baseBlue = decode[static_cast<std::size_t>(baseColor.blue())];
    const qreal baseLuminance = relativeLuminance(baseRed, baseGreen, baseBlue);
    if (baseLuminance > maximumMean && baseLuminance > 0.0) {
        const qreal baseGain = maximumMean / baseLuminance;
        baseRed *= baseGain;
        baseGreen *= baseGain;
        baseBlue *= baseGain;
    }

    const qreal sourcePresence =
        qBound<qreal>(0.0, style.sourceVisibility, 1.0) * kMaximumSourcePresence;
    for (int y = 0; y < image->height(); ++y) {
        uchar* line = image->scanLine(y);
        for (int x = 0; x < image->width(); ++x) {
            uchar* pixel = line + x * 4;
            const qreal red = decode[static_cast<std::size_t>(pixel[0])];
            const qreal green = decode[static_cast<std::size_t>(pixel[1])];
            const qreal blue = decode[static_cast<std::size_t>(pixel[2])];
            const qreal luminance = relativeLuminance(red, green, blue);
            const qreal compressedLuminance =
                (meanLuminance + (luminance - meanLuminance) * kBackdropContrast) * luminanceGain;
            const qreal chromaScale = kBackdropSaturation * luminanceGain;
            const qreal tonedRed =
                qBound<qreal>(0.0, compressedLuminance + (red - luminance) * chromaScale, 1.0);
            const qreal tonedGreen =
                qBound<qreal>(0.0, compressedLuminance + (green - luminance) * chromaScale, 1.0);
            const qreal tonedBlue =
                qBound<qreal>(0.0, compressedLuminance + (blue - luminance) * chromaScale, 1.0);

            pixel[0] = encodeLinear(baseRed + (tonedRed - baseRed) * sourcePresence);
            pixel[1] = encodeLinear(baseGreen + (tonedGreen - baseGreen) * sourcePresence);
            pixel[2] = encodeLinear(baseBlue + (tonedBlue - baseBlue) * sourcePresence);
            pixel[3] = 255;
        }
    }
}

bool sameStyle(const FocusBackdropStyle& left, const FocusBackdropStyle& right) {
    return left.baseColor == right.baseColor && left.darkAppearance == right.darkAppearance &&
           qFuzzyCompare(left.sourceVisibility + 1.0, right.sourceVisibility + 1.0);
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
} // namespace

QImage createFocusBackdropImage(const QImage& source, const FocusBackdropStyle& style) {
    QImage backdrop = lowFrequencyImage(flattenedWorkingImage(source, style.baseColor));
    applyAdaptiveTone(&backdrop, style);
    return backdrop;
}

const QPixmap& FocusBackdropCache::pixmap(const QString& sourceId, const QImage& source,
                                          const FocusBackdropStyle& style, QSize logicalSize,
                                          qreal devicePixelRatio) {
    if (source.isNull()) {
        clear();
        return m_rendered;
    }
    logicalSize = logicalSize.expandedTo(QSize(1, 1));
    devicePixelRatio = qMax<qreal>(1.0, devicePixelRatio);

    const bool materialMatches = m_sourceId == sourceId && m_sourceCacheKey == source.cacheKey() &&
                                 m_hasStyle && sameStyle(m_style, style);
    if (!materialMatches) {
        m_material = createFocusBackdropImage(source, style);
        m_sourceId = sourceId;
        m_sourceCacheKey = source.cacheKey();
        m_style = style;
        m_hasStyle = true;
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
    const QImage cropped = m_material.copy(coverCropRect(m_material.size(), pixelSize));
    m_rendered = QPixmap::fromImage(
        cropped.scaled(pixelSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    m_rendered.setDevicePixelRatio(devicePixelRatio);
    m_logicalSize = logicalSize;
    m_devicePixelRatio = devicePixelRatio;
    return m_rendered;
}

void FocusBackdropCache::clear() {
    m_sourceId.clear();
    m_sourceCacheKey = 0;
    m_style = {};
    m_hasStyle = false;
    m_material = {};
    m_logicalSize = {};
    m_devicePixelRatio = 0.0;
    m_rendered = {};
}

} // namespace qtm
