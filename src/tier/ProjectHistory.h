#pragma once

#include "tier/TierProject.h"

#include <QObject>
#include <QString>

#include <functional>

class QUndoStack;

namespace qtm {

/** Atomic project edit history backed by Qt's standard undo framework. */
class ProjectHistory final : public QObject {
    Q_OBJECT

public:
    struct State {
        State() = default;
        State(const TierProject& sourceProject, QString sourceSelectedImageId);
        State(const State& other);
        State& operator=(const State& other);
        State(State&&) noexcept = default;
        State& operator=(State&&) noexcept = default;

        TierProject project;
        QString selectedImageId;
    };

    using ApplyFunction = std::function<void(const State&)>;

    explicit ProjectHistory(ApplyFunction apply, QObject* parent = nullptr);

    /** Records an edit whose after state has already been applied to the live project. */
    bool push(const State& before, const State& after, const QString& text);
    void clear();
    void setClean();
    void undo();
    void redo();

    bool isClean() const;
    bool canUndo() const;
    bool canRedo() const;
    int count() const;
    int index() const;

signals:
    void stateChanged(bool clean, int index);

private:
    void notifyStateChanged();

    ApplyFunction m_apply;
    QUndoStack* m_stack{nullptr};
    bool m_reportedClean{true};
    int m_reportedIndex{0};
};

} // namespace qtm
