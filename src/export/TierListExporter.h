#pragma once

#include "export/ExportOptions.h"
#include "persistence/Result.h"

#include <QImage>
#include <QObject>

namespace qtm {

class TierBoardWidget;

/** Captures the live TierBoard rendering instead of maintaining a second export layout. */
class TierListExporter : public QObject {
    Q_OBJECT

public:
    explicit TierListExporter(QObject* parent = nullptr);

    QImage renderBoard(TierBoardWidget* board, const ExportOptions& options) const;
    Result<void> exportBoard(TierBoardWidget* board, const QString& filePath,
                             const ExportOptions& options) const;
};

} // namespace qtm
