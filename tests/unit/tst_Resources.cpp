#include <QtTest>

#include <vkui/core/VkIcon.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTranslator>

class tst_Resources : public QObject {
    Q_OBJECT

private slots:
    void iconsComeFromVkUi() {
        const QList symbols = {vkui::VkSymbol::Plus,     vkui::VkSymbol::Folder,
                               vkui::VkSymbol::Document, vkui::VkSymbol::Upload,
                               vkui::VkSymbol::Download, vkui::VkSymbol::Edit,
                               vkui::VkSymbol::Settings};
        for (const vkui::VkSymbol symbol : symbols) {
            const QIcon icon = vkui::icon(symbol);
            QVERIFY(!icon.isNull());
            QVERIFY(!icon.pixmap(QSize(24, 24)).isNull());
        }
    }

    void bundledChineseTranslatorCoversStandardDialogButtons() {
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(QTM_TEST_TRANSLATION_FILE)));
        qApp->installTranslator(&translator);

        QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Discard |
                                 QDialogButtonBox::Cancel);
        QCOMPARE(buttons.button(QDialogButtonBox::Save)->text(), QStringLiteral("保存"));
        QCOMPARE(buttons.button(QDialogButtonBox::Discard)->text(), QStringLiteral("不保存"));
        QCOMPARE(buttons.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("取消"));

        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(tst_Resources)
#include "tst_Resources.moc"
