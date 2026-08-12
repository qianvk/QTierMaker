#pragma once

#include <QWidget>

class QLabel;

namespace qtm {

class TransferProgressTrack;

/** Compact themed progress with determinate and low-overhead indeterminate presentation. */
class TransferProgressWidget final : public QWidget {
public:
    explicit TransferProgressWidget(QWidget* parent = nullptr);

    void setStatusText(const QString& text);
    void setProgress(qint64 received, qint64 total);
    void setIndeterminate();
    void reset();

    [[nodiscard]] bool isIndeterminate() const;
    [[nodiscard]] int percentage() const;

private:
    QLabel* m_status{nullptr};
    TransferProgressTrack* m_track{nullptr};
};

} // namespace qtm
