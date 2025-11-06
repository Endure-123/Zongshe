#pragma once

#include <functional>
#include <memory>
#include <vector>

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
};

class CommandManager {
public:
    using Callback = std::function<void()>;

    void Execute(std::unique_ptr<ICommand> command);
    void Undo();
    void Redo();

    bool CanUndo() const;
    bool CanRedo() const;

    void Clear();

    void SetOnStackChanged(Callback callback);

private:
    void Notify();

    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
    Callback m_callback;
};
