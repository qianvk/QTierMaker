#pragma once

#include <QWidget>

class QLabel;
class QHideEvent;
class QShowEvent;
class QTimeLine;
class QVariantAnimation;

namespace qtm {

/** Shared transfer progress track used by downloads in both full and compact surfaces. */
class TransferProgressIndicator final : public QWidget {
public:
    explicit TransferProgressIndicator(QWidget* parent = nullptr);
    ~TransferProgressIndicator() override;

    void setProgress(qint64 received, qint64 total);
    void setIndeterminate();
    void reset();

    [[nodiscard]] bool isIndeterminate() const;
    [[nodiscard]] int percentage() const;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setProgressRatio(qreal progress);

    QTimeLine* m_activityAnimation{nullptr};
    QVariantAnimation* m_valueAnimation{nullptr};
    qreal m_activityPhase{0.0};
    qreal m_displayedProgress{0.0};
    qreal m_targetProgress{0.0};
    bool m_indeterminate{false};
};

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
    TransferProgressIndicator* m_track{nullptr};
};

} // namespace qtm
