#include "update/UpdateButton.h"
#include "widgets/TransferProgressWidget.h"

#include <QHBoxLayout>
#include <QtTest>

using namespace qtm;

class UpdateButtonTest final : public QObject {
    Q_OBJECT

private slots:
    void reflectsUpdateLifecycle();
};

void UpdateButtonTest::reflectsUpdateLifecycle() {
    QWidget host;
    auto* layout = new QHBoxLayout(&host);
    auto* button = new UpdateButton(&host);
    button->setInstallText(QStringLiteral("Install"));
    layout->addWidget(button);

    button->setUpdateState(UpdateState::Available);
    QVERIFY(!button->isHidden());
    QVERIFY(button->isEnabled());
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonIconOnly);
    QCOMPARE(button->width(), 34);
    QVERIFY(!button->icon().isNull());

    button->setUpdateState(UpdateState::Downloading);
    auto* progress = dynamic_cast<TransferProgressIndicator*>(button->findChild<QWidget*>(
        QStringLiteral("UpdateTransferProgressIndicator")));
    QVERIFY(progress);
    QVERIFY(!progress->isHidden());
    QVERIFY(!button->isEnabled());
    QVERIFY(button->width() > 34);

    button->setDownloadProgress(0, -1);
    QVERIFY(progress->isIndeterminate());
    button->setDownloadProgress(25, 100);
    QVERIFY(!progress->isIndeterminate());
    QCOMPARE(progress->percentage(), 25);

    button->setUpdateState(UpdateState::Ready);
    QVERIFY(progress->isHidden());
    QVERIFY(button->isEnabled());
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    QCOMPARE(button->text(), QStringLiteral("Install"));
    QVERIFY(!button->icon().isNull());

    button->setUpdateState(UpdateState::Installing);
    QVERIFY(!button->isHidden());
    QVERIFY(!button->isEnabled());
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    QCOMPARE(button->text(), QStringLiteral("Install"));

    button->setUpdateState(UpdateState::Idle);
    QVERIFY(button->isHidden());
}

QTEST_MAIN(UpdateButtonTest)

#include "tst_UpdateButton.moc"
