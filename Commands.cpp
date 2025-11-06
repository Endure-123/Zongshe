#include "Commands.h"

#include <utility>

AddComponentCommand::AddComponentCommand(BoardModel &board, ComponentData data)
    : m_board(board), m_data(std::move(data)) {}

void AddComponentCommand::Execute() {
    if (!m_created) {
        m_board.AddComponent(m_data);
        m_created = true;
    } else {
        m_board.AddComponent(m_data);
    }
    m_board.SelectComponent(m_data.id);
}

void AddComponentCommand::Undo() { m_board.RemoveComponent(m_data.id); }

RemoveComponentCommand::RemoveComponentCommand(BoardModel &board, std::string id)
    : m_board(board), m_id(std::move(id)) {}

void RemoveComponentCommand::Execute() {
    if (!m_snapshot.has_value()) {
        ComponentData snapshot;
        if (m_board.RemoveComponent(m_id, &snapshot)) {
            m_snapshot = snapshot;
        }
    } else {
        m_board.RemoveComponent(m_id);
    }
}

void RemoveComponentCommand::Undo() {
    if (m_snapshot.has_value()) {
        m_board.AddComponent(*m_snapshot);
        m_board.SelectComponent(m_snapshot->id);
    }
}

MoveComponentCommand::MoveComponentCommand(BoardModel &board, std::string id, BoardPoint from, BoardPoint to)
    : m_board(board), m_id(std::move(id)), m_from(from), m_to(to) {}

void MoveComponentCommand::Execute() { m_board.SetComponentPosition(m_id, m_to); }

void MoveComponentCommand::Undo() { m_board.SetComponentPosition(m_id, m_from); }

UpdatePropertyCommand::UpdatePropertyCommand(BoardModel &board, std::string id, std::string key, std::string newValue)
    : m_board(board), m_id(std::move(id)), m_key(std::move(key)), m_newValue(std::move(newValue)) {}

void UpdatePropertyCommand::Execute() {
    std::string old;
    if (!m_oldValue.has_value()) {
        if (m_board.UpdateComponentProperty(m_id, m_key, m_newValue, &old)) {
            m_oldValue = old;
        }
    } else {
        m_board.UpdateComponentProperty(m_id, m_key, m_newValue);
    }
}

void UpdatePropertyCommand::Undo() {
    if (!m_oldValue.has_value()) {
        return;
    }
    m_board.UpdateComponentProperty(m_id, m_key, *m_oldValue);
}
