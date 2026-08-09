#pragma once

#include <vkui/window/VkFramelessDialog.h>

namespace qtm {

/**
 * Application edit/input dialog.
 *
 * Explicit action buttons own dismissal, so application dialogs lead with their title and do not
 * reserve platform caption-button space. Long-lived utility windows such as Preferences opt back
 * into platform chrome at their call site.
 */
class AppDialog : public vkui::VkFramelessDialog {
public:
    explicit AppDialog(const QString& title, QWidget* parent = nullptr)
        : vkui::VkFramelessDialog(title, parent) {
        setCloseButtonPlacement(CloseButtonPlacement::Hidden);
    }
};

} // namespace qtm
