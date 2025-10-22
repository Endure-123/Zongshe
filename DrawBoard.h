#pragma once
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <wx/dcbuffer.h>
#include <vector>
#include <memory>
#include <array>
#include <json/json.h>
#include "AppConfig.h"
#include "Component.h"   // ← 引入矢量门

class DrawBoard : public wxPanel
{
public:
    DrawBoard(wxWindow* parent);
    ~DrawBoard();

    int currentTool = ID_TOOL_ARROW; // 当前工具
    int selectedGateIndex = -1;      // -1 表示没有选中

    // ====== 拖动所需状态 ======
    int     m_draggingIndex = -1;    // 正在拖动的元件下标（-1 表示无）
    bool    m_isDragging = false;
    wxPoint m_dragStartMouse;        // 鼠标按下时的位置
    wxPoint m_dragStartCenter;       // 元件按下时的中心

    // 线/文本/鼠标
    wxPoint currentStart, currentEnd, mousePos;
    bool isDrawingLine, isInsertingText, isDrawing;

    // 直线与文本仍保持你的原结构
    std::vector<std::pair<wxPoint, wxPoint>> lines;
    std::vector<std::pair<wxPoint, wxString>> texts;

    // ⭐ 组件集合：改为矢量门对象
    std::vector<std::unique_ptr<Component>> components;

    wxStaticText* st1;
    wxStaticText* st2;

    // 指向 cMain 的“当前选中元件类型名称”（例如 "AND", "OR", "NOT"...）
    wxString* pSelectedGateName = nullptr;

    // 对外功能
    void AddGate(const wxPoint& center, const wxString& typeName);
    void SelectGate(const wxPoint& pos);
    void DeleteSelectedGate();
    void ClearTexts();
    void ClearPics();

    // JSON
    void SaveToJson(const std::string& filename);
    void LoadFromJson(const std::string& filename);

    // 命中测试：给定一点，返回命中的元件下标（从上到下优先最上层）
    int HitTestGate(const wxPoint& pt) const;

    // （可选）对外暴露：获取当前选中元件的四个锚点
    std::vector<wxPoint> GetSelectedGateAnchors() const;

private:
    void OnPaint(wxPaintEvent& event);
    void OnButtonMove(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void drawGrid(wxGraphicsContext* gc);

    // 计算锚点（由门对象的 m_BoundaryPoints 给出）
    std::array<wxPoint, 4> GetGateAnchorPoints(const Component* comp) const;

    // 工具：名字↔类型映射 + 工厂
    static ComponentType NameToType(const wxString& s);
    static const char* TypeToName(ComponentType t);
    static std::unique_ptr<Component> MakeComponent(ComponentType t, const wxPoint& center);

    // 选中锚点样式（如需要你也可以自己画四点；各门类也会在选中时自行画定位点）
    static const wxColour HANDLE_FILL_RGB;
    static const wxColour HANDLE_STROKE_RGB;
    static constexpr int  HANDLE_RADIUS_PX = 5;
    static constexpr int  HANDLE_STROKE_PX = 1;
};
