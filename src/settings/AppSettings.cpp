#include "settings/AppSettings.h"

#include "settings/SettingsKeys.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace qtm {

namespace {
QString appearanceToString(AppearanceMode mode) {
    switch (mode) {
    case AppearanceMode::Light:
        return QStringLiteral("light");
    case AppearanceMode::Dark:
        return QStringLiteral("dark");
    case AppearanceMode::System:
    default:
        return QStringLiteral("system");
    }
}

AppearanceMode appearanceFromString(const QString& value) {
    if (value == QStringLiteral("light")) {
        return AppearanceMode::Light;
    }
    if (value == QStringLiteral("dark")) {
        return AppearanceMode::Dark;
    }
    return AppearanceMode::System;
}

QString blankAreaActionToString(BlankAreaAction action) {
    switch (action) {
    case BlankAreaAction::TierMissionControl:
        return QStringLiteral("tierMissionControl");
    case BlankAreaAction::GalleryMissionControl:
        return QStringLiteral("galleryMissionControl");
    case BlankAreaAction::None:
    default:
        return QStringLiteral("none");
    }
}

BlankAreaAction blankAreaActionFromString(const QString& value, BlankAreaAction fallback) {
    if (value == QStringLiteral("tierMissionControl")) {
        return BlankAreaAction::TierMissionControl;
    }
    if (value == QStringLiteral("galleryMissionControl")) {
        return BlankAreaAction::GalleryMissionControl;
    }
    if (value == QStringLiteral("none")) {
        return BlankAreaAction::None;
    }
    return fallback;
}

QString previewBackgroundModeToString(PreviewBackgroundMode mode) {
    return mode == PreviewBackgroundMode::SelfImage ? QStringLiteral("selfImage")
                                                    : QStringLiteral("none");
}

PreviewBackgroundMode previewBackgroundModeFromString(const QString& value) {
    return value == QStringLiteral("selfImage") ? PreviewBackgroundMode::SelfImage
                                                : PreviewBackgroundMode::None;
}

QString backdropEffectToString(BackdropEffect effect) {
    return effect == BackdropEffect::LiquidGlass ? QStringLiteral("liquidGlass")
                                                  : QStringLiteral("depthSoftFocus");
}

BackdropEffect backdropEffectFromString(const QString& value) {
    return value == QStringLiteral("liquidGlass") ? BackdropEffect::LiquidGlass
                                                   : BackdropEffect::DepthSoftFocus;
}

qreal realSetting(const QSettings& settings, QStringView key, qreal fallback) {
    bool converted = false;
    const qreal value = settings.value(key.toString(), fallback).toDouble(&converted);
    return converted ? value : fallback;
}

LiquidGlassParameters readLiquidGlassParameters(const QSettings& settings) {
    LiquidGlassParameters parameters;
    parameters.cornerRadius =
        realSetting(settings, settings_keys::liquidGlassCornerRadius, parameters.cornerRadius);
    parameters.blurRadius =
        realSetting(settings, settings_keys::liquidGlassBlurRadius, parameters.blurRadius);
    parameters.refractionHeightFraction = realSetting(
        settings, settings_keys::liquidGlassRefractionHeight,
        parameters.refractionHeightFraction);
    parameters.refractionAmountFraction = realSetting(
        settings, settings_keys::liquidGlassRefractionAmount,
        parameters.refractionAmountFraction);
    parameters.chromaticAberration = realSetting(
        settings, settings_keys::liquidGlassChromaticAberration,
        parameters.chromaticAberration);
    return normalizedLiquidGlassParameters(parameters);
}

void writeLiquidGlassParameters(QSettings& settings,
                                const LiquidGlassParameters& parameters) {
    settings.setValue(settings_keys::liquidGlassCornerRadius.toString(),
                      parameters.cornerRadius);
    settings.setValue(settings_keys::liquidGlassBlurRadius.toString(), parameters.blurRadius);
    settings.setValue(settings_keys::liquidGlassRefractionHeight.toString(),
                      parameters.refractionHeightFraction);
    settings.setValue(settings_keys::liquidGlassRefractionAmount.toString(),
                      parameters.refractionAmountFraction);
    settings.setValue(settings_keys::liquidGlassChromaticAberration.toString(),
                      parameters.chromaticAberration);
}

QString fallbackProjectDirectory() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (path.isEmpty()) {
        path = QDir::homePath();
    }
    return QDir::cleanPath(QDir(path).filePath(QStringLiteral("QTierMaker")));
}

