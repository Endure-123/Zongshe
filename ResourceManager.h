#pragma once
#include <wx/wx.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <set>
#include <vector>

class ResourceManager {
public:
    // 左侧树控件：加载图标+名称
    static wxImageList* LoadImageList(const wxString& imgDir, std::vector<wxString>& gateNames);

    // 工具栏按钮图标
    static wxBitmap LoadBitmap(const wxString& imgDir, const wxString& name, int size = -1);
};
