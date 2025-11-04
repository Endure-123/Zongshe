#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace bookshelf {

    struct BSNode {
        std::string name;
        double width = 0, height = 0;
        bool terminal = false;
    };

    struct BSPin {
        std::string cellName;
        std::string pinName; // 可能是 I/O 或 P0/P1...
        double dx = 0.0, dy = 0.0; // 相对 cell 中心
    };

    struct BSNet {
        std::string name;
        std::vector<BSPin> pins;
    };

    struct BSDesign {
        std::vector<BSNode> nodes;
        std::vector<BSNet> nets;
        std::unordered_map<std::string, size_t> nodeIndexByName;
    };

    /// 解析 .nodes 与 .nets（UTF-8 文本）
    BSDesign ParseBookShelf(const std::filesystem::path& nodesPath,
        const std::filesystem::path& netsPath);

} // namespace bookshelf