void migrateLegacySettings(QSettings& settings) {
    if (!settings.allKeys().isEmpty()) {
        return;
    }

    // Product renaming must not reset an existing installation. Copy once into the new settings
    // domain, then let QTierMaker own all subsequent writes.
    QSettings legacy(QStringLiteral("TierListMaker"), QStringLiteral("TierListMaker"));
    const QStringList keys = legacy.allKeys();
    for (const QString& key : keys) {
        settings.setValue(key, legacy.value(key));
    }
    if (!keys.isEmpty()) {
        settings.sync();
    }
}
} // namespace

AppSettings::AppSettings(QObject* parent)
    : QObject(parent),
      m_settings(QStringLiteral("QTierMaker"), QStringLiteral("QTierMaker")) {
    migrateLegacySettings(m_settings);
    m_liquidGlassParameters = readLiquidGlassParameters(m_settings);
}

AppSettings::~AppSettings() {
    if (m_liquidGlassParametersDirty) {
        writeLiquidGlassParameters(m_settings, m_liquidGlassParameters);
    }
}

QString AppSettings::language() const {
    return m_settings.value(settings_keys::language.toString(), QStringLiteral("system"))
        .toString();
}

void AppSettings::setLanguage(const QString& language) {
    if (this->language() == language) {
        return;
    }
    m_settings.setValue(settings_keys::language.toString(), language);
    emit languageChanged(language);
    emit changed();
}

AppearanceMode AppSettings::appearance() const {
    return appearanceFromString(
        m_settings.value(settings_keys::appearance.toString(), QStringLiteral("system"))
            .toString());
}

void AppSettings::setAppearance(AppearanceMode mode) {
    if (appearance() == mode) {
        return;
    }
    m_settings.setValue(settings_keys::appearance.toString(), appearanceToString(mode));
    emit appearanceChanged(mode);
    emit changed();
}

bool AppSettings::autosaveEnabled() const {
    return m_settings.value(settings_keys::autosaveEnabled.toString(), true).toBool();
}

void AppSettings::setAutosaveEnabled(bool enabled) {
    m_settings.setValue(settings_keys::autosaveEnabled.toString(), enabled);
    emit changed();
}

int AppSettings::autosaveIntervalMinutes() const {
    return m_settings.value(settings_keys::autosaveIntervalMinutes.toString(), 3).toInt();
}

void AppSettings::setAutosaveIntervalMinutes(int minutes) {
    m_settings.setValue(settings_keys::autosaveIntervalMinutes.toString(), qBound(1, minutes, 60));
    emit changed();
}

QString AppSettings::defaultProjectDirectory() const {
    const QString configured =
        m_settings.value(settings_keys::defaultProjectDirectory.toString()).toString();
    if (!configured.trimmed().isEmpty() && QFileInfo::exists(configured)) {
        return QDir::cleanPath(configured);
    }
    return fallbackProjectDirectory();
}

void AppSettings::setDefaultProjectDirectory(const QString& path) {
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty() || defaultProjectDirectory() == clean) {
        return;
    }
    m_settings.setValue(settings_keys::defaultProjectDirectory.toString(), clean);
    emit changed();
}

QString AppSettings::defaultTemplateId() const {
    return m_settings.value(settings_keys::defaultTemplate.toString()).toString();
}

void AppSettings::setDefaultTemplateId(const QString& id) {
    const QString clean = id.trimmed();
    if (defaultTemplateId() == clean) {
        return;
    }
    if (clean.isEmpty()) {
        m_settings.remove(settings_keys::defaultTemplate.toString());
    } else {
        m_settings.setValue(settings_keys::defaultTemplate.toString(), clean);
    }
    emit changed();
}

