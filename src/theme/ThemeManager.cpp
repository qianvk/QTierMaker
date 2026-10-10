#include "theme/ThemeManager.h"

#include <vkui/core/VkAppearance.h>
#include <vkui/core/VkThemeManager.h>

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
    vkui::VkThemeManager::instance()->setAppearance(appearance);
}

} // namespace qtm
