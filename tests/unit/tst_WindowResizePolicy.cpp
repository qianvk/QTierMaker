#include "window/AppDialog.h"

#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QPushButton>
#include <QWidget>
#include <QWindow>
#include <QtTest>

#include <vkui/window/VWindowAgent.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
#import <AppKit/AppKit.h>
#elif defined(Q_OS_WIN)
#include <dwmapi.h>
#include <qt_windows.h>
#endif

class WindowResizePolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void nativeResizePolicyTracksExplicitSetting();
    void dialogClosePlacementTracksPolicy();
    void transientAgentNeverClaimsOwnerWindow();
    void nativeStateSurvivesGeometryChanges();
    void fullContentChromeTracksPolicyAndPalette();
};

#if defined(Q_OS_WIN)
namespace {
LRESULT hitTestAt(QWidget& host, const QPoint& clientPosition) {
    const auto hwnd = reinterpret_cast<HWND>(host.winId());
    POINT nativePosition{clientPosition.x(), clientPosition.y()};
    ::ClientToScreen(hwnd, &nativePosition);
    return ::SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(nativePosition.x, nativePosition.y));
}

bool nativeWindowEnabled(QWidget& host) {
    return ::IsWindowEnabled(reinterpret_cast<HWND>(host.winId()));
}
} // namespace
#endif

void WindowResizePolicyTest::nativeStateSurvivesGeometryChanges() {
#if defined(Q_OS_WIN)
    QWidget host;
    vkui::VWindowAgent agent(host);
    host.resize(640, 420);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    const auto hwnd = reinterpret_cast<HWND>(host.winId());
    host.move(host.pos() + QPoint(20, 20));
    host.resize(660, 440);
    QTRY_VERIFY(::IsWindowVisible(hwnd));

    // Modal owners must stay disabled even if layout or DPI changes resize them.
    ::EnableWindow(hwnd, FALSE);
    host.resize(680, 460);
    agent.setSystemButtons(vkui::VSystemButton::Close);
    QCoreApplication::processEvents();
    QVERIFY(!::IsWindowEnabled(hwnd));
    agent.setSystemButtons(vkui::VStandardSystemButtons);
    QVERIFY(!::IsWindowEnabled(hwnd));
    ::EnableWindow(hwnd, TRUE);

    host.showMaximized();
    QTRY_VERIFY(::IsZoomed(hwnd));
    QVERIFY(::IsWindowVisible(hwnd));
    agent.setSystemButtonsVisible(false);
    QVERIFY(::IsZoomed(hwnd));
    agent.setSystemButtonsVisible(true);
    QVERIFY(::IsZoomed(hwnd));
    MONITORINFO monitor{sizeof(MONITORINFO)};
    QVERIFY(::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
    const auto fitsWorkArea = [&] {
        RECT rect{};
        ::GetClientRect(hwnd, &rect);
        ::MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
        return rect.left == monitor.rcWork.left && rect.top == monitor.rcWork.top &&
               rect.right == monitor.rcWork.right && rect.bottom == monitor.rcWork.bottom;
    };
    QTRY_VERIFY(fitsWorkArea());
    host.showNormal();
    QTRY_VERIFY(!::IsZoomed(hwnd));
    host.showMinimized();
    QTRY_VERIFY(::IsIconic(hwnd));
    host.showNormal();
    QTRY_VERIFY(!::IsIconic(hwnd));
    QVERIFY(::IsWindowVisible(hwnd));

    // Compare the real client rectangle with Qt's platform geometry, not only QWidget state.
    host.resize(700, 480);
    const auto geometryAgrees = [&] {
        RECT rect{};
        ::GetClientRect(hwnd, &rect);
        const QSize expected = host.windowHandle()->size() * host.devicePixelRatioF();
        return qAbs(rect.right - expected.width()) <= 2 &&
               qAbs(rect.bottom - expected.height()) <= 2;
    };
    QTRY_VERIFY(geometryAgrees());
    QCOMPARE(host.size(), QSize(700, 480));

    QPushButton button(QStringLiteral("Still interactive"), &host);
    button.setGeometry(40, 80, 180, 40);
    button.show();
    QSignalSpy clicked(&button, &QPushButton::clicked);
    const QPoint point = button.geometry().center() * host.devicePixelRatioF();
    const LPARAM position = MAKELPARAM(point.x(), point.y());
    ::SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, position);
    ::SendMessageW(hwnd, WM_LBUTTONUP, 0, position);
    QTRY_COMPARE(clicked.count(), 1);
#else
    QSKIP("Windows native style regression.");
#endif
}

