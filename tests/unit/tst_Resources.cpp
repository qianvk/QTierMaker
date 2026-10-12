#include <QtTest>

#include "i18n/LanguageManager.h"
#include "window/AppMessageDialog.h"
#include <vkui/Widgets.h>
#include <vkui/core/VkIcon.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTranslator>

class tst_Resources : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        vkui::installVkUi(*qApp);
    }

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

    void confirmationCancelFollowsRuntimeLanguageSwitch() {
        qtm::LanguageManager languageManager(qApp);
        QVERIFY(languageManager.setLanguage(QStringLiteral("en")));
        qtm::AppMessageDialog existing(qtm::AppMessageDialog::Icon::Warning,
                                       QStringLiteral("Confirm"), QStringLiteral("Continue?"),
                                       QDialogButtonBox::Cancel);
        QCOMPARE(existing.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("Cancel"));
        QDialogButtonBox regular(QDialogButtonBox::Cancel);
        auto* customReject = regular.addButton(QStringLiteral("Keep editing"),
                                               QDialogButtonBox::RejectRole);

        QVERIFY(languageManager.setLanguage(QStringLiteral("zh_CN")));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);

        qtm::AppMessageDialog created(qtm::AppMessageDialog::Icon::Warning,
                                      QStringLiteral("Confirm"), QStringLiteral("Continue?"),
                                      QDialogButtonBox::Cancel);
        QCOMPARE(created.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("取消"));
        QCOMPARE(existing.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("取消"));
        QCOMPARE(regular.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("取消"));
        QCOMPARE(customReject->text(), QStringLiteral("Keep editing"));

        // A platform or style can leave a standard button carrying its previous label. Opening
        // the actual confirmation must reconcile that label with the selected application language.
        created.button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancel"));
        created.setDefaultButton(QDialogButtonBox::Cancel);
        created.setEscapeButton(QDialogButtonBox::Cancel);
        created.show();
        QVERIFY(QTest::qWaitForWindowExposed(&created));
        QTRY_COMPARE(created.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("取消"));
        QCOMPARE(created.standardButton(created.button(QDialogButtonBox::Cancel)),
                 QDialogButtonBox::Cancel);
        QCOMPARE(created.buttonRole(created.button(QDialogButtonBox::Cancel)),
                 QDialogButtonBox::RejectRole);
        QCOMPARE(created.defaultButton(), created.button(QDialogButtonBox::Cancel));
        QCOMPARE(created.escapeButton(), created.button(QDialogButtonBox::Cancel));

        QVERIFY(languageManager.setLanguage(QStringLiteral("en")));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
        QCOMPARE(existing.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("Cancel"));
        QCOMPARE(regular.button(QDialogButtonBox::Cancel)->text(), QStringLiteral("Cancel"));
    }
};

QTEST_MAIN(tst_Resources)
#include "tst_Resources.moc"
