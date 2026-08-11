#include "pages/ProjectLocationDialog.h"
#include "preview/PreviewOverlay.h"
#include "window/AppDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QtTest>

#include <vkui/window/VkWindowAgent.h>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

using namespace qtm;

class WindowLifecycleTest final : public QObject {
    Q_OBJECT

private slots:
    void editDialogsLeadWithTitle();
    void projectLocationDialogFitsDynamicOption();
    void dialogClosePreservesWindowInput();
};

void WindowLifecycleTest::editDialogsLeadWithTitle() {
    AppDialog dialog(QStringLiteral("Edit Image"));
    dialog.resize(480, 320);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCOMPARE(dialog.closeButtonPlacement(), AppDialog::CloseButtonPlacement::Hidden);

    auto* title =
        dialog.findChild<QLabel*>(QStringLiteral("VkFramelessDialogTitleLabel"));
    QVERIFY(title != nullptr);
    QCOMPARE(title->parentWidget(), dialog.titleBar());
    QTRY_COMPARE(title->geometry().left(),
                 dialog.titleBar()->layout()->contentsMargins().left());
    QCOMPARE(dialog.windowAgent()->systemButtonVisibility(),
             vkui::VkWindowAgent::SystemButtonVisibility::AlwaysHidden);
}

void WindowLifecycleTest::projectLocationDialogFitsDynamicOption() {
    const QString defaultDirectory = QDir::temp().filePath(QStringLiteral("qtm-default-location"));
    const QString alternateDirectory = QDir::temp().filePath(QStringLiteral(
        "qtm-changed-location-with-a-deliberately-long-parent-folder-name-for-wrapping"));
    ProjectLocationDialog dialog(QStringLiteral("Layout Test"), defaultDirectory,
                                 defaultDirectory);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    auto* directoryEdit =
        dialog.findChild<QLineEdit*>(QStringLiteral("ProjectLocationDirectoryEdit"));
    auto* defaultCheck = dialog.findChild<QCheckBox*>(
        QStringLiteral("ProjectLocationDefaultDirectoryCheck"));
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    QVERIFY(directoryEdit != nullptr);
    QVERIFY(defaultCheck != nullptr);
    QVERIFY(buttons != nullptr);
    QVERIFY(defaultCheck->isHidden());
    const int collapsedHeight = dialog.height();
    const int stableWidth = dialog.width();

    directoryEdit->setText(QDir::toNativeSeparators(alternateDirectory));
    QTRY_VERIFY(defaultCheck->isVisible());
    QTRY_VERIFY(dialog.height() > collapsedHeight);
    QCOMPARE(dialog.width(), stableWidth);

    const int checkBottom = defaultCheck->mapTo(&dialog, defaultCheck->rect().bottomLeft()).y();
    const int buttonTop = buttons->mapTo(&dialog, buttons->rect().topLeft()).y();
    const int buttonBottom = buttons->mapTo(&dialog, buttons->rect().bottomLeft()).y();
    QVERIFY2(buttonTop > checkBottom, "The dynamic default-folder option overlaps the buttons.");
    QVERIFY2(buttonBottom < dialog.height(), "The dialog clips its action buttons.");

    directoryEdit->setText(QDir::toNativeSeparators(defaultDirectory));
    QTRY_VERIFY(defaultCheck->isHidden());
    QTRY_COMPARE(dialog.height(), collapsedHeight);
    QCOMPARE(dialog.width(), stableWidth);
}

#if defined(Q_OS_WIN)
namespace {

LPARAM screenPointParameter(const QPoint& screenPosition) {
    return MAKELPARAM(screenPosition.x(), screenPosition.y());
}

POINT nativeScreenPosition(QWidget& host, const QPoint& hostPosition) {
    const qreal scale = host.devicePixelRatioF();
    POINT position{qRound(hostPosition.x() * scale), qRound(hostPosition.y() * scale)};
    ::ClientToScreen(reinterpret_cast<HWND>(host.winId()), &position);
    return position;
}

LRESULT hitTestAt(QWidget& host, const QPoint& hostPosition) {
    const HWND hwnd = reinterpret_cast<HWND>(host.winId());
    const POINT screenPosition = nativeScreenPosition(host, hostPosition);
    return ::SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(screenPosition.x, screenPosition.y));
}

QPoint findCaptionPoint(QWidget& host) {
    for (int y = 12; y < 40; y += 4) {
        for (int x = 120; x < host.width() - 180; x += 24) {
            if (hitTestAt(host, QPoint(x, y)) == HTCAPTION) {
                return QPoint(x, y);
            }
        }
    }
    return {};
}

void clickNativeCaptionClose(AppDialog& dialog) {
    auto* closeButton = dialog.findChild<QPushButton*>(QStringLiteral("qwkWindowsCloseButton"));
    QVERIFY(closeButton);

    const QPoint dialogPosition = closeButton->mapTo(&dialog, closeButton->rect().center());
    const POINT nativePosition = nativeScreenPosition(dialog, dialogPosition);
    const QPoint screenPosition(nativePosition.x, nativePosition.y);
    const HWND hwnd = reinterpret_cast<HWND>(dialog.winId());
    const LPARAM position = screenPointParameter(screenPosition);
    QCOMPARE(::SendMessageW(hwnd, WM_NCHITTEST, 0, position), static_cast<LRESULT>(HTCLOSE));
    QTest::mouseClick(closeButton, Qt::LeftButton);
}

