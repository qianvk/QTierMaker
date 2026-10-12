#include "settings/AppSettings.h"
#include "theme/Theme.h"
#include "theme/ThemeManager.h"
#include "window/AppTitleBar.h"

#include <QApplication>
#include <QStandardPaths>
#include <QStyleHints>
#include <QLineEdit>
#include <QtTest>
#include <vkui/core/VkAppearance.h>
#include <vkui/core/VkThemeManager.h>

using namespace qtm;

class tst_ThemeManager : public QObject {
    Q_OBJECT

private slots:
    void titleTextFollowsLiveAppearanceChanges() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const auto original = settings.appearance();
        ThemeManager bridge(&settings);
        settings.setAppearance(AppearanceMode::Light);
        bridge.apply();
        QWidget surface;
        AppTitleBar title(&surface);
        title.setDocumentTitle(QStringLiteral("Readable title"));
        title.setTitleEditable(false);
        surface.resize(900, 100);
        title.resize(900, 54);
        surface.show();
        QVERIFY(QTest::qWaitForWindowExposed(&surface));
        settings.setAppearance(AppearanceMode::Dark);
        QTRY_VERIFY(title.titleEditor()->palette().color(QPalette::Text).lightness() > 128);
        settings.setAppearance(AppearanceMode::Light);
        QTRY_VERIFY(title.titleEditor()->palette().color(QPalette::Text).lightness() < 128);
        settings.setAppearance(original);
    }
    void windowsCaptionColorsFollowAppearance() {
#if defined(Q_OS_WIN)
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const auto original = settings.appearance();
        ThemeManager bridge(&settings);
        settings.setAppearance(AppearanceMode::Dark);
        bridge.apply();
        QTRY_COMPARE(qApp->styleHints()->colorScheme(), Qt::ColorScheme::Dark);
        settings.setAppearance(AppearanceMode::Light);
        QTRY_COMPARE(qApp->styleHints()->colorScheme(), Qt::ColorScheme::Light);
        settings.setAppearance(AppearanceMode::System);
        bridge.apply();
        QVERIFY(vkui::VkThemeManager::instance()->appearance() == vkui::VkAppearance::Auto);
        settings.setAppearance(original);
#else
        QSKIP("Qt's Windows caption renderer uses the application color scheme.");
#endif
    }
    void lightAndDarkHaveDistinctTokens() {
        auto* manager = vkui::VkThemeManager::instance();
        const vkui::VkAppearance original = manager->appearance();
        manager->setAppearance(vkui::VkAppearance::Light);
        const Theme light(manager->theme());
        const QColor lightAccent = manager->theme().colors().accent;
        QCOMPARE(activeThemeTokens().contentBackground,
                 manager->theme().colors().contentBackground);
        QCOMPARE(qApp->palette().color(QPalette::Base),
                 manager->theme().colors().contentBackground);
        QCOMPARE(light.tokens().popoverBackground, manager->theme().colors().popoverBackground);
        manager->setAppearance(vkui::VkAppearance::Dark);
        const Theme dark(manager->theme());
        QVERIFY(light.tokens().contentBackground != dark.tokens().contentBackground);
        QCOMPARE(light.tokens().accent, lightAccent);
        QCOMPARE(activeThemeTokens().tierRowBackground,
                 manager->theme().colors().elevatedBackground);
        QCOMPARE(dark.tokens().separator, manager->theme().colors().separator);
        QCOMPARE(dark.tokens().imageBorder, manager->theme().colors().border);
        QCOMPARE(qApp->palette().color(QPalette::Window),
                 manager->theme().colors().windowBackground);
        QVERIFY(dark.isDark());
        QVERIFY(!light.isDark());
        QVERIFY(dark.tokens().primaryText.lightness() >
                dark.tokens().contentBackground.lightness());
        QVERIFY(light.tokens().primaryText.lightness() <
                light.tokens().contentBackground.lightness());
        manager->setAppearance(original);
    }

    void settingsDriveTheVkUiTheme() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        ThemeManager bridge(&settings);

        settings.setAppearance(AppearanceMode::Dark);
        bridge.apply();
        QCOMPARE(vkui::VkThemeManager::instance()->effectiveAppearance(), vkui::VkAppearance::Dark);
        QCOMPARE(activeThemeTokens().contentBackground,
                 vkui::VkThemeManager::instance()->theme().colors().contentBackground);

        settings.setAppearance(AppearanceMode::Light);
        QCOMPARE(vkui::VkThemeManager::instance()->effectiveAppearance(),
                 vkui::VkAppearance::Light);
        QCOMPARE(qApp->palette().color(QPalette::Base), activeThemeTokens().contentBackground);
        settings.setAppearance(AppearanceMode::System);
    }

    void tierListToolTipsSettingPersistsAndSignals() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const bool original = settings.tierListToolTipsEnabled();
        QSignalSpy changed(&settings, &AppSettings::tierListToolTipsEnabledChanged);

        settings.setTierListToolTipsEnabled(!original);
        QCOMPARE(settings.tierListToolTipsEnabled(), !original);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().constFirst().toBool(), !original);

        settings.setTierListToolTipsEnabled(original);
    }

    void overviewBackdropSettingPersistsAndSignals() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const BackdropEffect original = settings.overviewBackdropEffect();
        const BackdropEffect changedEffect = original == BackdropEffect::DepthSoftFocus
                                                 ? BackdropEffect::LiquidGlass
                                                 : BackdropEffect::DepthSoftFocus;
        QSignalSpy changed(&settings, &AppSettings::overviewBackdropEffectChanged);

        settings.setOverviewBackdropEffect(changedEffect);
        QCOMPARE(settings.overviewBackdropEffect(), changedEffect);
        QCOMPARE(changed.count(), 1);

        settings.setOverviewBackdropEffect(original);
    }

    void previewEffectSettingPersistsAndSignals() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const BackdropEffect original = settings.previewEffect();
        const BackdropEffect changedEffect = original == BackdropEffect::DepthSoftFocus
                                                 ? BackdropEffect::LiquidGlass
                                                 : BackdropEffect::DepthSoftFocus;
        QSignalSpy changed(&settings, &AppSettings::previewEffectChanged);

        settings.setPreviewEffect(changedEffect);
        QCOMPARE(settings.previewEffect(), changedEffect);
        QCOMPARE(changed.count(), 1);

        settings.setPreviewEffect(original);
    }

    void liquidGlassParametersPersistAndSignal() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const LiquidGlassParameters original = settings.liquidGlassParameters();
        LiquidGlassParameters modified = original;
        modified.cornerRadius = 12.0;
        modified.blurRadius = 12.5;
        modified.refractionHeightFraction = 0.35;
        modified.refractionAmountFraction = 0.45;
        modified.chromaticAberration = 1.0;
        QSignalSpy changed(&settings, &AppSettings::liquidGlassParametersChanged);

        settings.setLiquidGlassParameters(modified);
        QVERIFY(settings.liquidGlassParameters() == modified);
        QCOMPARE(changed.count(), 1);

        settings.setLiquidGlassParameters(modified);
        QCOMPARE(changed.count(), 1);
        settings.setLiquidGlassParameters(original);
    }

    void liquidGlassPreviewDefersPersistenceUntilCommit() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const LiquidGlassParameters original = settings.liquidGlassParameters();
        LiquidGlassParameters modified = original;
        modified.cornerRadius = original.cornerRadius < 127.0 ? original.cornerRadius + 1.0
                                                             : original.cornerRadius - 1.0;
        QSignalSpy parameterChanged(&settings, &AppSettings::liquidGlassParametersChanged);
        QSignalSpy settingsChanged(&settings, &AppSettings::changed);

        settings.previewLiquidGlassParameters(modified);
        QVERIFY(settings.liquidGlassParameters() == modified);
        QCOMPARE(parameterChanged.count(), 1);
        QCOMPARE(settingsChanged.count(), 0);

        AppSettings persistedBeforeCommit;
        QVERIFY(persistedBeforeCommit.liquidGlassParameters() == original);

        settings.setLiquidGlassParameters(modified);
        QCOMPARE(parameterChanged.count(), 1);
        QCOMPARE(settingsChanged.count(), 1);
        AppSettings persistedAfterCommit;
        QVERIFY(persistedAfterCommit.liquidGlassParameters() == modified);
        settings.setLiquidGlassParameters(original);
    }

    void updateChecksAreScheduledPerApplicationVersion() {
        QStandardPaths::setTestModeEnabled(true);
        AppSettings settings;
        const bool original = settings.autoUpdateEnabled();
        const QDateTime now = QDateTime::currentDateTimeUtc();

        settings.setAutoUpdateEnabled(true);
        settings.recordSuccessfulUpdateCheck(QStringLiteral("0.2.0-beta.4"), now);
        QVERIFY(!settings.shouldRunAutoUpdateCheck(QStringLiteral("0.2.0-beta.4"),
                                                   now.addSecs(60 * 60)));
        QVERIFY(settings.shouldRunAutoUpdateCheck(QStringLiteral("0.2.0-beta.5"),
                                                  now.addSecs(60 * 60)));
        QVERIFY(settings.shouldRunAutoUpdateCheck(QStringLiteral("0.2.0-beta.4"),
                                                  now.addSecs(25 * 60 * 60)));

        settings.setAutoUpdateEnabled(false);
        QVERIFY(!settings.shouldRunAutoUpdateCheck(QStringLiteral("0.2.0-beta.5"), now.addDays(2)));
        settings.setAutoUpdateEnabled(original);
    }
};

QTEST_MAIN(tst_ThemeManager)
#include "tst_ThemeManager.moc"
