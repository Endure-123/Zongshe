#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace bookshelf {

    struct Node {
        std::string name;    // 唯一名
        double width;        // 像素/任意单位
        double height;       // 同上
        bool terminal = false; // 终端(IO)则 true
    };

    struct Pin {
        std::string cellName; // 所属单元名
        std::string pinName;  // 引脚名（P0/P1/... 或 I/O 等）
        double dx = 0.0;      // 相对单元中心的偏移
        double dy = 0.0;
    };

    struct Net {
        std::string name;
        std::vector<Pin> pins;
    };

    struct ExportOptions {
        std::filesystem::path outDir = "output";
        bool forceCreateDir = true;
        int floatPrecision = 6;
    };

    std::pair<std::filesystem::path, std::filesystem::path>
        ExportBookShelf(const std::string& projectName,
            const std::vector<Node>& nodes,
            const std::vector<Net>& nets,
            const ExportOptions& opt = {});

} // namespace bookshelf
