#include "assets/LiquidGlassBackdrop.h"

#include <QTest>

#include <algorithm>
#include <cmath>

using namespace qtm;

namespace {
QImage horizontalGradient(QSize size) {
    QImage source(size, QImage::Format_RGBA8888);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            const int value = qBound(0, x, 255);
            source.setPixelColor(x, y, QColor(value, value, value));
        }
    }
    return source;
}

QImage coordinatePattern(QSize size) {
    QImage source(size, QImage::Format_RGBA8888);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            source.setPixelColor(x, y,
                                 QColor(qBound(0, x, 255),
                                        qBound(0, qRound(x * 0.75 + y * 0.5), 255),
                                        qBound(0, 255 - x, 255)));
        }
    }
    return source;
}

LiquidGlassBackdropStyle clearLensStyle(QSize size, qreal refractionHeight,
                                        qreal refractionAmount) {
    LiquidGlassBackdropStyle style;
    style.baseColor = Qt::black;
    style.contentTreatment = LiquidGlassContentTreatment::ClearSurface;
    style.sourceLayout = LiquidGlassSourceLayout::Stretch;
    style.parameters.cornerRadius = 0.0;
    const qreal minimumDimension = qMin(size.width(), size.height());
    style.parameters.refractionHeightFraction = refractionHeight * 2.0 / minimumDimension;
    style.parameters.refractionAmountFraction = refractionAmount / minimumDimension;
    style.parameters.chromaticAberration = 0.0;
    style.depthEffect = false;
    style.highlightStyle = LiquidGlassHighlightStyle::None;
    return style;
}

qreal circleMap(qreal value) {
    return 1.0 - std::sqrt(1.0 - value * value);
}

qreal sampleLinearRamp(qreal coordinate, qreal multiplier, qreal offset = 0.0) {
    return qBound<qreal>(0.0, coordinate * multiplier + offset, 255.0);
}

int maximumChannelDistance(const QColor& left, const QColor& right) {
    return std::max({std::abs(left.red() - right.red()), std::abs(left.green() - right.green()),
                     std::abs(left.blue() - right.blue())});
}

int maximumRed(const QImage& image, QRect area) {
    area = area.intersected(image.rect());
    int maximum = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            maximum = qMax(maximum, image.pixelColor(x, y).red());
        }
    }
    return maximum;
}
} // namespace

class LiquidGlassBackdropTest final : public QObject {
    Q_OBJECT

private slots:
    void usesAndroidPlaygroundDefaultsAndBounds();
    void appliesConfiguredBlurBeforeLens();
    void createsOpaqueRoundedRectLens();
    void preservesPixelsBeyondRefractionHeight();
    void matchesAndroidCircleMapDisplacement();
    void depthEffectBlendsTheSdfAndRadialGradients();
    void reproducesSevenSampleChromaticAberration();
    void chromaticAberrationVanishesOnTheShapeAxes();
    void appliesAndroidDefaultDirectionalHighlight();
    void appliesAndroidVibrancyBeforeRefraction();
    void composesSourceVisibilityBeforeRefraction();
    void boundsProcessingResolution();
    void reusesRasterUntilLensInputsChange();
};

void LiquidGlassBackdropTest::usesAndroidPlaygroundDefaultsAndBounds() {
    const LiquidGlassParameters defaults;
    QCOMPARE(defaults.cornerRadius, 16.0);
    QCOMPARE(defaults.blurRadius, 0.0);
    QCOMPARE(defaults.refractionHeightFraction, 0.2);
    QCOMPARE(defaults.refractionAmountFraction, 0.2);
    QCOMPARE(defaults.chromaticAberration, 0.0);

    LiquidGlassParameters outOfRange;
    outOfRange.cornerRadius = 256.0;
    outOfRange.blurRadius = 64.0;
    outOfRange.refractionHeightFraction = -1.0;
    outOfRange.refractionAmountFraction = 1.5;
    outOfRange.chromaticAberration = -0.5;
    const LiquidGlassParameters normalized =
        normalizedLiquidGlassParameters(outOfRange);
    QCOMPARE(normalized.cornerRadius, 128.0);
    QCOMPARE(normalized.blurRadius, 32.0);
    QCOMPARE(normalized.refractionHeightFraction, 0.0);
    QCOMPARE(normalized.refractionAmountFraction, 1.0);
    QCOMPARE(normalized.chromaticAberration, 0.0);
}

