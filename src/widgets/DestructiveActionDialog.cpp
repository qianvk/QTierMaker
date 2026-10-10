#include "widgets/DestructiveActionDialog.h"

#include "window/AppMessageDialog.h"

#include <QCoreApplication>

namespace qtm {

bool confirmDestructiveAction(QWidget* parent, const QString& title, const QString& text,
                              const QString& confirmText) {
    const QString actionText =
        confirmText.isEmpty()
            ? QCoreApplication::translate("DestructiveActionDialog", "Confirm")
            : confirmText;
    return AppMessageDialog::confirmDestructive(parent, title, text, actionText);
}

} // namespace qtm
