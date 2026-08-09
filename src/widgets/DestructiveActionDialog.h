#pragma once

#include <QString>

class QWidget;

namespace qtm {

bool confirmDestructiveAction(QWidget* parent, const QString& title, const QString& text,
                              const QString& confirmText = {});

} // namespace qtm
