#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct BoardPoint {
    double x{0.0};
    double y{0.0};
};

struct ComponentData {
    std::string id;
    std::string type;
    BoardPoint position;
    std::map<std::string, std::string> properties;
};

class BoardModel {
public:
    using Listener = std::function<void()>;
    using ListenerToken = int;

    ComponentData &AddComponent(const ComponentData &data);
    bool RemoveComponent(const std::string &id, ComponentData *outData = nullptr);

    ComponentData *FindComponent(const std::string &id);
    const ComponentData *FindComponent(const std::string &id) const;

    const std::vector<ComponentData> &GetComponents() const { return m_components; }

    void Clear();

    void SelectComponent(const std::string &id);
    void ClearSelection();

    ComponentData *GetSelectedComponent();
    const ComponentData *GetSelectedComponent() const;

    bool SetComponentPosition(const std::string &id, const BoardPoint &pos, BoardPoint *oldPos = nullptr);
    bool UpdateComponentProperty(const std::string &id, const std::string &key, const std::string &value,
                                 std::string *oldValue = nullptr);

    ListenerToken AddListener(Listener callback);
    void RemoveListener(ListenerToken token);

private:
    ComponentData *DoFindComponent(const std::string &id);
    const ComponentData *DoFindComponent(const std::string &id) const;

    void Notify();

    std::vector<ComponentData> m_components;
    std::string m_selectedId;
    std::vector<std::pair<ListenerToken, Listener>> m_listeners;
    ListenerToken m_nextToken{1};
};
