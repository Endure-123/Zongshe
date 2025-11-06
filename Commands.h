#pragma once

#include "BoardModel.h"
#include "CommandManager.h"

#include <optional>

class AddComponentCommand : public ICommand {
public:
    AddComponentCommand(BoardModel &board, ComponentData data);

    void Execute() override;
    void Undo() override;

private:
    BoardModel &m_board;
    ComponentData m_data;
    bool m_created{false};
};

class RemoveComponentCommand : public ICommand {
public:
    RemoveComponentCommand(BoardModel &board, std::string id);

    void Execute() override;
    void Undo() override;

private:
    BoardModel &m_board;
    std::string m_id;
    std::optional<ComponentData> m_snapshot;
};

class MoveComponentCommand : public ICommand {
public:
    MoveComponentCommand(BoardModel &board, std::string id, BoardPoint from, BoardPoint to);

    void Execute() override;
    void Undo() override;

private:
    BoardModel &m_board;
    std::string m_id;
    BoardPoint m_from;
    BoardPoint m_to;
};

class UpdatePropertyCommand : public ICommand {
public:
    UpdatePropertyCommand(BoardModel &board, std::string id, std::string key, std::string newValue);

    void Execute() override;
    void Undo() override;

private:
    BoardModel &m_board;
    std::string m_id;
    std::string m_key;
    std::string m_newValue;
    std::optional<std::string> m_oldValue;
};