void WindowResizePolicyTest::fullContentChromeTracksPolicyAndPalette() {
#if defined(Q_OS_WIN)
    QWidget host;
    vkui::VWindowAgent agent(host);
    host.resize(640, 420);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QCOMPARE(host.contentsMargins().top(), 0);
    // Qt's expanded-client renderer must not replace DWM caption controls.
    QVERIFY(!host.windowHandle()->flags().testFlag(Qt::ExpandedClientAreaHint));
    QVERIFY(host.windowHandle()->flags().testFlag(Qt::NoTitleBarBackgroundHint));
    QVERIFY(host.windowHandle()->flags().testFlag(Qt::WindowCloseButtonHint));

    const auto hwnd = reinterpret_cast<HWND>(host.winId());
    // DWM must remain eligible for native corners and captions, rather than a
    // layered frameless window with custom-painted controls.
    QVERIFY(!(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED));
    QVERIFY(!host.testAttribute(Qt::WA_NoSystemBackground));
    QCOMPARE(host.grab().toImage().pixelColor(20, 80).alpha(), 255);
    auto* nativeSurface = host.findChild<QWidget*>(QStringLiteral("VkNativeCaptionSurface"));
    QVERIFY(nativeSurface);
    QVERIFY(nativeSurface->isVisible());
    QTRY_COMPARE(nativeSurface->geometry().right() + 1, host.width());
    const auto closeAtRightEdge = [&] {
        RECT client{}, frame{}, buttons{};
        ::GetClientRect(hwnd, &client);
        ::GetWindowRect(hwnd, &frame);
        ::DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &buttons, sizeof(buttons));
        POINT right{frame.left + buttons.right, frame.top + (buttons.top + buttons.bottom) / 2};
        ::ScreenToClient(hwnd, &right);
        // DWM may retain the native thin frame on the last physical pixel when maximized.
        const int thinFrame = ::GetSystemMetricsForDpi(SM_CXBORDER, ::GetDpiForWindow(hwnd));
        if (client.right - right.x > thinFrame) {
            return false;
        }
        return hitTestAt(host, QPoint(qMin(client.right, right.x) - 1, right.y)) == HTCLOSE;
    };
    QTRY_VERIFY(closeAtRightEdge());
    QVERIFY(nativeSurface->testAttribute(Qt::WA_TransparentForMouseEvents));
    const QImage captionImage = nativeSurface->grab().toImage();
    QCOMPARE(captionImage.pixelColor(captionImage.width() / 2, captionImage.height() / 2).alpha(),
             0);
    RECT buttons{};
    QVERIFY(SUCCEEDED(
        ::DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &buttons, sizeof(buttons))));
    RECT frame{};
    QVERIFY(::GetWindowRect(hwnd, &frame));
    const int buttonWidth = (buttons.right - buttons.left) / 3;
    const POINT maximum{frame.left + buttons.right - buttonWidth * 3 / 2,
                        frame.top + (buttons.top + buttons.bottom) / 2};
    QCOMPARE(::SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(maximum.x, maximum.y)),
             static_cast<LRESULT>(HTMAXBUTTON));
    agent.setSystemButtonsVisible(false);
    QVERIFY(!nativeSurface->isVisible());
    QVERIFY(!host.windowHandle()->flags().testFlag(Qt::WindowCloseButtonHint));
    QVERIFY(!host.windowHandle()->flags().testFlag(Qt::WindowMinMaxButtonsHint));
    agent.setSystemButtonsVisible(true);
    QVERIFY(nativeSurface->isVisible());
    QVERIFY(host.windowHandle()->flags().testFlag(Qt::WindowCloseButtonHint));

    host.resize(720, 460);
    QTRY_COMPARE(nativeSurface->geometry().right() + 1, host.width());
    QTRY_VERIFY(closeAtRightEdge());
    host.showMaximized();
    QTRY_VERIFY(::IsZoomed(hwnd));
    QTRY_COMPARE(nativeSurface->geometry().right() + 1, host.width());
    QTRY_VERIFY(closeAtRightEdge());
    host.showNormal();
    QTRY_VERIFY(!::IsZoomed(hwnd));

    DWORD corner = 0;
    const HRESULT supported = ::DwmGetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
    if (SUCCEEDED(supported)) {
        QCOMPARE(corner, DWORD(2));
    }
    QPalette palette = host.palette();
    palette.setColor(QPalette::Window, QColor(24, 24, 24));
    palette.setColor(QPalette::WindowText, Qt::white);
    host.setPalette(palette);
    BOOL dark = FALSE;
    const HRESULT darkSupported = ::DwmGetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    if (SUCCEEDED(darkSupported)) {
        QTRY_VERIFY((::DwmGetWindowAttribute(hwnd, 20, &dark, sizeof(dark)), dark));
        palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::WindowText, Qt::black);
        host.setPalette(palette);
        QTRY_VERIFY((::DwmGetWindowAttribute(hwnd, 20, &dark, sizeof(dark)), !dark));
    }
#else
    QSKIP("Windows full-content chrome regression.");
#endif
}

