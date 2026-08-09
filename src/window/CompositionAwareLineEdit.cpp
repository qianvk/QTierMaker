#include "window/CompositionAwareLineEdit.h"

#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>

namespace qtm {

CompositionAwareLineEdit::CompositionAwareLineEdit(QWidget* parent) : QLineEdit(parent) {
    connect(this, &QLineEdit::textChanged, this, [this]() {
        if (!m_handlingInputMethodEvent) {
            emit visualTextChanged();
        }
    });
}

QString CompositionAwareLineEdit::visualText() const {
    QString visibleText = text();
    if (!m_preeditText.isEmpty()) {
        visibleText.insert(qBound(0, cursorPosition(), visibleText.size()), m_preeditText);
    }
    return visibleText;
}

bool CompositionAwareLineEdit::hasActiveComposition() const {
    return !m_preeditText.isEmpty();
}

void CompositionAwareLineEdit::commitComposition() {
    if (hasActiveComposition() && QGuiApplication::inputMethod()) {
        QGuiApplication::inputMethod()->commit();
    }
}

void CompositionAwareLineEdit::cancelComposition() {
    if (hasActiveComposition() && QGuiApplication::inputMethod()) {
        QGuiApplication::inputMethod()->reset();
    }
    if (!m_preeditText.isEmpty()) {
        m_preeditText.clear();
        emit visualTextChanged();
    }
}

void CompositionAwareLineEdit::inputMethodEvent(QInputMethodEvent* event) {
    m_handlingInputMethodEvent = true;
    QLineEdit::inputMethodEvent(event);
    m_preeditText = event ? event->preeditString() : QString();
    m_handlingInputMethodEvent = false;
    emit visualTextChanged();
}

void CompositionAwareLineEdit::focusOutEvent(QFocusEvent* event) {
    QLineEdit::focusOutEvent(event);
    if (!m_preeditText.isEmpty()) {
        m_preeditText.clear();
        emit visualTextChanged();
    }
}

} // namespace qtm
