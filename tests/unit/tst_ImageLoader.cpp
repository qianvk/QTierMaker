#include "assets/ImageLoader.h"

#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace qtm;

class ImageLoaderTest final : public QObject {
    Q_OBJECT

private slots:
    void decodesOversizedImageAtDisplaySize();
    void doesNotUpscaleSmallImage();
};

void ImageLoaderTest::decodesOversizedImageAtDisplaySize() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("portrait.jpg"));
    QImage source(QSize(2400, 3200), QImage::Format_RGB32);
    source.fill(QColor(48, 96, 160));
    QVERIFY(source.save(path, "JPG", 92));

    auto loaded = ImageLoader::load(path, QSize(600, 600));
    QVERIFY(loaded.hasValue());
    QCOMPARE(loaded.value().size(), QSize(450, 600));
}

void ImageLoaderTest::doesNotUpscaleSmallImage() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("small.png"));
    QImage source(QSize(320, 180), QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor(80, 120, 180));
    QVERIFY(source.save(path));

    auto loaded = ImageLoader::load(path, QSize(1920, 1080));
    QVERIFY(loaded.hasValue());
    QCOMPARE(loaded.value().size(), source.size());
}

QTEST_MAIN(ImageLoaderTest)

#include "tst_ImageLoader.moc"
