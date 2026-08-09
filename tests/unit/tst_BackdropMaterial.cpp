#include "assets/BackdropMaterial.h"

#include <QTest>

using namespace qtm;

namespace {
QImage patternedSource() {
    QImage source(192, 108, QImage::Format_RGBA8888);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y,
                                 ((x / 8 + y / 8) % 2) == 0 ? QColor(QStringLiteral("#f7c948"))
                                                            : QColor(QStringLiteral("#2764d8")));
        }
    }
    return source;
}
} // namespace

class BackdropMaterialTest final : public QObject {
    Q_OBJECT

private slots:
    void dispatchesAndCachesSelectedMaterial();
};

void BackdropMaterialTest::dispatchesAndCachesSelectedMaterial() {
    BackdropMaterialCache cache;
    const QImage source = patternedSource();
    const BackdropMaterialStyle style{QColor(QStringLiteral("#f4f4f5")), 1.0, false};
    const QSize logicalSize(320, 180);

    const QPixmap focus = cache.pixmap(BackdropEffect::DepthSoftFocus, QStringLiteral("preview"),
                                       source, style, logicalSize, 1.0);
    QVERIFY(!focus.isNull());
    QCOMPARE(focus.size(), logicalSize);
    const qint64 reusedFocusKey =
        cache
            .pixmap(BackdropEffect::DepthSoftFocus, QStringLiteral("preview"), source, style,
                    logicalSize, 1.0)
            .cacheKey();
    QCOMPARE(reusedFocusKey, focus.cacheKey());

    const QPixmap liquid = cache.pixmap(BackdropEffect::LiquidGlass, QStringLiteral("preview"),
                                        source, style, logicalSize, 1.0);
    QVERIFY(!liquid.isNull());
    QCOMPARE(liquid.size(), logicalSize);
    QVERIFY(liquid.toImage() != focus.toImage());

    BackdropMaterialStyle imageStyle{QColor(QStringLiteral("#f4f4f5")), 1.0, false,
                                     BackdropMaterialPurpose::GlassOverlay};
    imageStyle.liquidGlass.cornerRadius = 4.0;
    imageStyle.liquidGlass.refractionHeightFraction = 0.08;
    imageStyle.liquidGlass.refractionAmountFraction = 0.08;
    imageStyle.drawHighlight = true;
    const QPixmap imageSurface =
        cache.pixmap(BackdropEffect::LiquidGlass, QStringLiteral("preview"), source, imageStyle,
                     logicalSize, 1.0);
    QVERIFY(!imageSurface.isNull());
    QVERIFY(imageSurface.toImage() != liquid.toImage());
}

QTEST_MAIN(BackdropMaterialTest)

#include "tst_BackdropMaterial.moc"