void LiquidGlassBackdropTest::appliesConfiguredBlurBeforeLens() {
    const QSize size(96, 48);
    QImage source(size, QImage::Format_RGBA8888);
    source.fill(Qt::black);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = size.width() / 2; x < size.width(); ++x) {
            source.setPixelColor(x, y, Qt::white);
        }
    }

    LiquidGlassBackdropStyle style = clearLensStyle(size, 0.0, 0.0);
    style.parameters.blurRadius = 8.0;
    const QImage material =
        createLiquidGlassBackdropImage(source, style, source.size());

    const int leftEdge = material.pixelColor(size.width() / 2 - 1, size.height() / 2).red();
    const int rightEdge = material.pixelColor(size.width() / 2, size.height() / 2).red();
    QVERIFY(leftEdge > 0 && leftEdge < 128);
    QVERIFY(rightEdge >= 128 && rightEdge < 255);
    QCOMPARE(material.pixelColor(0, size.height() / 2), QColor(Qt::black));
    QCOMPARE(material.pixelColor(size.width() - 1, size.height() / 2), QColor(Qt::white));
}

void LiquidGlassBackdropTest::createsOpaqueRoundedRectLens() {
    const QSize size(240, 140);
    const QImage source = coordinatePattern(size);
    const LiquidGlassBackdropStyle style = clearLensStyle(size, 20.0, 40.0);

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    QCOMPARE(material.size(), source.size());
    QCOMPARE(material.format(), QImage::Format_RGBA8888);
    QVERIFY(material != source);
    QCOMPARE(material.pixelColor(0, material.height() / 2).alpha(), 255);
}

void LiquidGlassBackdropTest::preservesPixelsBeyondRefractionHeight() {
    const QSize size(240, 140);
    const QImage source = coordinatePattern(size);
    const LiquidGlassBackdropStyle style = clearLensStyle(size, 20.0, 40.0);

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    QCOMPARE(material.pixelColor(24, size.height() / 2), source.pixelColor(24, size.height() / 2));
    QCOMPARE(material.pixelColor(size.width() / 2, size.height() / 2),
             source.pixelColor(size.width() / 2, size.height() / 2));
}

void LiquidGlassBackdropTest::matchesAndroidCircleMapDisplacement() {
    constexpr qreal kRefractionHeight = 20.0;
    constexpr qreal kRefractionAmount = 40.0;
    const QSize size(240, 140);
    const QImage source = horizontalGradient(size);
    const LiquidGlassBackdropStyle style =
        clearLensStyle(size, kRefractionHeight, kRefractionAmount);

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    const int x = 10;
    const int y = size.height() / 2;
    const qreal depthInside = x + 0.5;
    const qreal displacement = circleMap(1.0 - depthInside / kRefractionHeight) * kRefractionAmount;
    const int expected = qRound(x + displacement);
    QVERIFY2(std::abs(material.pixelColor(x, y).red() - expected) <= 1,
             qPrintable(QStringLiteral("actual=%1 expected=%2")
                            .arg(material.pixelColor(x, y).red())
                            .arg(expected)));
}

void LiquidGlassBackdropTest::depthEffectBlendsTheSdfAndRadialGradients() {
    const QSize size(240, 140);
    const QImage source = coordinatePattern(size);
    LiquidGlassBackdropStyle style = clearLensStyle(size, 36.0, 58.0);

    const QImage withoutDepth = createLiquidGlassBackdropImage(source, style, source.size());
    style.depthEffect = true;
    const QImage withDepth = createLiquidGlassBackdropImage(source, style, source.size());

    const QPoint offAxis(20, 5);
    QVERIFY(maximumChannelDistance(withoutDepth.pixelColor(offAxis),
                                   withDepth.pixelColor(offAxis)) > 3);
}

