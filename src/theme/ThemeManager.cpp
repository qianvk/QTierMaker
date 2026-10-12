#include "theme/ThemeManager.h"

#include <vkui/core/VkAppearance.h>
#include <vkui/core/VkThemeManager.h>
#include <QGuiApplication>
#include <QStyleHints>

namespace qtm {

ThemeManager::ThemeManager(AppSettings* settings, QObject* parent)
    : QObject(parent), m_settings(settings) {
    connect(m_settings, &AppSettings::appearanceChanged, this,
            [this](AppearanceMode) { apply(); });
}

void ThemeManager::apply() {
    vkui::VkAppearance appearance = vkui::VkAppearance::Auto;
    if (m_settings) {
        switch (m_settings->appearance()) {
        case AppearanceMode::Light:
            appearance = vkui::VkAppearance::Light;
            break;
        case AppearanceMode::Dark:
            appearance = vkui::VkAppearance::Dark;
            break;
        case AppearanceMode::System:
            appearance = vkui::VkAppearance::Auto;
            break;
        }
    }
#if defined(Q_OS_WIN)
    // Qt's native caption renderer uses colorScheme rather than the widget palette.
    // Keep its button colors consistent with the user's explicit app appearance.
    if (auto* hints = QGuiApplication::styleHints()) {
        switch (appearance) {
        case vkui::VkAppearance::Light:
            hints->setColorScheme(Qt::ColorScheme::Light);
            break;
        case vkui::VkAppearance::Dark:
            hints->setColorScheme(Qt::ColorScheme::Dark);
            break;
        case vkui::VkAppearance::Auto:
            hints->unsetColorScheme();
            break;
        }
    }
#endif
    vkui::VkThemeManager::instance()->setAppearance(appearance);
}

} // namespace qtm
