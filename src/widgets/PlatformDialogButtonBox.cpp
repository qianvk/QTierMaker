#include "widgets/PlatformDialogButtonBox.h"

namespace qtm {

PlatformDialogButtonBox::PlatformDialogButtonBox(const StandardButtons buttons, QWidget* parent)
    : QDialogButtonBox(buttons, Qt::Horizontal, parent) {
    setObjectName(QStringLiteral("PlatformDialogButtonBox"));
    setCenterButtons(false);
}

PlatformDialogButtonBox* PlatformDialogButtonBox::confirmCancel(const QString& confirmText,
                                                                 QWidget* parent) {
    auto* buttons = new PlatformDialogButtonBox(QDialogButtonBox::Cancel, parent);
    buttons->addButton(confirmText, QDialogButtonBox::AcceptRole);
    return buttons;
}

} // namespace qtm
