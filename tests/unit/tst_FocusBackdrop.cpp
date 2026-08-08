#include "assets/FocusBackdrop.h"

#include <QTest>

#include <algorithm>
#include <cmath>

using namespace tlm;

namespace {
qreal luminance(const QColor& color) {
    const auto linearChannel = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linearChannel(color.redF()) + 0.7152 * linearChannel(color.greenF()) +
           0.0722 * linearChannel(color.blueF());
}

qreal averageLuminance(const QImage& image) {
    qreal sum = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            sum += luminance(image.pixelColor(x, y));
        }
    }
    return sum / qMax<qreal>(1.0, static_cast<qreal>(image.width()) * image.height());
}

qreal averageHorizontalContrast(const QImage& image) {
    qreal sum = 0.0;
    int samples = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 1; x < image.width(); ++x) {
            sum +=
                std::abs(luminance(image.pixelColor(x, y)) - luminance(image.pixelColor(x - 1, y)));
            ++samples;
        }
    }
    return sum / qMax(1, samples);
}

qreal averageChroma(const QImage& image) {
    qreal sum = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            const qreal minimum = std::min({color.redF(), color.greenF(), color.blueF()});
            const qreal maximum = std::max({color.redF(), color.greenF(), color.blueF()});
            sum += maximum - minimum;
        }
    }
    return sum / qMax<qreal>(1.0, static_cast<qreal>(image.width()) * image.height());
}

QImage highFrequencySource() {
    QImage source(128, 96, QImage::Format_RGBA8888);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y,
                                 ((x / 2 + y / 2) % 2) == 0 ? QColor(QStringLiteral("#ffffff"))
                                                            : QColor(QStringLiteral("#0068ff")));
        }
    }
    return source;
}
} // namespace

class FocusBackdropTest final : public QObject {
    Q_OBJECT

private slots:
    void suppressesBrightnessDetailAndChroma();
    void adaptsToneToAppearance();
    void boundsProcessingResolution();
    void reusesRasterUntilInputsChange();
};

void FocusBackdropTest::suppressesBrightnessDetailAndChroma() {
    const QImage source = highFrequencySource();
    const FocusBackdropStyle style{QColor(QStringLiteral("#f4f4f5")), 1.0, false};

    const QImage backdrop = createFocusBackdropImage(source, style);

    QCOMPARE(backdrop.size(), source.size());
    QCOMPARE(backdrop.format(), QImage::Format_RGBA8888);
    QVERIFY(averageLuminance(backdrop) < averageLuminance(source) * 0.90);
    QVERIFY(averageHorizontalContrast(backdrop) < averageHorizontalContrast(source) * 0.25);
    QVERIFY(averageChroma(backdrop) < averageChroma(source) * 0.70);
    QCOMPARE(backdrop.pixelColor(0, 0).alpha(), 255);
}

void FocusBackdropTest::adaptsToneToAppearance() {
    const QImage source = highFrequencySource();
    const QImage light =
        createFocusBackdropImage(source, {QColor(QStringLiteral("#f4f4f5")), 1.0, false});
    const QImage dark =
        createFocusBackdropImage(source, {QColor(QStringLiteral("#202124")), 1.0, true});

    QVERIFY(averageLuminance(dark) < averageLuminance(light));
}

void FocusBackdropTest::boundsProcessingResolution() {
    QImage source(2560, 1440, QImage::Format_RGBA8888);
    source.fill(QColor(QStringLiteral("#5b8fd8")));

    const QImage backdrop =
        createFocusBackdropImage(source, {QColor(QStringLiteral("#f4f4f5")), 1.0, false});

    QCOMPARE(backdrop.size(), QSize(1280, 720));
}

void FocusBackdropTest::reusesRasterUntilInputsChange() {
    FocusBackdropCache cache;
    const QImage source = highFrequencySource();
    FocusBackdropStyle style{QColor(QStringLiteral("#f4f4f5")), 1.0, false};

    const qint64 firstKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    const qint64 reusedKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QCOMPARE(reusedKey, firstKey);

    style.sourceVisibility = 0.5;
    const qint64 changedKey =
        cache.pixmap(QStringLiteral("background"), source, style, QSize(320, 180), 2.0).cacheKey();
    QVERIFY(changedKey != firstKey);
}

QTEST_MAIN(FocusBackdropTest)

#include "tst_FocusBackdrop.moc"
