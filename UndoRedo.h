#pragma once
#include <memory>
#include <stack>
#include <functional>

struct ICommand {
    virtual ~ICommand() = default;
    virtual void Do() = 0;
    virtual void Undo() = 0;
};

class CommandManager {
public:
    using OnChanged = std::function<void(bool canUndo, bool canRedo)>;

    void SetOnChanged(OnChanged cb) { onChanged_ = std::move(cb); }

    void Execute(std::unique_ptr<ICommand> cmd) {
        if (!cmd) return;
        cmd->Do();
        undo_.push(std::move(cmd));
        while (!redo_.empty()) redo_.pop();
        notify();
    }

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    void Undo() {
        if (undo_.empty()) return;
        auto cmd = std::move(undo_.top()); undo_.pop();
        cmd->Undo();
        redo_.push(std::move(cmd));
        notify();
    }

    void Redo() {
        if (redo_.empty()) return;
        auto cmd = std::move(redo_.top()); redo_.pop();
        cmd->Do();
        undo_.push(std::move(cmd));
        notify();
    }

    void Clear() {
        while (!undo_.empty()) undo_.pop();
        while (!redo_.empty()) redo_.pop();
        notify();
    }

private:
    void notify() { if (onChanged_) onChanged_(CanUndo(), CanRedo()); }
    std::stack<std::unique_ptr<ICommand>> undo_, redo_;
    OnChanged onChanged_;
};