void clickClientPoint(QWidget& host, const QPoint& hostPosition) {
    const HWND hwnd = reinterpret_cast<HWND>(host.winId());
    const qreal scale = host.devicePixelRatioF();
    POINT clientPosition{qRound(hostPosition.x() * scale), qRound(hostPosition.y() * scale)};
    const LPARAM position = MAKELPARAM(clientPosition.x, clientPosition.y);
    ::SendMessageW(hwnd, WM_MOUSEMOVE, 0, position);
    ::SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, position);
    ::SendMessageW(hwnd, WM_LBUTTONUP, 0, position);
}

} // namespace
#endif

void WindowLifecycleTest::dialogClosePreservesWindowInput() {
#if defined(Q_OS_WIN)
    QWidget host;
    host.resize(960, 640);
    QWidget sidebarTitleBar(&host);
    sidebarTitleBar.setGeometry(0, 0, 240, 44);
    QWidget contentTitleBar(&host);
    contentTitleBar.setGeometry(240, 0, host.width() - 240, 44);
    PreviewOverlay preview(&host);
    preview.setGeometry(host.rect());

    vkui::VkWindowAgent hostAgent;
    hostAgent.setResizable(true);
    QVERIFY(hostAgent.setup(&host));
    QVERIFY(hostAgent.installSystemButtons());
    QVERIFY(hostAgent.addTitleBar(&sidebarTitleBar));
    QVERIFY(hostAgent.addTitleBar(&contentTitleBar));
    QVERIFY(hostAgent.setHitTestVisible(&sidebarTitleBar, &preview, true));
    QVERIFY(hostAgent.setHitTestVisible(&contentTitleBar, &preview, true));

    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    const QPoint captionPoint = findCaptionPoint(host);
    QVERIFY(!captionPoint.isNull());
    QCOMPARE(hitTestAt(host, captionPoint), static_cast<LRESULT>(HTCAPTION));

    for (int cycle = 0; cycle < 12; ++cycle) {
        QPointer<AppDialog> dialog = new AppDialog(QStringLiteral("Lifecycle"), &host);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        // App dialogs hide caption controls by default; this regression exercises the native
        // close path explicitly.
        dialog->setCloseButtonPlacement(AppDialog::CloseButtonPlacement::Platform);
        dialog->resize(520, 360);

        // Installing custom caption controls must not create the platform window before show().
        QVERIFY(!dialog->internalWinId());
        dialog->show();
        QTRY_VERIFY(qApp->activeModalWidget());
        QCOMPARE(qApp->activeModalWidget(), dialog.data());
        QVERIFY(QTest::qWaitForWindowExposed(dialog));
        QVERIFY(!::IsWindowEnabled(reinterpret_cast<HWND>(host.winId())));

        clickNativeCaptionClose(*dialog);
        QTRY_VERIFY(dialog.isNull());
        QTRY_VERIFY(!qApp->activeModalWidget());
        QTRY_VERIFY(::IsWindowEnabled(reinterpret_cast<HWND>(host.winId())));
        QVERIFY(!QWidget::mouseGrabber());
        QVERIFY(!QWidget::keyboardGrabber());
        QCOMPARE(hitTestAt(host, captionPoint), static_cast<LRESULT>(HTCAPTION));
    }

    QPixmap pixmap(240, 160);
    pixmap.fill(Qt::red);
    preview.openPreview(QRect(host.rect().center(), QSize(40, 30)), pixmap);
    QTRY_VERIFY(preview.isOpen());
    QVERIFY(!QWidget::mouseGrabber());
    QVERIFY(!QWidget::keyboardGrabber());

    const QPoint outsideInOverlay(8, preview.height() - 8);
    const QPoint outsideInHost = preview.mapTo(&host, outsideInOverlay);
    QCOMPARE(hitTestAt(host, outsideInHost), static_cast<LRESULT>(HTCLIENT));
    QVERIFY(!preview.toolTipTextAt(outsideInOverlay).isEmpty());

    clickClientPoint(host, outsideInHost);
    QTRY_VERIFY(!preview.isOpen());

    preview.openPreview(QRect(host.rect().center(), QSize(40, 30)), pixmap);
    QTRY_VERIFY(preview.isOpen());
    const QPoint imagePosition = preview.rect().center();
    QVERIFY(preview.toolTipTextAt(imagePosition) != preview.toolTipTextAt(outsideInOverlay));
    QTest::mouseDClick(&preview, Qt::LeftButton, Qt::NoModifier, imagePosition);
    QTRY_VERIFY(!preview.isOpen());
#else
    QSKIP("The native AppDialog lifecycle regression is specific to Windows.");
#endif
}

QTEST_MAIN(WindowLifecycleTest)

#include "tst_WindowLifecycle.moc"