bool AppSettings::autoUpdateEnabled() const {
    return m_settings.value(settings_keys::autoUpdateEnabled.toString(), false).toBool();
}

void AppSettings::setAutoUpdateEnabled(bool enabled) {
    if (autoUpdateEnabled() == enabled) {
        return;
    }
    m_settings.setValue(settings_keys::autoUpdateEnabled.toString(), enabled);
    emit autoUpdateEnabledChanged(enabled);
    emit changed();
}

QDateTime AppSettings::lastUpdateCheckAt() const {
    return m_settings.value(settings_keys::lastUpdateCheckAt.toString()).toDateTime().toUTC();
}

QString AppSettings::lastUpdateCheckVersion() const {
    return m_settings.value(settings_keys::lastUpdateCheckVersion.toString()).toString().trimmed();
}

void AppSettings::recordSuccessfulUpdateCheck(const QString& version, const QDateTime& checkedAt) {
    m_settings.setValue(settings_keys::lastUpdateCheckAt.toString(), checkedAt.toUTC());
    m_settings.setValue(settings_keys::lastUpdateCheckVersion.toString(), version.trimmed());
    emit changed();
}

bool AppSettings::shouldRunAutoUpdateCheck(const QString& currentVersion,
                                           const QDateTime& now) const {
    if (!autoUpdateEnabled()) {
        return false;
    }
    if (lastUpdateCheckVersion() != currentVersion.trimmed()) {
        return true;
    }
    const QDateTime lastCheck = lastUpdateCheckAt();
    if (!lastCheck.isValid()) {
        return true;
    }
    constexpr qint64 kAutoUpdateIntervalSecs = 24 * 60 * 60;
    return lastCheck.secsTo(now.toUTC()) >= kAutoUpdateIntervalSecs;
}

BlankAreaAction AppSettings::blankDoubleClickAction() const {
    return blankAreaActionFromString(m_settings
                                         .value(settings_keys::blankDoubleClickAction.toString(),
                                                QStringLiteral("galleryMissionControl"))
                                         .toString(),
                                     BlankAreaAction::GalleryMissionControl);
}

void AppSettings::setBlankDoubleClickAction(BlankAreaAction action) {
    if (blankDoubleClickAction() == action) {
        return;
    }
    m_settings.setValue(settings_keys::blankDoubleClickAction.toString(),
                        blankAreaActionToString(action));
    emit blankDoubleClickActionChanged(action);
    emit changed();
}

BlankAreaAction AppSettings::blankLongPressAction() const {
    return blankAreaActionFromString(m_settings
                                         .value(settings_keys::blankLongPressAction.toString(),
                                                QStringLiteral("tierMissionControl"))
                                         .toString(),
                                     BlankAreaAction::TierMissionControl);
}

void AppSettings::setBlankLongPressAction(BlankAreaAction action) {
    if (blankLongPressAction() == action) {
        return;
    }
    m_settings.setValue(settings_keys::blankLongPressAction.toString(),
                        blankAreaActionToString(action));
    emit blankLongPressActionChanged(action);
    emit changed();
}

bool AppSettings::tierListToolTipsEnabled() const {
    return m_settings.value(settings_keys::tierListToolTipsEnabled.toString(), true).toBool();
}

void AppSettings::setTierListToolTipsEnabled(bool enabled) {
    if (tierListToolTipsEnabled() == enabled) {
        return;
    }
    m_settings.setValue(settings_keys::tierListToolTipsEnabled.toString(), enabled);
    emit tierListToolTipsEnabledChanged(enabled);
    emit changed();
}

BackdropEffect AppSettings::overviewBackdropEffect() const {
    return backdropEffectFromString(
        m_settings
            .value(settings_keys::overviewBackdropEffect.toString(),
                   QStringLiteral("depthSoftFocus"))
            .toString());
}

void AppSettings::setOverviewBackdropEffect(BackdropEffect effect) {
    if (overviewBackdropEffect() == effect) {
        return;
    }
    m_settings.setValue(settings_keys::overviewBackdropEffect.toString(),
                        backdropEffectToString(effect));
    emit overviewBackdropEffectChanged(effect);
    emit changed();
}