void LiquidGlassBackdropTest::reproducesSevenSampleChromaticAberration() {
    constexpr qreal kRefractionHeight = 26.0;
    constexpr qreal kRefractionAmount = 44.0;
    const QSize size(241, 141);
    const QImage source = coordinatePattern(size);
    LiquidGlassBackdropStyle style = clearLensStyle(size, kRefractionHeight, kRefractionAmount);
    style.parameters.chromaticAberration = 1.0;

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    const int x = 10;
    const int y = 20;
    const qreal halfWidth = size.width() * 0.5;
    const qreal halfHeight = size.height() * 0.5;
    const qreal centeredX = x + 0.5 - halfWidth;
    const qreal centeredY = y + 0.5 - halfHeight;
    const qreal depthInside = x + 0.5;
    const qreal displacement = circleMap(1.0 - depthInside / kRefractionHeight) * kRefractionAmount;
    const qreal refractedX = x + displacement;
    const qreal dispersedX = displacement * ((centeredX * centeredY) / (halfWidth * halfHeight));
    const auto redAt = [&](qreal scale) {
        return sampleLinearRamp(refractedX + dispersedX * scale, 1.0);
    };
    const auto greenAt = [&](qreal scale) {
        return sampleLinearRamp(refractedX + dispersedX * scale, 0.75, y * 0.5);
    };
    const auto blueAt = [&](qreal scale) {
        return sampleLinearRamp(refractedX + dispersedX * scale, -1.0, 255.0);
    };
    const int expectedRed = qRound(redAt(1.0) / 3.5 + redAt(2.0 / 3.0) / 3.5 +
                                   redAt(1.0 / 3.0) / 3.5 + redAt(-1.0) / 7.0);
    const int expectedGreen = qRound(greenAt(2.0 / 3.0) / 7.0 + greenAt(1.0 / 3.0) / 3.5 +
                                     greenAt(0.0) / 3.5 + greenAt(-1.0 / 3.0) / 3.5);
    const int expectedBlue =
        qRound(blueAt(-1.0 / 3.0) / 3.0 + blueAt(-2.0 / 3.0) / 3.0 + blueAt(-1.0) / 3.0);

    const QColor actual = material.pixelColor(x, y);
    QVERIFY(std::abs(actual.red() - expectedRed) <= 1);
    QVERIFY(std::abs(actual.green() - expectedGreen) <= 1);
    QVERIFY(std::abs(actual.blue() - expectedBlue) <= 1);
}

void LiquidGlassBackdropTest::chromaticAberrationVanishesOnTheShapeAxes() {
    const QSize size(241, 141);
    const QImage source = coordinatePattern(size);
    LiquidGlassBackdropStyle style = clearLensStyle(size, 26.0, 44.0);

    const QImage withoutDispersion = createLiquidGlassBackdropImage(source, style, source.size());
    style.parameters.chromaticAberration = 1.0;
    const QImage withDispersion = createLiquidGlassBackdropImage(source, style, source.size());

    const QPoint horizontalAxis(10, size.height() / 2);
    const QPoint verticalAxis(size.width() / 2, 10);
    QCOMPARE(withDispersion.pixelColor(horizontalAxis),
             withoutDispersion.pixelColor(horizontalAxis));
    QCOMPARE(withDispersion.pixelColor(verticalAxis), withoutDispersion.pixelColor(verticalAxis));
}

void LiquidGlassBackdropTest::appliesAndroidDefaultDirectionalHighlight() {
    const QSize size(240, 140);
    QImage source(size, QImage::Format_RGBA8888);
    source.fill(QColor(QStringLiteral("#606060")));
    LiquidGlassBackdropStyle style = clearLensStyle(size, 0.0, 0.0);
    constexpr qreal kCornerRadiusRatio = 0.22;
    style.parameters.cornerRadius = size.height() * kCornerRadiusRatio;
    style.highlightStyle = LiquidGlassHighlightStyle::Default;

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    const int cornerExtent = qRound(size.height() * kCornerRadiusRatio);
    const int alignedCorners =
        qMax(maximumRed(material, QRect(0, 0, cornerExtent, cornerExtent)),
             maximumRed(material, QRect(size.width() - cornerExtent, size.height() - cornerExtent,
                                        cornerExtent, cornerExtent)));
    const int crossCorners = qMax(
        maximumRed(material, QRect(size.width() - cornerExtent, 0, cornerExtent, cornerExtent)),
        maximumRed(material, QRect(0, size.height() - cornerExtent, cornerExtent, cornerExtent)));
    QVERIFY2(
        alignedCorners > crossCorners + 8,
        qPrintable(QStringLiteral("aligned=%1 cross=%2").arg(alignedCorners).arg(crossCorners)));
}

