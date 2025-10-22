#pragma once

// 图片资源路径：仅用于 UI 图标
#define IMG_DIR "C:\\Users\\WANG\\Desktop\\MyOwn\\resource\\image\\"

// 工具栏 ID
enum ToolIDs {
    ID_TOOL_ARROW = wxID_HIGHEST + 1,
    ID_TOOL_TEXT,
    ID_TOOL_HAND,
    ID_TOOL_DELETE   // 新增删除工具
};