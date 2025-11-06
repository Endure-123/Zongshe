#include "BoardSerializer.h"

#include "json/json.h"

#include <fstream>
#include <stdexcept>

void BoardSerializer::SaveToFile(const BoardModel &board, const std::string &path) {
    Json::Value root(Json::objectValue);
    root["components"] = Json::arrayValue;

    for (const auto &component : board.GetComponents()) {
        Json::Value node(Json::objectValue);
        node["id"] = component.id;
        node["type"] = component.type;

        Json::Value position(Json::objectValue);
        position["x"] = component.position.x;
        position["y"] = component.position.y;
        node["position"] = position;

        Json::Value props(Json::objectValue);
        for (const auto &entry : component.properties) {
            props[entry.first] = entry.second;
        }
        node["properties"] = props;

        root["components"].append(node);
    }

    if (const ComponentData *selected = board.GetSelectedComponent()) {
        root["selected"] = selected->id;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";

    std::ofstream stream(path, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    stream << Json::writeString(builder, root);
}

void BoardSerializer::LoadFromFile(const std::string &path, BoardModel &board) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        throw std::runtime_error("Failed to parse JSON: " + errors);
    }

    board.Clear();

    const Json::Value components = root.get("components", Json::arrayValue);
    for (const auto &node : components) {
        ComponentData data;
        data.id = node.get("id", "").asString();
        data.type = node.get("type", "component").asString();

        const Json::Value &position = node["position"];
        data.position.x = position.get("x", 0.0).asDouble();
        data.position.y = position.get("y", 0.0).asDouble();

        const Json::Value &props = node["properties"];
        if (props.isObject()) {
            for (const auto &name : props.getMemberNames()) {
                data.properties[name] = props[name].asString();
            }
        }

        board.AddComponent(data);
    }

    const std::string selected = root.get("selected", "").asString();
    if (!selected.empty()) {
        board.SelectComponent(selected);
    } else {
        board.ClearSelection();
    }
}
