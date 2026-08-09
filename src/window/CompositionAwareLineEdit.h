#pragma once

#include <QLineEdit>

class QFocusEvent;
class QInputMethodEvent;

namespace qtm {

/** QLineEdit that exposes the text currently rendered by an input method. */
class CompositionAwareLineEdit final : public QLineEdit {
    Q_OBJECT

public:
    explicit CompositionAwareLineEdit(QWidget* parent = nullptr);

    QString visualText() const;
    bool hasActiveComposition() const;
    void commitComposition();
    void cancelComposition();

signals:
    void visualTextChanged();

protected:
    void inputMethodEvent(QInputMethodEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    QString m_preeditText;
    bool m_handlingInputMethodEvent{false};
};

} // namespace qtm
