#pragma once

#include "assets/BackdropEffect.h"
#include "assets/LiquidGlassBackdrop.h"

#include <QDateTime>
#include <QObject>
#include <QSettings>
#include <QString>

namespace tlm {

enum class AppearanceMode { System, Light, Dark };
enum class BlankAreaAction { None, TierMissionControl, GalleryMissionControl };
enum class PreviewBackgroundMode { None, SelfImage };

/** Persistent application preferences backed by QSettings. */
class AppSettings : public QObject {
    Q_OBJECT

public:
    explicit AppSettings(QObject* parent = nullptr);
    ~AppSettings() override;

    QString language() const;
    void setLanguage(const QString& language);

    AppearanceMode appearance() const;
    void setAppearance(AppearanceMode mode);

    bool autosaveEnabled() const;
    void setAutosaveEnabled(bool enabled);
    int autosaveIntervalMinutes() const;
    void setAutosaveIntervalMinutes(int minutes);
    QString defaultProjectDirectory() const;
    void setDefaultProjectDirectory(const QString& path);
    QString defaultTemplateId() const;
    void setDefaultTemplateId(const QString& id);
    bool hasSeededSampleProject(const QString& id) const;
    void recordSeededSampleProject(const QString& id);

    bool autoUpdateEnabled() const;
    void setAutoUpdateEnabled(bool enabled);
    QDateTime lastUpdateCheckAt() const;
    QString lastUpdateCheckVersion() const;
    void recordSuccessfulUpdateCheck(const QString& version, const QDateTime& checkedAt);
    bool shouldRunAutoUpdateCheck(const QString& currentVersion,
                                  const QDateTime& now = QDateTime::currentDateTimeUtc()) const;

    BlankAreaAction blankDoubleClickAction() const;
    void setBlankDoubleClickAction(BlankAreaAction action);
    BlankAreaAction blankLongPressAction() const;
    void setBlankLongPressAction(BlankAreaAction action);
    bool tierListToolTipsEnabled() const;
    void setTierListToolTipsEnabled(bool enabled);
    BackdropEffect overviewBackdropEffect() const;
    void setOverviewBackdropEffect(BackdropEffect effect);
    PreviewBackgroundMode previewBackgroundMode() const;
    void setPreviewBackgroundMode(PreviewBackgroundMode mode);
    BackdropEffect previewEffect() const;
    void setPreviewEffect(BackdropEffect effect);
    LiquidGlassParameters liquidGlassParameters() const;
    void previewLiquidGlassParameters(const LiquidGlassParameters& parameters);
    void setLiquidGlassParameters(const LiquidGlassParameters& parameters);

    QString defaultExportFormat() const;
    void setDefaultExportFormat(const QString& format);
    int defaultExportScale() const;
    void setDefaultExportScale(int scale);

    bool animationsEnabled() const;
    void setAnimationsEnabled(bool enabled);
    bool reducedMotion() const;
    void setReducedMotion(bool enabled);

    bool localOnlyMode() const;
    void setLocalOnlyMode(bool enabled);

signals:
    void changed();
    void languageChanged(const QString& language);
    void appearanceChanged(AppearanceMode mode);
    void autoUpdateEnabledChanged(bool enabled);
    void blankDoubleClickActionChanged(BlankAreaAction action);
    void blankLongPressActionChanged(BlankAreaAction action);
    void tierListToolTipsEnabledChanged(bool enabled);
    void overviewBackdropEffectChanged(BackdropEffect effect);
    void previewBackgroundModeChanged(PreviewBackgroundMode mode);
    void previewEffectChanged(BackdropEffect effect);
    void liquidGlassParametersChanged();

private:
    QSettings m_settings;
    LiquidGlassParameters m_liquidGlassParameters;
    bool m_liquidGlassParametersDirty{false};
};

} // namespace tlm
