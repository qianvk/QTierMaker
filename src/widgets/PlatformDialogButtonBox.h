#pragma once

#include <QDialogButtonBox>

namespace qtm {

/** Dialog actions whose visual order is resolved by the active platform style. */
class PlatformDialogButtonBox final : public QDialogButtonBox {
public:
    explicit PlatformDialogButtonBox(StandardButtons buttons, QWidget* parent = nullptr);

    /** Creates a semantic Accept/Reject pair without encoding a visual order. */
    static PlatformDialogButtonBox* confirmCancel(const QString& confirmText, QWidget* parent);
};

} // namespace qtm
