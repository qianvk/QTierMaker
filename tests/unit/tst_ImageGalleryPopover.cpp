#include "tier/ImageGalleryPopover.h"

#include <QPushButton>
#include <QShortcut>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

using namespace tlm;

class ImageGalleryPopoverTest final : public QObject {
    Q_OBJECT

private slots:
    void spacePreviewSurvivesTemporaryPopoverSuspension();
};

void ImageGalleryPopoverTest::spacePreviewSurvivesTemporaryPopoverSuspension() {
    QWidget host;
    host.resize(720, 480);
    QPushButton anchor(QStringLiteral("Gallery"), &host);
    anchor.setGeometry(24, 24, 100, 32);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    TierProject project;
    TierImage image;
    image.id = QStringLiteral("selected-image");
    image.displayName = QStringLiteral("Selected image");
    project.images.append(image);

    auto* popover = new ImageGalleryPopover(&host);
    popover->setData(&project, nullptr, nullptr, image.id);
    popover->openFor(&anchor);
    QTRY_VERIFY(popover->isOpen());

    QWidget* grid = popover->findChild<QWidget*>(QStringLiteral("ImageGalleryGrid"));
    QVERIFY(grid);
    QShortcut* previewShortcut =
        grid->window()->findChild<QShortcut*>(QStringLiteral("ImageGalleryPreviewShortcut"));
    QVERIFY(previewShortcut);
    QCOMPARE(previewShortcut->key(), QKeySequence(Qt::Key_Space));
    QSignalSpy previewSpy(popover, &ImageGalleryPopover::imagePreviewRequested);
    QSignalSpy closedSpy(popover, &ImageGalleryPopover::closed);

    grid->setFocus(Qt::OtherFocusReason);
    QTest::keyClick(grid, Qt::Key_Space);
    QCOMPARE(previewSpy.count(), 1);
    QCOMPARE(previewSpy.constFirst().at(0).toString(), image.id);

    QVERIFY(popover->suspendForPreview());
    QVERIFY(!popover->isOpen());
    QCOMPARE(closedSpy.count(), 0);

    QVERIFY(popover->restoreAfterPreview());
    QTRY_VERIFY(popover->isOpen());
    QVERIFY(QMetaObject::invokeMethod(previewShortcut, "activated", Qt::DirectConnection));
    QCOMPARE(previewSpy.count(), 2);
    QCOMPARE(previewSpy.constLast().at(0).toString(), image.id);

    popover->closeImmediately();
    QCOMPARE(closedSpy.count(), 1);
}

QTEST_MAIN(ImageGalleryPopoverTest)

#include "tst_ImageGalleryPopover.moc"
