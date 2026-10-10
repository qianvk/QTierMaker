#pragma once

#include "settings/AppSettings.h"
#include <QObject>

namespace qtm {

/** Maps the persisted appearance preference onto VkUI process-wide theme manager. */
class ThemeManager : public QObject {
    Q_OBJECT

public:
    explicit ThemeManager(AppSettings* settings, QObject* parent = nullptr);

    void apply();

private:
    AppSettings* m_settings{nullptr};
};

} // namespace qtm