PreviewBackgroundMode AppSettings::previewBackgroundMode() const {
    return previewBackgroundModeFromString(
        m_settings.value(settings_keys::previewBackgroundMode.toString(), QStringLiteral("none"))
            .toString());
}

void AppSettings::setPreviewBackgroundMode(PreviewBackgroundMode mode) {
    if (previewBackgroundMode() == mode) {
        return;
    }
    m_settings.setValue(settings_keys::previewBackgroundMode.toString(),
                        previewBackgroundModeToString(mode));
    emit previewBackgroundModeChanged(mode);
    emit changed();
}

BackdropEffect AppSettings::previewEffect() const {
    return backdropEffectFromString(
        m_settings
            .value(settings_keys::previewEffect.toString(), QStringLiteral("depthSoftFocus"))
            .toString());
}

void AppSettings::setPreviewEffect(BackdropEffect effect) {
    if (previewEffect() == effect) {
        return;
    }
    m_settings.setValue(settings_keys::previewEffect.toString(), backdropEffectToString(effect));
    emit previewEffectChanged(effect);
    emit changed();
}

LiquidGlassParameters AppSettings::liquidGlassParameters() const {
    return m_liquidGlassParameters;
}

void AppSettings::previewLiquidGlassParameters(const LiquidGlassParameters& parameters) {
    const LiquidGlassParameters normalized = normalizedLiquidGlassParameters(parameters);
    if (m_liquidGlassParameters == normalized) {
        return;
    }

    m_liquidGlassParameters = normalized;
    m_liquidGlassParametersDirty = true;
    emit liquidGlassParametersChanged();
}

void AppSettings::setLiquidGlassParameters(const LiquidGlassParameters& parameters) {
    const LiquidGlassParameters normalized = normalizedLiquidGlassParameters(parameters);
    const bool valueChanged = m_liquidGlassParameters != normalized;
    const bool persistenceChanged = readLiquidGlassParameters(m_settings) != normalized;
    if (!valueChanged && !persistenceChanged && !m_liquidGlassParametersDirty) {
        return;
    }

    m_liquidGlassParameters = normalized;
    m_liquidGlassParametersDirty = false;
    if (persistenceChanged) {
        writeLiquidGlassParameters(m_settings, normalized);
    }
    if (valueChanged) {
        emit liquidGlassParametersChanged();
    }
    if (persistenceChanged) {
        emit changed();
    }
}

QString AppSettings::defaultExportFormat() const {
    return m_settings.value(settings_keys::exportFormat.toString(), QStringLiteral("png"))
        .toString();
}

void AppSettings::setDefaultExportFormat(const QString& format) {
    m_settings.setValue(settings_keys::exportFormat.toString(), format);
    emit changed();
}

int AppSettings::defaultExportScale() const {
    return m_settings.value(settings_keys::exportScale.toString(), 2).toInt();
}

void AppSettings::setDefaultExportScale(int scale) {
    m_settings.setValue(settings_keys::exportScale.toString(), qBound(1, scale, 4));
    emit changed();
}

bool AppSettings::animationsEnabled() const {
    return m_settings.value(settings_keys::animationsEnabled.toString(), true).toBool();
}

void AppSettings::setAnimationsEnabled(bool enabled) {
    m_settings.setValue(settings_keys::animationsEnabled.toString(), enabled);
    emit changed();
}

bool AppSettings::reducedMotion() const {
    return m_settings.value(settings_keys::reducedMotion.toString(), false).toBool();
}

void AppSettings::setReducedMotion(bool enabled) {
    m_settings.setValue(settings_keys::reducedMotion.toString(), enabled);
    emit changed();
}

bool AppSettings::localOnlyMode() const {
    return m_settings.value(settings_keys::localOnlyMode.toString(), true).toBool();
}

void AppSettings::setLocalOnlyMode(bool enabled) {
    m_settings.setValue(settings_keys::localOnlyMode.toString(), enabled);
    emit changed();
}

} // namespace qtm
