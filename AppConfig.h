#pragma once

// 全局配置 - 图片资源路径
#define IMG_DIR "C:\\Users\\WANG\\Desktop\\MyOwn\\resource\\image\\"
#define GATE_DRAW_SCALE 50   // 缩小到原图的 50%

// 工具栏 ID
enum ToolIDs {
    ID_TOOL_ARROW = wxID_HIGHEST + 1,
    ID_TOOL_TEXT,
    ID_TOOL_HAND,
    ID_TOOL_DELETE   // 新增删除工具
};