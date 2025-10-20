#pragma once
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <wx/dcbuffer.h>
#include <vector>
#include <json/json.h>
#include "AppConfig.h"
#include <array>


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

    // ---- 选中锚点样式（四个小圆点）----
    static constexpr int  HANDLE_RADIUS_PX = 5;                           // 半径（像素）
    static constexpr int  HANDLE_STROKE_PX = 1;                           // 描边宽度
    static constexpr int  HANDLE_OFFSET_PX = 0;                           // 相对角点外扩(+)或内缩(-)
    //static constexpr auto HANDLE_FILL_RGB = wxColour(255, 255, 255);       // 填充：白
    //static constexpr auto HANDLE_STROKE_RGB = wxColour(0, 0, 0);             // 描边：黑
    static const wxColour HANDLE_FILL_RGB;
    static const wxColour HANDLE_STROKE_RGB;


    // ---- 计算/绘制锚点的小工具函数 ----
    std::array<wxPoint, 4> GetGateAnchorPoints(const wxPoint& topLeft, const wxSize& bmpSize) const;
    void DrawSelectionHandles(wxGraphicsContext* gc, const std::array<wxPoint, 4>& anchors);

    // （可选）对外暴露：获取当前选中元件的四个锚点，方便后续“连线吸附”
public:
    std::vector<wxPoint> GetSelectedGateAnchors() const;

};
