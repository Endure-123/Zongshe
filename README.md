# Zongshe Diagram Editor

该仓库提供一个基于 wxWidgets 的电路/逻辑图编辑器。当前实现保留了解耦合后的核心模型与 GUI 层，但所有源码均位于仓库根目录，便于直接在 Visual Studio 2022 或其他 IDE 中管理。

## 主要模块
- **BoardModel / Commands**：维护元件、选择状态和可撤销命令栈，不依赖任何 wxWidgets 类型。
- **BoardSerializer**：基于 `json` 目录下的 jsoncpp 实现 JSON 导入与导出，保存 `.json` 格式的工程文件。
- **DrawBoard / PropertyPane**：wxWidgets 面板，仅负责渲染、鼠标交互与属性编辑，通过模型接口驱动。
- **MainFrame / App**：应用入口、菜单/工具栏、文件对话框以及资源图标加载（依赖 `resource/image` 下的 PNG）。

## 构建与运行
```bash
cmake -S . -B build
cmake --build build
./build/zongshe
```

> 注意：需要事先安装 wxWidgets (>=3.1) 以及 jsoncpp 随附于仓库的 `json/` 源码即可。
