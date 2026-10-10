#include "widgets/PlatformDialogButtonBox.h"

#include <QAbstractButton>
#include <QApplication>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest>

#include <vkui/Widgets.h>

namespace {

QAbstractButton* buttonForRole(QDialogButtonBox* box, QDialogButtonBox::ButtonRole role) {
    for (QAbstractButton* button : box->buttons()) {
        if (box->buttonRole(button) == role) {
            return button;
        }
    }
    return nullptr;
}

int horizontalCenter(const QWidget* widget, const QWidget* ancestor) {
    return widget->mapTo(ancestor, widget->rect().center()).x();
}

} // namespace

class DialogButtonOrderTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void confirmCancelUsesPlatformOrder();
    void saveDiscardCancelUsesPlatformOrder();
};

void DialogButtonOrderTest::initTestCase() {
    vkui::installVkUi(*qApp);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QCOMPARE(qApp->style()->styleHint(QStyle::SH_DialogButtonLayout),
             static_cast<int>(QDialogButtonBox::MacLayout));
#elif defined(Q_OS_WIN)
    QCOMPARE(qApp->style()->styleHint(QStyle::SH_DialogButtonLayout),
             static_cast<int>(QDialogButtonBox::WinLayout));
#endif
}

void DialogButtonOrderTest::confirmCancelUsesPlatformOrder() {
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* buttons =
        qtm::PlatformDialogButtonBox::confirmCancel(QStringLiteral("Confirm"), &host);
    layout->addWidget(buttons);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QCoreApplication::processEvents();

    QAbstractButton* confirm = buttonForRole(buttons, QDialogButtonBox::AcceptRole);
    QAbstractButton* cancel = buttons->button(QDialogButtonBox::Cancel);
    QVERIFY(confirm);
    QVERIFY(cancel);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QVERIFY(horizontalCenter(cancel, &host) < horizontalCenter(confirm, &host));
#elif defined(Q_OS_WIN)
    QVERIFY(horizontalCenter(confirm, &host) < horizontalCenter(cancel, &host));
#endif
}

void DialogButtonOrderTest::saveDiscardCancelUsesPlatformOrder() {
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* buttons = new qtm::PlatformDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, &host);
    layout->addWidget(buttons);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QCoreApplication::processEvents();

    QAbstractButton* save = buttons->button(QDialogButtonBox::Save);
    QAbstractButton* discard = buttons->button(QDialogButtonBox::Discard);
    QAbstractButton* cancel = buttons->button(QDialogButtonBox::Cancel);
    QVERIFY(save);
    QVERIFY(discard);
    QVERIFY(cancel);

    const int saveX = horizontalCenter(save, &host);
    const int discardX = horizontalCenter(discard, &host);
    const int cancelX = horizontalCenter(cancel, &host);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QVERIFY(discardX < cancelX);
    QVERIFY(cancelX < saveX);
#elif defined(Q_OS_WIN)
    QVERIFY(saveX < discardX);
    QVERIFY(discardX < cancelX);
#endif
}

QTEST_MAIN(DialogButtonOrderTest)

#include "tst_DialogButtonOrder.moc"
