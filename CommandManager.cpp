#include "CommandManager.h"

void CommandManager::Execute(std::unique_ptr<ICommand> command) {
    if (!command) {
        return;
    }

    command->Execute();
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();
    Notify();
}

void CommandManager::Undo() {
    if (m_undoStack.empty()) {
        return;
    }

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->Undo();
    m_redoStack.push_back(std::move(command));
    Notify();
}

void CommandManager::Redo() {
    if (m_redoStack.empty()) {
        return;
    }

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->Execute();
    m_undoStack.push_back(std::move(command));
    Notify();
}

bool CommandManager::CanUndo() const { return !m_undoStack.empty(); }

bool CommandManager::CanRedo() const { return !m_redoStack.empty(); }

void CommandManager::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    Notify();
}

void CommandManager::SetOnStackChanged(Callback callback) { m_callback = std::move(callback); }

void CommandManager::Notify() {
    if (m_callback) {
        m_callback();
    }
}
