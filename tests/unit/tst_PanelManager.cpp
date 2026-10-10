#include <QSplitterHandle>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest>

#include <vkui/widgets/controls/VSplitter.h>
#include <vkui/widgets/panels/VPanelManager.h>

class PanelManagerIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void publicManagerOwnsCollapseAndRestore();
};

void PanelManagerIntegrationTest::publicManagerOwnsCollapseAndRestore() {
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new vkui::VSplitter(Qt::Horizontal, &window);
    splitter->setChildrenCollapsible(true);
    auto* navigation = new QWidget(splitter);
    navigation->setMinimumWidth(208);
    auto* content = new QWidget(splitter);
    content->setMinimumWidth(620);
    splitter->addWidget(navigation);
    splitter->addWidget(content);
    splitter->setSizes({240, 660});
    layout->addWidget(splitter);

    vkui::VPanelManager manager(window);
    QVERIFY(manager.setLayoutRoot(splitter));
    QVERIFY(manager.registerPanel(QStringLiteral("navigation"), QStringLiteral("Navigation"),
                                  navigation, 1));
    QVERIFY(manager.registerPanel(QStringLiteral("content"), QStringLiteral("Content"), content, 2));

    window.resize(900, 560);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QSplitterHandle* handle = splitter->handle(1);
    QVERIFY(handle != nullptr);
    QTest::mouseClick(handle, Qt::LeftButton, Qt::NoModifier, handle->rect().center());
    QTRY_VERIFY(!manager.isPanelExpanded(QStringLiteral("navigation")));
    QTRY_COMPARE(splitter->sizes().constFirst(), 0);

    const QList<QWidget*> edgeHandles = manager.windowEdgeHandles();
    QCOMPARE(edgeHandles.size(), 1);
    QWidget* edgeHandle = edgeHandles.constFirst();
    QVERIFY(edgeHandle != nullptr);
    QTest::mouseClick(edgeHandle, Qt::LeftButton, Qt::NoModifier,
                      QPoint(edgeHandle->width() / 2, edgeHandle->height() / 2));
    QTRY_VERIFY(manager.isPanelExpanded(QStringLiteral("navigation")));
    QTRY_VERIFY(splitter->sizes().constFirst() >= navigation->minimumWidth());
}

QTEST_MAIN(PanelManagerIntegrationTest)

#include "tst_PanelManager.moc"
