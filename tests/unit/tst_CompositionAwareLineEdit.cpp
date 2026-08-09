#include "window/CompositionAwareLineEdit.h"

#include <QInputMethodEvent>
#include <QSignalSpy>
#include <QtTest>

using namespace qtm;

class CompositionAwareLineEditTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesPreeditTextBeforeCommit();
    void preeditReplacesSelectedTextForSizing();
};

void CompositionAwareLineEditTest::exposesPreeditTextBeforeCommit() {
    CompositionAwareLineEdit edit;
    edit.resize(240, 36);
    edit.show();
    edit.setFocus(Qt::OtherFocusReason);
    QSignalSpy visualTextSpy(&edit, &CompositionAwareLineEdit::visualTextChanged);

    QInputMethodEvent preedit(QStringLiteral("中文标题"), {});
    QApplication::sendEvent(&edit, &preedit);
    QCOMPARE(edit.text(), QString());
    QCOMPARE(edit.visualText(), QStringLiteral("中文标题"));
    QCOMPARE(visualTextSpy.count(), 1);

    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("中文标题"));
    QApplication::sendEvent(&edit, &commit);
    QCOMPARE(edit.text(), QStringLiteral("中文标题"));
    QCOMPARE(edit.visualText(), edit.text());
    QCOMPARE(visualTextSpy.count(), 2);
}

void CompositionAwareLineEditTest::preeditReplacesSelectedTextForSizing() {
    CompositionAwareLineEdit edit;
    edit.setText(QStringLiteral("Old title"));
    edit.selectAll();

    QInputMethodEvent preedit(QStringLiteral("新标题"), {});
    QApplication::sendEvent(&edit, &preedit);

    QCOMPARE(edit.visualText(), QStringLiteral("新标题"));
}

QTEST_MAIN(CompositionAwareLineEditTest)

#include "tst_CompositionAwareLineEdit.moc"
