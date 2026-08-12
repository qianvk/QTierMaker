#pragma once

#include <QLabel>

namespace qtm {

/** A word-wrapping label whose layout height always follows its assigned width. */
class WrappingLabel final : public QLabel {
public:
    explicit WrappingLabel(QWidget* parent = nullptr);
    WrappingLabel(const QString& text, QWidget* parent = nullptr);

    void setText(const QString& text);

    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateRequiredHeight();
};

} // namespace qtm
