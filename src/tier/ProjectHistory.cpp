#include "tier/ProjectHistory.h"

#include <QUndoCommand>
#include <QUndoStack>

#include <utility>

namespace qtm {

namespace {
constexpr int kMaximumUndoCommands = 200;

class ProjectStateCommand final : public QUndoCommand {
public:
    ProjectStateCommand(ProjectHistory::State before, ProjectHistory::State after, QString text,
                        ProjectHistory::ApplyFunction apply)
        : QUndoCommand(std::move(text)), m_before(std::move(before)), m_after(std::move(after)),
          m_apply(std::move(apply)) {}

    void undo() override {
        m_apply(m_before);
    }

    void redo() override {
        // QUndoStack calls redo() synchronously from push(). The editor has already applied the
        // mutation before it captures the after state, so replaying that snapshot here would reset
        // the model while the originating input event (notably a drop event) is still unwinding.
        if (m_initialRedo) {
            m_initialRedo = false;
            return;
        }
        m_apply(m_after);
    }

private:
    ProjectHistory::State m_before;
    ProjectHistory::State m_after;
    ProjectHistory::ApplyFunction m_apply;
    bool m_initialRedo{true};
};
} // namespace

ProjectHistory::State::State(const TierProject& sourceProject, QString sourceSelectedImageId)
    : project(sourceProject.detachedCopy()),
      selectedImageId(std::move(sourceSelectedImageId)) {}

ProjectHistory::State::State(const State& other)
    : project(other.project.detachedCopy()), selectedImageId(other.selectedImageId) {}

ProjectHistory::State& ProjectHistory::State::operator=(const State& other) {
    if (this == &other) {
        return *this;
    }
    project = other.project.detachedCopy();
    selectedImageId = other.selectedImageId;
    return *this;
}

ProjectHistory::ProjectHistory(ApplyFunction apply, QObject* parent)
    : QObject(parent), m_apply(std::move(apply)), m_stack(new QUndoStack(this)) {
    m_stack->setUndoLimit(kMaximumUndoCommands);
    connect(m_stack, &QUndoStack::indexChanged, this,
            [this](int) { notifyStateChanged(); });
    connect(m_stack, &QUndoStack::cleanChanged, this,
            [this](bool) { notifyStateChanged(); });
}

bool ProjectHistory::push(const State& before, const State& after, const QString& text) {
    if (before.project.hasSamePersistentContent(after.project)) {
        return false;
    }
    m_stack->push(new ProjectStateCommand(before, after, text, m_apply));
    return true;
}

void ProjectHistory::clear() {
    m_stack->clear();
    notifyStateChanged();
}

void ProjectHistory::setClean() {
    m_stack->setClean();
    notifyStateChanged();
}

void ProjectHistory::undo() {
    m_stack->undo();
}

void ProjectHistory::redo() {
    m_stack->redo();
}

bool ProjectHistory::isClean() const {
    return m_stack->isClean();
}

bool ProjectHistory::canUndo() const {
    return m_stack->canUndo();
}

bool ProjectHistory::canRedo() const {
    return m_stack->canRedo();
}

int ProjectHistory::count() const {
    return m_stack->count();
}

int ProjectHistory::index() const {
    return m_stack->index();
}

void ProjectHistory::notifyStateChanged() {
    const bool clean = m_stack->isClean();
    const int currentIndex = m_stack->index();
    if (clean == m_reportedClean && currentIndex == m_reportedIndex) {
        return;
    }
    m_reportedClean = clean;
    m_reportedIndex = currentIndex;
    emit stateChanged(clean, currentIndex);
}

} // namespace qtm
