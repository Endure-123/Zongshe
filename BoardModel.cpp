#include "BoardModel.h"

#include <algorithm>
#include <stdexcept>

ComponentData &BoardModel::AddComponent(const ComponentData &data) {
    if (FindComponent(data.id) != nullptr) {
        throw std::runtime_error("Component with duplicated id: " + data.id);
    }

    m_components.push_back(data);
    Notify();
    return m_components.back();
}

bool BoardModel::RemoveComponent(const std::string &id, ComponentData *outData) {
    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [&](const ComponentData &comp) { return comp.id == id; });
    if (it == m_components.end()) {
        return false;
    }

    if (outData) {
        *outData = *it;
    }

    m_components.erase(it);
    if (m_selectedId == id) {
        m_selectedId.clear();
    }
    Notify();
    return true;
}

ComponentData *BoardModel::FindComponent(const std::string &id) { return DoFindComponent(id); }

const ComponentData *BoardModel::FindComponent(const std::string &id) const { return DoFindComponent(id); }

ComponentData *BoardModel::GetSelectedComponent() { return DoFindComponent(m_selectedId); }

const ComponentData *BoardModel::GetSelectedComponent() const { return DoFindComponent(m_selectedId); }

void BoardModel::Clear() {
    m_components.clear();
    m_selectedId.clear();
    Notify();
}

void BoardModel::SelectComponent(const std::string &id) {
    if (m_selectedId == id) {
        return;
    }

    if (id.empty() || FindComponent(id) != nullptr) {
        m_selectedId = id;
        Notify();
    }
}

void BoardModel::ClearSelection() {
    if (!m_selectedId.empty()) {
        m_selectedId.clear();
        Notify();
    }
}

bool BoardModel::SetComponentPosition(const std::string &id, const BoardPoint &pos, BoardPoint *oldPos) {
    ComponentData *comp = FindComponent(id);
    if (!comp) {
        return false;
    }

    if (oldPos) {
        *oldPos = comp->position;
    }

    comp->position = pos;
    Notify();
    return true;
}

bool BoardModel::UpdateComponentProperty(const std::string &id, const std::string &key, const std::string &value,
                                         std::string *oldValue) {
    ComponentData *comp = FindComponent(id);
    if (!comp) {
        return false;
    }

    if (oldValue) {
        auto iter = comp->properties.find(key);
        if (iter != comp->properties.end()) {
            *oldValue = iter->second;
        } else {
            oldValue->clear();
        }
    }

    comp->properties[key] = value;
    Notify();
    return true;
}

BoardModel::ListenerToken BoardModel::AddListener(Listener callback) {
    const ListenerToken token = m_nextToken++;
    m_listeners.emplace_back(token, std::move(callback));
    return token;
}

void BoardModel::RemoveListener(ListenerToken token) {
    for (auto &entry : m_listeners) {
        if (entry.first == token) {
            entry.second = nullptr;
            break;
        }
    }
}

ComponentData *BoardModel::DoFindComponent(const std::string &id) {
    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [&](const ComponentData &comp) { return comp.id == id; });
    if (it == m_components.end()) {
        return nullptr;
    }
    return &(*it);
}

const ComponentData *BoardModel::DoFindComponent(const std::string &id) const {
    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [&](const ComponentData &comp) { return comp.id == id; });
    if (it == m_components.end()) {
        return nullptr;
    }
    return &(*it);
}

void BoardModel::Notify() {
    for (const auto &entry : m_listeners) {
        if (entry.second) {
            entry.second();
        }
    }
}
