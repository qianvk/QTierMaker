#include "window/AppDialog.h"

#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QWidget>
#include <QtTest>

#include <vkui/window/VWindowAgent.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
#import <AppKit/AppKit.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

class WindowResizePolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void nativeResizePolicyTracksExplicitSetting();
    void dialogClosePlacementTracksPolicy();
    void transientAgentNeverClaimsOwnerWindow();
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
    QTRY_COMPARE(title->geometry().left(),
                 dialog.titleBar()->layout()->contentsMargins().left());

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
