#pragma once
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <wx/dcbuffer.h>
#include <vector>
#include <memory>
#include <array>
#include <optional>
#include <json/json.h>
#include "AppConfig.h"
#include "Component.h"
#include "SelectionEvents.h"
#include "UndoRedo.h"
#include <filesystem>
#include "BookShelfExporter.h"

// 统一选择类型（供属性面板查询）
enum class SelKind { None = 0, Gate = 1, Wire = 2 };

// —— 快照结构（用于 Undo/Redo）——
struct GateSnapshot {
    ComponentType type;
    wxPoint center;
    double scale{ 1.0 };
};

struct WireSnapshot {
    std::vector<wxPoint> poly;
};

class Simulator; // 前向声明

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
    bool isDrawingLine = false, isInsertingText = false, isDrawing = false;
    int selectedTextIndex = -1;           // 选中的文本（texts 数组下标）

    // 文本拖动状态（与元件拖动解耦）
    int     m_dragTextIndex = -1;
    bool    m_isDraggingText = false;
    wxPoint m_dragTextStartMouse;
    wxPoint m_dragTextStartPos;

    // —— 文本相关接口 —— 
    int  HitTestText(const wxPoint& pt) const;  // 命中哪个文本（-1 表示未命中）
    void DeleteSelectedText();                   // 删除选中文本（不进撤销栈）

    // 替换旧的直线容器：每根线是一条折线（点序列）
    std::vector<std::vector<wxPoint>> wires;

    // 画线状态
    wxPoint lineStart;                               // 起点（已吸附/对齐后）
    bool    isRouting = false;                       // 是否正在画线

    // 直线与文本仍保持你的原结构
    std::vector<std::pair<wxPoint, wxPoint>> lines;
    std::vector<std::pair<wxPoint, wxString>> texts;

    // ⭐ 组件集合：矢量门对象
    std::vector<std::unique_ptr<Component>> components;

    wxStaticText* st1 = nullptr;
    wxStaticText* st2 = nullptr;

    // 指向 cMain 的“当前选中元件类型名称”（例如 "AND", "OR", "NOT"...）
    wxString* pSelectedGateName = nullptr;

    // === 属性面板查询所需的选择状态接口 ===
    SelKind GetSelectionKind() const { return m_selKind; }
    long    GetSelectionId()   const { return m_selId; } // Gate / Wire 的索引

    // 可选：外部强制切换（目前面板不调用，预留）
    void SelectNone();
    void SelectGateByIndex(int idx);
    void SelectWireByIndex(int idx);

    // ===== 对外功能（保持你的接口）=====
    void AddGate(const wxPoint& center, const wxString& typeName);
    void SelectGate(const wxPoint& pos);
    void DeleteSelectedGate();
    void DeleteSelectedWire();
    void DeleteSelection();                  // 优先删除元件，否则删除线
    void ClearTexts();
    void ClearPics();

	// ===== 书架格式导入导出 =====
    bool ExportAsBookShelf(const std::string& projectName,
        const std::filesystem::path& outDir);
    bool ImportBookShelf(const std::filesystem::path& nodesPath,
        const std::filesystem::path& netsPath);

    // JSON
    void SaveToJson(const std::string& filename);
    void LoadFromJson(const std::string& filename);

    // 命中测试：给定一点，返回命中的元件下标（从上到下优先最上层）
    int HitTestGate(const wxPoint& pt) const;

    // （可选）对外暴露：获取当前选中元件的四个锚点
    std::vector<wxPoint> GetSelectedGateAnchors() const;

    // 工具：名字↔类型映射 + 工厂
    static ComponentType NameToType(const wxString& s);
    static const char* TypeToName(ComponentType t);

    // ===== 与命令管理器对接 =====
    void SetCommandManager(CommandManager* m) { m_cmd = m; }

    // —— 命令层需要的最小 API（被 ICommand 调用）——
    long AddGateFromSnapshot(const GateSnapshot& s);                    // 返回 gate 索引
    std::optional<GateSnapshot> ExportGateByIndex(long id) const;       // 导出 gate 快照
    void DeleteGateByIndex(long id);                                     // 删除 gate
    void MoveGateTo(long id, const wxPoint& pos);                        // 移动 gate

    long AddWire(const WireSnapshot& w);                                 // 返回 wire 索引
    std::optional<WireSnapshot> ExportWireByIndex(long id) const;        // 导出 wire 快照
    void DeleteWireByIndex(long id);                                      // 删除 wire

    void ZoomInCenter();                                        // 以视窗中心放大
    void ZoomOutCenter();                                       // 以视窗中心缩小
    void ZoomBy(double factor, const wxPoint& anchorDevicePt);  // 以给定屏幕点为锚

    // 仿真控制
    void SimStart();
    void SimStop();
    void SimStep();
    bool IsSimulating() const { return m_simulating; }
    void ToggleStartNodeAt(const wxPoint& pos);  // 点击切换起始节点电平
    void DrawNodeStates(wxGraphicsContext* gc);  // 绘制节点状态

    //节点吸附
    bool FindNearestGatePin(const wxPoint& pos, wxPoint& snappedPos, int tolerance = 10) const;

    // 供渲染时查询某条 wire 是否高电平
    bool IsWireHighForPaint(int wireIndex) const;

    Simulator* m_sim = nullptr;

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

    static std::unique_ptr<Component> MakeComponent(ComponentType t, const wxPoint& center);

    // 选中锚点样式
    static const wxColour HANDLE_FILL_RGB;
    static const wxColour HANDLE_STROKE_RGB;
    static constexpr int  HANDLE_RADIUS_PX = 5;
    static constexpr int  HANDLE_STROKE_PX = 1;

    // 找到最近引脚（返回是否命中），out 为吸附后的坐标
    bool FindNearestPin(const wxPoint& p, wxPoint& out, int* compIdx = nullptr) const;

    // 生成 L 形曼哈顿路径（start→end）
    std::vector<wxPoint> MakeManhattan(const wxPoint& a, const wxPoint& b) const;

    // 吸附到网格/步进
    wxPoint SnapToGrid(const wxPoint& p) const;
    wxPoint SnapToStep(const wxPoint& p) const;      // 半格吸附
    void RerouteWiresForMovedComponent(int compIdx, const std::vector<wxPoint>& prevPins); // 线跟随重算（传入移动前引脚坐标）

    int selectedWireIndex = -1;          // 选中的连线（wires 数组下标）

    // 命中测试：点是否命中某条折线（返回下标；未命中返回 -1）
    int HitTestWire(const wxPoint& pt) const;

    // 点到线段的平方距离
    static int Dist2_PointToSeg(const wxPoint& p, const wxPoint& a, const wxPoint& b);

    // 参数
    static constexpr int GRID = 20;
    static constexpr int STEP = GRID / 2;            // 半格步进
    static constexpr int SNAP_PIN_RADIUS = 12;       // 鼠标在这个半径内就吸附到引脚

    // 线条命中阈值（像素）
    static constexpr int LINE_HIT_PX = 6;

    // ★ 统一选择类型与索引（供属性面板/外部查询）
    SelKind m_selKind = SelKind::None;
    long    m_selId = -1;

    // ★ 选择变化事件
    void NotifySelectionChanged();

    // ★ 命令管理器
    CommandManager* m_cmd = nullptr;

    
    bool m_simulating = false;
    wxTimer* m_simTimer = nullptr;

    void OnTimer(wxTimerEvent& e);
};