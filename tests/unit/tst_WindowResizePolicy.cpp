#include "window/AppDialog.h"

#include <QDialog>
#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>
#include <QtTest>

#include <vkui/window/VkFramelessDialog.h>
#include <vkui/window/VkWindowAgent.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
#import <AppKit/AppKit.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

class WindowResizePolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void nativeResizeIsOptIn();
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
    QCOMPARE(dialog.windowAgent()->systemButtonVisibility(),
             vkui::VkWindowAgent::SystemButtonVisibility::AlwaysVisible);

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    auto* nativeView = reinterpret_cast<NSView*>(dialog.winId());
    QVERIFY(nativeView != nil);
    NSButton* closeButton = [nativeView.window standardWindowButton:NSWindowCloseButton];
    QVERIFY(closeButton != nil);
    QVERIFY(!closeButton.hidden);
    QVERIFY(closeButton.superview != nil);
    QVERIFY(NSIntersectsRect(closeButton.frame, closeButton.superview.bounds));
#endif

    dialog.setCloseButtonPlacement(vkui::VkFramelessDialog::CloseButtonPlacement::Hidden);
    QCOMPARE(dialog.windowAgent()->systemButtonVisibility(),
             vkui::VkWindowAgent::SystemButtonVisibility::AlwaysHidden);
    auto* title =
        dialog.findChild<QLabel*>(QStringLiteral("VkFramelessDialogTitleLabel"));
    auto* fallbackClose =
        dialog.findChild<QToolButton*>(QStringLiteral("VkFramelessDialogCloseButton"));
    QVERIFY(title != nullptr);
    QVERIFY(fallbackClose != nullptr);
    QVERIFY(!fallbackClose->isVisible());
    QCOMPARE(title->parentWidget(), dialog.titleBar());
    QTRY_COMPARE(title->geometry().left(),
                 dialog.titleBar()->layout()->contentsMargins().left());

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QVERIFY(closeButton.hidden);
#endif
}

void WindowResizePolicyTest::nativeResizeIsOptIn() {
    QWidget host;
    host.resize(480, 320);
    vkui::VkWindowAgent agent;
    QVERIFY(!agent.isResizable());
    QVERIFY(agent.setup(&host));
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

    QVERIFY(agent.installSystemButtons());
    auto* closeButton = host.findChild<QPushButton*>(QStringLiteral("qwkWindowsCloseButton"));
    QVERIFY(closeButton);
    QCOMPARE(closeButton->mapTo(&host, closeButton->rect().topRight()), host.rect().topRight());
    closeButton->setDown(true);
    QPixmap closeBackplate(closeButton->size());
    closeBackplate.fill(Qt::transparent);
    closeButton->render(&closeBackplate);
    closeButton->setDown(false);
    QCOMPARE(closeBackplate.toImage().pixelColor(closeBackplate.width() - 1, 0),
             QColor(196, 43, 28));

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

    vkui::VkWindowAgent hostAgent;
    hostAgent.setResizable(true);
    QVERIFY(hostAgent.setup(&host));
    QVERIFY(hostAgent.addTitleBar(&hostTitleBar));
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QCOMPARE(hitTestAt(host, QPoint(240, 22)), static_cast<LRESULT>(HTCAPTION));

    for (int cycle = 0; cycle < 12; ++cycle) {
        QPointer<QDialog> dialog =
            new QDialog(&host, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                                   Qt::WindowCloseButtonHint);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->resize(360, 220);
        auto* dialogTitleBar = new QWidget(dialog);
        dialogTitleBar->setGeometry(0, 0, dialog->width(), 44);

        auto* dialogAgent = new vkui::VkWindowAgent(dialog);
        QVERIFY(!dialog->internalWinId());
        QVERIFY(dialogAgent->setup(dialog));
        QVERIFY(!dialog->internalWinId());
        QVERIFY(dialogAgent->installSystemButtons());
        QVERIFY(!dialog->internalWinId());
        QVERIFY(dialogAgent->addTitleBar(dialogTitleBar));

        // Agent setup must never hook the already-native owner while the dialog is still alien.
        QCOMPARE(hitTestAt(host, QPoint(240, 22)), static_cast<LRESULT>(HTCAPTION));

        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog));
        QVERIFY(dialog->internalWinId());
        QTRY_VERIFY(!nativeWindowEnabled(host));

        auto* closeButton =
            dialog->findChild<QPushButton*>(QStringLiteral("qwkWindowsCloseButton"));
        QVERIFY(closeButton);
        QTest::mouseClick(closeButton, Qt::LeftButton);
        QTRY_VERIFY(dialog.isNull());

        // Destroying a modal transient must synchronously release its owner and preserve the
        // original context across repeated native-window lifecycles.
        QTRY_VERIFY(nativeWindowEnabled(host));
        QCOMPARE(hitTestAt(host, QPoint(240, 22)), static_cast<LRESULT>(HTCAPTION));
    }
#else
    QSKIP("The native owner-window regression is specific to the Windows HWND backend.");
#endif
}

QTEST_MAIN(WindowResizePolicyTest)

#include "tst_WindowResizePolicy.moc"
