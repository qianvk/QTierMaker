#include "window/AppDialog.h"

#include <QLayout>
#include <QShowEvent>

namespace qtm {

AppDialog::AppDialog(const QString& title, QWidget* parent)
    : vkui::VkFramelessDialog(title, parent) {
    setCloseButtonPlacement(CloseButtonPlacement::Hidden);
}

void AppDialog::showEvent(QShowEvent* event) {
    const bool explicitlySized = testAttribute(Qt::WA_Resized);
    if (layout()) {
        if (isResizable()) {
            layout()->setSizeConstraints(QLayout::SetDefaultConstraint,
                                         QLayout::SetDefaultConstraint);
        } else {
            // Let compact dialogs follow dynamic rows vertically while preserving their current
            // width. Purpose-sized dialogs may grow when required, but are never collapsed below
            // the geometry chosen by their caller.
            layout()->setSizeConstraints(QLayout::SetMinimumSize,
                                         explicitlySized ? QLayout::SetMinimumSize
                                                         : QLayout::SetFixedSize);
        }
        layout()->activate();
    }
    vkui::VkFramelessDialog::showEvent(event);
}

} // namespace qtm
