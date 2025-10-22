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
    // 替换旧的直线容器：每根线是一条折线（点序列）
    std::vector<std::vector<wxPoint>> wires;

    // 画线状态
    wxPoint lineStart;                               // ★ 新增：起点（已吸附/对齐后）
    bool    isRouting = false;                       // ★ 新增：是否正在画线

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
    // 该元件移动前的引脚坐标
    std::vector<wxPoint> preMovePins;               


    // 工具：名字↔类型映射 + 工厂
    static ComponentType NameToType(const wxString& s);
    static const char* TypeToName(ComponentType t);
    static std::unique_ptr<Component> MakeComponent(ComponentType t, const wxPoint& center);

    // 选中锚点样式（如需要你也可以自己画四点；各门类也会在选中时自行画定位点）
    static const wxColour HANDLE_FILL_RGB;
    static const wxColour HANDLE_STROKE_RGB;
    static constexpr int  HANDLE_RADIUS_PX = 5;
    static constexpr int  HANDLE_STROKE_PX = 1;

    // 找到最近引脚（返回是否命中），out 为吸附后的坐标
    bool FindNearestPin(const wxPoint& p, wxPoint& out, int* compIdx = nullptr) const;

    // 生成 L 形曼哈顿路径（start→end），优先水平再垂直（或反之均可）
    std::vector<wxPoint> MakeManhattan(const wxPoint& a, const wxPoint& b) const;

    // 可选：吸附到网格
    wxPoint SnapToGrid(const wxPoint& p) const;

    wxPoint SnapToStep(const wxPoint& p) const;     // 半格吸附
    void RerouteWiresForMovedComponent(int compIdx); // 线跟随重算


    // 参数
    static constexpr int GRID = 20;
    static constexpr int STEP = GRID / 2;           // 半格步进
    static constexpr int SNAP_PIN_RADIUS = 12;   // 鼠标在这个半径内就吸附到引脚
};