void LiquidGlassBackdropTest::appliesAndroidVibrancyBeforeRefraction() {
    const QSize size(64, 64);
    QImage source(size, QImage::Format_RGBA8888);
    source.fill(QColor(80, 120, 160));
    LiquidGlassBackdropStyle style = clearLensStyle(size, 0.0, 0.0);
    style.contentTreatment = LiquidGlassContentTreatment::TonedBackdrop;

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    const qreal inverseSaturation = 1.0 - 1.5;
    const qreal r = 0.213 * inverseSaturation;
    const qreal g = 0.715 * inverseSaturation;
    const qreal b = 0.072 * inverseSaturation;
    const QColor expected(qBound(0, qRound((r + 1.5) * 80 + g * 120 + b * 160), 255),
                          qBound(0, qRound(r * 80 + (g + 1.5) * 120 + b * 160), 255),
                          qBound(0, qRound(r * 80 + g * 120 + (b + 1.5) * 160), 255));
    QCOMPARE(material.pixelColor(size.width() / 2, size.height() / 2), expected);
}

void LiquidGlassBackdropTest::composesSourceVisibilityBeforeRefraction() {
    const QSize size(64, 64);
    QImage source(size, QImage::Format_RGBA8888);
    source.fill(Qt::red);
    LiquidGlassBackdropStyle style = clearLensStyle(size, 0.0, 0.0);
    style.baseColor = QColor(QStringLiteral("#246be8"));
    style.sourceVisibility = 0.0;

    const QImage material = createLiquidGlassBackdropImage(source, style, source.size());

    QCOMPARE(material.pixelColor(size.width() / 2, size.height() / 2), style.baseColor);
}

void LiquidGlassBackdropTest::boundsProcessingResolution() {
    QImage source(2560, 1440, QImage::Format_RGBA8888);
    source.fill(QColor(QStringLiteral("#5b8fd8")));
    LiquidGlassBackdropStyle style;
    style.baseColor = QColor(QStringLiteral("#f4f4f5"));
    style.highlightStyle = LiquidGlassHighlightStyle::None;

    const QImage material = createLiquidGlassBackdropImage(source, style, QSize(2560, 1440));

    QCOMPARE(material.size(), QSize(1600, 900));
}

void LiquidGlassBackdropTest::reusesRasterUntilLensInputsChange() {
    LiquidGlassBackdropCache cache;
    const QImage source = coordinatePattern(QSize(192, 108));
    LiquidGlassBackdropStyle style;
    style.baseColor = QColor(QStringLiteral("#f4f4f5"));

    const qint64 firstKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    const qint64 reusedKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QCOMPARE(reusedKey, firstKey);

    style.parameters.refractionAmountFraction = 0.24;
    const qint64 changedAmountKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QVERIFY(changedAmountKey != firstKey);

    style.depthEffect = false;
    const qint64 changedDepthKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QVERIFY(changedDepthKey != changedAmountKey);

    style.parameters.chromaticAberration = 1.0;
    const qint64 changedDispersionKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QVERIFY(changedDispersionKey != changedDepthKey);

    style.parameters.chromaticAberration = 0.5;
    const qint64 reusedBinaryDispersionKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QCOMPARE(reusedBinaryDispersionKey, changedDispersionKey);

    style.highlightStyle = LiquidGlassHighlightStyle::None;
    const qint64 changedHighlightKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QVERIFY(changedHighlightKey != changedDispersionKey);
}

QTEST_MAIN(LiquidGlassBackdropTest)

#include "tst_LiquidGlassBackdrop.moc"
