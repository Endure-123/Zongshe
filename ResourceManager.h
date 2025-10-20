#pragma once
#include <wx/wx.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <set>
#include <vector>

class ResourceManager {
public:
    // 加载所有元器件图标到 ImageList，并返回名字列表
    static wxImageList* LoadImageList(const wxString& imgDir, std::vector<wxString>& gateNames);

    // 加载原始位图（可指定缩放大小，size=-1 表示原始大小）
    static wxBitmap LoadBitmap(const wxString& imgDir, const wxString& name, int size = -1);
    
    // 新增：按百分比缩放（画布元器件用）
    static wxBitmap LoadBitmapByPercent(const wxString& imgDir, const wxString& name, int scalePercent = 100);
};