void WindowResizePolicyTest::dialogClosePlacementTracksPolicy() {
    QWidget host;
    host.resize(960, 640);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    qtm::AppDialog dialog(QStringLiteral("Preferences"), &host);
    dialog.setWindowModality(Qt::NonModal);
    dialog.resize(520, 360);
    dialog.setResizable(true);
    dialog.setCloseButtonPlacement(qtm::AppDialog::CloseButtonPlacement::Platform);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCOMPARE(dialog.size(), QSize(520, 360));
    QVERIFY(dialog.windowAgent()->systemButtonsVisible());

    dialog.resize(600, 400);
    QCoreApplication::processEvents();
    QCOMPARE(dialog.size(), QSize(600, 400));

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    auto* nativeView = reinterpret_cast<NSView*>(dialog.winId());
    QVERIFY(nativeView != nil);
    NSButton* closeButton = [nativeView.window standardWindowButton:NSWindowCloseButton];
    QVERIFY(closeButton != nil);
    QVERIFY(!closeButton.hidden);
    QVERIFY(closeButton.superview != nil);
    QVERIFY(NSIntersectsRect(closeButton.frame, closeButton.superview.bounds));
#endif

    dialog.setCloseButtonPlacement(qtm::AppDialog::CloseButtonPlacement::Hidden);
    QVERIFY(!dialog.windowAgent()->systemButtonsVisible());
    QCoreApplication::processEvents();
    QCOMPARE(dialog.size(), QSize(600, 400));
    auto* title = dialog.findChild<QLabel*>(QStringLiteral("AppDialogTitleLabel"));
    QVERIFY(title != nullptr);
    QCOMPARE(title->parentWidget(), dialog.titleBar());
    QTRY_COMPARE(title->geometry().left(), dialog.titleBar()->layout()->contentsMargins().left());

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QVERIFY(closeButton.hidden);
#endif
}

void WindowResizePolicyTest::nativeResizePolicyTracksExplicitSetting() {
    QWidget host;
    host.resize(480, 320);
    vkui::VWindowAgent agent(host);
    agent.setResizable(false);
    QVERIFY(!agent.isResizable());
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    auto* nativeView = reinterpret_cast<NSView*>(host.winId());
    QVERIFY(nativeView != nil);
    NSWindow* nativeWindow = nativeView.window;
    QVERIFY(nativeWindow != nil);
    QVERIFY(!(nativeWindow.styleMask & NSWindowStyleMaskResizable));

    agent.setResizable(true);
    QCoreApplication::processEvents();
    QVERIFY(nativeWindow.styleMask & NSWindowStyleMaskResizable);

    agent.setResizable(false);
    QCoreApplication::processEvents();
    QVERIFY(!(nativeWindow.styleMask & NSWindowStyleMaskResizable));
#elif defined(Q_OS_WIN)
    const auto hwnd = reinterpret_cast<HWND>(host.winId());
    const auto style = [hwnd]() { return ::GetWindowLongPtrW(hwnd, GWL_STYLE); };
    QVERIFY(!(style() & (WS_THICKFRAME | WS_MAXIMIZEBOX)));

    agent.setResizable(true);
    QCoreApplication::processEvents();
    QVERIFY(style() & WS_THICKFRAME);

    agent.setResizable(false);
    QCoreApplication::processEvents();
    QVERIFY(!(style() & (WS_THICKFRAME | WS_MAXIMIZEBOX)));
#else
    agent.setResizable(true);
    QVERIFY(agent.isResizable());
    agent.setResizable(false);
    QVERIFY(!agent.isResizable());
#endif
}

void WindowResizePolicyTest::transientAgentNeverClaimsOwnerWindow() {
#if defined(Q_OS_WIN)
    QWidget host;
    host.resize(640, 420);
    QWidget hostTitleBar(&host);
    hostTitleBar.setGeometry(0, 0, host.width(), 44);

    vkui::VWindowAgent hostAgent(host);
    hostAgent.setResizable(true);
    hostAgent.setSystemButtons(vkui::VStandardSystemButtons);
    QVERIFY(hostAgent.addTitleBar(&hostTitleBar));
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QCOMPARE(hitTestAt(host, QPoint(240, 22)), static_cast<LRESULT>(HTCAPTION));

    for (int cycle = 0; cycle < 12; ++cycle) {
        QPointer<qtm::AppDialog> dialog = new qtm::AppDialog(QStringLiteral("Transient"), &host);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->setCloseButtonPlacement(qtm::AppDialog::CloseButtonPlacement::Platform);
        dialog->resize(360, 220);

        QVERIFY(dialog->internalWinId());
        QVERIFY(dialog->internalWinId() != host.internalWinId());
        QCOMPARE(hitTestAt(host, QPoint(240, 22)), static_cast<LRESULT>(HTCAPTION));

        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog));
        QVERIFY(dialog->internalWinId());
        QTRY_VERIFY(!nativeWindowEnabled(host));

        const HWND dialogWindow = reinterpret_cast<HWND>(dialog->winId());
        QVERIFY(::PostMessageW(dialogWindow, WM_CLOSE, 0, 0));
        QTRY_VERIFY(dialog.isNull());

        QTRY_VERIFY(nativeWindowEnabled(host));
        QCOMPARE(hitTestAt(host, QPoint(240, 22)), static_cast<LRESULT>(HTCAPTION));
    }
#else
    QSKIP("The native owner-window regression is specific to the Windows HWND backend.");
#endif
}

QTEST_MAIN(WindowResizePolicyTest)

#include "tst_WindowResizePolicy.moc"
