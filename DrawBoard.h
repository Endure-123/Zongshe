#pragma once
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <wx/dcbuffer.h>
#include <vector>
#include <json/json.h>
#include "AppConfig.h"

// 保存元件信息
struct GateInfo {
    wxPoint pos;
    wxString name;
    wxBitmap bmp;
};

class DrawBoard : public wxPanel
{
public:
    DrawBoard(wxWindow* parent);
    ~DrawBoard();

    int currentTool = ID_TOOL_ARROW; // 当前工具
    int selectedGateIndex = -1;      // -1 表示没有选中

    wxPoint currentStart, currentEnd, mousePos;
    bool isDrawingLine, isInsertingText, isDrawing;

    std::vector<std::pair<wxPoint, wxPoint>> lines;
    std::vector<std::pair<wxPoint, wxString>> texts;
    std::vector<GateInfo> gates;

    wxStaticText* st1;
    wxStaticText* st2;

    wxString* pSelectedGateName = nullptr; // 指向 cMain 的当前选中元件

    void AddGate(const wxPoint& pos, const wxBitmap& bmp, const wxString& name);
    void SelectGate(const wxPoint& pos);   // 根据鼠标位置选择元件
    void DeleteSelectedGate();             // 删除元件
    void ClearTexts();
    void ClearPics();

    // JSON 保存/加载
    void SaveToJson(const std::string& filename);
    void LoadFromJson(const std::string& filename);

private:
    void OnPaint(wxPaintEvent& event);
    void OnButtonMove(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void drawGrid(wxGraphicsContext* gc);
};
