#include "DrawBoard.h"
#include <fstream>
#include "SelectionEvents.h"
#include "EditCommands.h"
#include <cmath>   // 为 std::lround
// === 追加：导出 BookShelf 网表 ===
#include "BookShelfExporter.h"
#include <unordered_map>
#include <map>
#include <set>
#include "BookShelfImporter.h"
#include "Simulator.h"
#include "Component.h"
using bookshelf::BSDesign;
using bookshelf::ParseBookShelf;
using bookshelf::Node;
using bookshelf::Net;
using bookshelf::Pin;
using bookshelf::ExportOptions;



namespace {
    // 轴对齐包围盒宽高（由 m_BoundaryPoints[4] 推得）
    inline std::pair<double, double> AABBwh(const wxPoint pts[4]) {
        int minx = pts[0].x, maxx = pts[0].x, miny = pts[0].y, maxy = pts[0].y;
        for (int i = 1; i < 4; ++i) {
            minx = std::min(minx, pts[i].x);
            maxx = std::max(maxx, pts[i].x);
            miny = std::min(miny, pts[i].y);
            maxy = std::max(maxy, pts[i].y);
        }
        return { double(maxx - minx), double(maxy - miny) };
    }

    inline bool NearlyEqual(const wxPoint& a, const wxPoint& b, int tol = 1) {
        return (std::abs(a.x - b.x) <= tol) && (std::abs(a.y - b.y) <= tol);
    }

    inline std::string MakeNodeName(const char* base, int idx) {
        return std::string(base) + "_" + std::to_string(idx);
    }

    // 给 component 的每个 pin 生成名字：P0,P1,...
    inline std::string PinNameByIndex(int i) { return "P" + std::to_string(i); }

    // 简单判定：起始/终止节点 -> terminal，其它为普通 cell
    inline bool IsTerminal(ComponentType t) {
        return (t == NODE_START || t == NODE_END);
    }
}



// 选中锚点配色
const wxColour DrawBoard::HANDLE_FILL_RGB(255, 255, 255);
const wxColour DrawBoard::HANDLE_STROKE_RGB(0, 0, 0);

// ★ 在一个 .cpp 里实现一次全局事件
wxDEFINE_EVENT(EVT_SELECTION_CHANGED, wxCommandEvent);

DrawBoard::DrawBoard(wxWindow* parent) : wxPanel(parent)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &DrawBoard::OnPaint, this);
    Bind(wxEVT_MOTION, &DrawBoard::OnButtonMove, this);
    Bind(wxEVT_LEFT_DOWN, &DrawBoard::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &DrawBoard::OnLeftUp, this);

    st1 = new wxStaticText(this, -1, wxT(""), wxPoint(10, 10));
    st2 = new wxStaticText(this, -1, wxT(""), wxPoint(10, 30));

    // 创建仿真器与定时器
    m_sim = new Simulator(this);
    m_simTimer = new wxTimer(this);
    Bind(wxEVT_TIMER, &DrawBoard::OnTimer, this);
}

DrawBoard::~DrawBoard()
{
    if (m_simTimer && m_simTimer->IsRunning()) {
        m_simTimer->Stop();
    }
    Unbind(wxEVT_TIMER, &DrawBoard::OnTimer, this);
    delete m_simTimer;
    m_simTimer = nullptr;

    delete m_sim;
    m_sim = nullptr;
}

void DrawBoard::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();

    wxBitmap bufferBitmap(GetClientSize());
    wxMemoryDC memDC;
    memDC.SelectObject(bufferBitmap);
    memDC.SetBackground(*wxWHITE_BRUSH);
    memDC.Clear();

    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (gc) {
        // 背景网格
        drawGrid(gc);

        // ====== A) 已保存的线（折线绘制）======
        for (int wi = 0; wi < (int)wires.size(); ++wi) {
            const auto& poly = wires[wi];

            // 仿真着色：高电平红色，低电平蓝灰；未仿真则默认黑
            if (m_simulating && m_sim) {
                bool high = m_sim->IsWireHigh(wi);
                wxColour cc = high ? wxColour(255, 0, 0) : wxColour(100, 120, 200);
                gc->SetPen(wxPen(cc, 2));
            }
            else {
                gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
            }

            for (size_t i = 1; i < poly.size(); ++i) {
                gc->StrokeLine(poly[i - 1].x, poly[i - 1].y, poly[i].x, poly[i].y);
            }
        }

        // ====== A1) 被选中的连线：两端定位点 ======
        if (selectedWireIndex >= 0 && selectedWireIndex < (int)wires.size()) {
            const auto& poly = wires[selectedWireIndex];
            if (poly.size() >= 2) {
                const wxPoint& p0 = poly.front();
                const wxPoint& p1 = poly.back();

                gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
                gc->SetBrush(*wxWHITE_BRUSH);
                const int r = 5;
                gc->DrawEllipse(p0.x - r, p0.y - r, 2 * r, 2 * r);
                gc->DrawEllipse(p1.x - r, p1.y - r, 2 * r, 2 * r);
            }
        }

        // ====== B) 预览线（正在布线）======
        if (isRouting) {
            wxPoint hover = mousePos, snapEnd;
            if (FindNearestPin(mousePos, snapEnd, nullptr)) hover = snapEnd;
            else hover = SnapToGrid(mousePos);

            auto preview = MakeManhattan(lineStart, hover);
            gc->SetPen(wxPen(wxColour(0, 0, 255), 2, wxPENSTYLE_DOT));
            for (size_t i = 1; i < preview.size(); ++i) {
                gc->StrokeLine(preview[i - 1].x, preview[i - 1].y,
                    preview[i].x, preview[i].y);
            }
        }

        // 文本
        for (const auto& txt : texts) {
            gc->SetFont(wxFontInfo(12).Family(wxFONTFAMILY_DEFAULT), *wxBLACK);
            gc->DrawText(txt.second, txt.first.x, txt.first.y);
        }

        // ====== 新增：被选中文本的可视化选框 ======
        if (selectedTextIndex >= 0 && selectedTextIndex < (int)texts.size()) {
            const auto& t = texts[selectedTextIndex];
            gc->SetFont(wxFontInfo(12).Family(wxFONTFAMILY_DEFAULT), *wxBLACK);

            double tw, th, descent, extlead;
            gc->GetTextExtent(t.second, &tw, &th, &descent, &extlead);

            const int pad = 3; // 选框内边距
            wxRect box(t.first.x - pad, t.first.y - pad, (int)tw + pad * 2, (int)th + pad * 2);

            gc->SetPen(wxPen(wxColour(0, 120, 215), 1, wxPENSTYLE_SOLID)); // Windows选中蓝
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->DrawRectangle(box.x, box.y, box.width, box.height);
        }

        // ⭐ 矢量门绘制
        for (int i = 0; i < (int)components.size(); ++i) {
            components[i]->SetSelected(i == selectedGateIndex);
            components[i]->drawSelf(memDC);
        }

        // 绘制节点状态（输入/输出0-1）
        DrawNodeStates(gc);

        // 十字线
        gc->SetPen(wxPen(wxColour(0, 0, 0), 1));
        gc->StrokeLine(mousePos.x, 0, mousePos.x, GetSize().y);
        gc->StrokeLine(0, mousePos.y, GetSize().x, mousePos.y);

        delete gc;
    }
    dc.Blit(0, 0, GetClientSize().GetWidth(), GetClientSize().GetHeight(), &memDC, 0, 0);
}

void DrawBoard::drawGrid(wxGraphicsContext* gc)
{
    wxSize size = GetClientSize();
    gc->SetPen(wxPen(wxColour(200, 200, 200), 1, wxPENSTYLE_DOT_DASH));
    int gridSize = GRID;
    for (int x = 0; x < size.GetWidth(); x += gridSize)  gc->StrokeLine(x, 0, x, size.GetHeight());
    for (int y = 0; y < size.GetHeight(); y += gridSize) gc->StrokeLine(0, y, size.GetWidth(), y);
}

void DrawBoard::OnButtonMove(wxMouseEvent& event)
{
    mousePos = event.GetPosition();
    if (st1) st1->SetLabel(wxString::Format("x: %d", event.GetX()));
    if (st2) st2->SetLabel(wxString::Format("y: %d", event.GetY()));

    if (isDrawing) currentEnd = mousePos;

    // 拖动组件（实时预览；真正的命令在 MouseUp 记录 from->to）
    if (m_isDragging && m_draggingIndex >= 0 && m_draggingIndex < (int)components.size()) {
        const wxPoint delta = mousePos - m_dragStartMouse;
        wxPoint target = m_dragStartCenter + delta;
        wxPoint snapped = SnapToStep(target);
        if (snapped != components[m_draggingIndex]->GetCenter()) {
            components[m_draggingIndex]->SetCenter(snapped);
            RerouteWiresForMovedComponent(m_draggingIndex, preMovePins);
            preMovePins = components[m_draggingIndex]->GetPins();
            Refresh(false);
        }
        return;
    }

    // 拖动文本（不吸附）
    if (m_isDraggingText && m_dragTextIndex >= 0 && m_dragTextIndex < (int)texts.size()) {
        const wxPoint delta = mousePos - m_dragTextStartMouse;
        wxPoint target = m_dragTextStartPos + delta; // 不使用 SnapToStep / SnapToGrid
        texts[m_dragTextIndex].first = target;
        Refresh(false);
        return;
    }

    Refresh(false);
}

void DrawBoard::OnLeftDown(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();

    // 点击起始节点切换电平（优先处理）
    if (m_sim) {
        ToggleStartNodeAt(pos);
    }


    // 插入模式：由外部设置 pSelectedGateName 决定要插哪个门
    if (pSelectedGateName && !pSelectedGateName->IsEmpty()) {
        AddGate(pos, *pSelectedGateName);   // 内部走命令
        Refresh(false);
        return;
    }

    // Arrow 工具：选择 + 准备拖动
    if (currentTool == ID_TOOL_ARROW) {
        int hit = HitTestGate(pos);
        if (hit >= 0) {
            selectedGateIndex = hit;
            m_draggingIndex = hit;
            m_isDragging = true;
            m_dragStartMouse = pos;
            m_dragStartCenter = components[hit]->GetCenter();
            preMovePins = components[hit]->GetPins();
            if (!HasCapture()) CaptureMouse();
            Refresh(false);

            m_selKind = SelKind::Gate;
            m_selId = selectedGateIndex;
            NotifySelectionChanged();
        }
        else {
            selectedGateIndex = -1;
            m_draggingIndex = -1;
            m_isDragging = false;

            // 先看连线
            int w = HitTestWire(pos);
            if (w >= 0) {
                selectedWireIndex = w;
                m_selKind = SelKind::Wire;
                m_selId = selectedWireIndex;

                // 取消文本选择/拖动
                selectedTextIndex = -1;
                m_isDraggingText = false;
                m_dragTextIndex = -1;
            }
            else {
                selectedWireIndex = -1;

                // ===== 新增：命中文本？ =====
                int t = HitTestText(pos);
                if (t >= 0) {
                    selectedTextIndex = t;

                    // 准备拖动文本
                    m_isDraggingText = true;
                    m_dragTextIndex = t;
                    m_dragTextStartMouse = pos;
                    m_dragTextStartPos = texts[t].first;

                    // 不改变 SelKind（避免影响你现有的属性面板逻辑）
                    m_selKind = SelKind::None;
                    m_selId = -1;
                }
                else {
                    // 均未命中
                    selectedTextIndex = -1;
                    m_isDraggingText = false;
                    m_dragTextIndex = -1;

                    m_selKind = SelKind::None;
                    m_selId = -1;
                }
            }
            Refresh(false);
            NotifySelectionChanged();
        }
    }

    // 画线
    if (currentTool == ID_TOOL_HAND) {
        // 开始画线：优先吸附到引脚
        wxPoint snap;
        if (FindNearestPin(pos, snap, nullptr)) lineStart = snap;
        else                                    lineStart = SnapToGrid(pos);
        isRouting = true;
        return;
    }

    // 文本
    if (currentTool == ID_TOOL_TEXT && isInsertingText) {
        wxTextEntryDialog dlg(this, "请输入要插入的文本：", "插入文本");
        if (dlg.ShowModal() == wxID_OK) {
            wxString input = dlg.GetValue();
            if (!input.IsEmpty()) {
                texts.emplace_back(pos, input);
                Refresh(false);
            }
        }
    }
}

void DrawBoard::OnLeftUp(wxMouseEvent& event)
{
    // 结束拖动 → 生成移动命令
    if (m_isDragging) {
        m_isDragging = false;
        int idx = m_draggingIndex;
        m_draggingIndex = -1;
        if (HasCapture()) ReleaseMouse();

        if (idx >= 0 && idx < (int)components.size()) {
            wxPoint from = m_dragStartCenter;
            wxPoint to = components[idx]->GetCenter();
            if (from != to && m_cmd) {
                m_cmd->Execute(std::make_unique<MoveGateCmd>(this, idx, from, to));
            }
        }
        preMovePins.clear();
        Refresh(false);
    }

    // 结束文本拖动（不进入撤销/重做）
    if (m_isDraggingText) {
        m_isDraggingText = false;
        m_dragTextIndex = -1;
        Refresh(false);
    }


    // 旧直线逻辑（保持兼容）
    if (isDrawingLine && isDrawing) {
        currentEnd = event.GetPosition();
        lines.emplace_back(std::make_pair(currentStart, currentEnd));
        isDrawing = false;
        Refresh(false);
    }

    // 结束布线 → 生成添加连线命令
    if (isRouting) {
        wxPoint pos = event.GetPosition();
        wxPoint endPt;
        if (!FindNearestPin(pos, endPt, nullptr)) endPt = SnapToGrid(pos);

        auto poly = MakeManhattan(lineStart, endPt);
        if (poly.size() >= 2 && !(poly.front() == poly.back())) {
            WireSnapshot w{ poly };
            if (m_cmd) {
                m_cmd->Execute(std::make_unique<AddWireCmd>(this, w));
                selectedWireIndex = (int)wires.size() - 1;
            }
            else {
                AddWire(w);
                selectedWireIndex = (int)wires.size() - 1;
            }
            m_selKind = SelKind::Wire;
            m_selId = selectedWireIndex;
            NotifySelectionChanged();
        }
        isRouting = false;
        Refresh(false);
        return;
    }
}

int DrawBoard::HitTestText(const wxPoint& pt) const
{
    // 从上往下（后绘制优先）
    // 注意：需要一个 gc/度量，这里临时创建 DC+GC
    wxBitmap tmpBmp(1, 1);
    wxMemoryDC mdc(tmpBmp);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(mdc));
    if (!gc) return -1;

    // 与绘制时一致的字体
    gc->SetFont(wxFontInfo(12).Family(wxFONTFAMILY_DEFAULT), *wxBLACK);

    for (int i = (int)texts.size() - 1; i >= 0; --i) {
        const auto& t = texts[i];
        double tw, th, descent, extlead;
        gc->GetTextExtent(t.second, &tw, &th, &descent, &extlead);
        wxRect box(t.first.x, t.first.y, (int)tw, (int)th);
        // 给点小容差，选中体验更好
        box.Inflate(3, 3);
        if (box.Contains(pt)) return i;
    }
    return -1;
}

void DrawBoard::DeleteSelectedText()
{
    if (selectedTextIndex >= 0 && selectedTextIndex < (int)texts.size()) {
        texts.erase(texts.begin() + selectedTextIndex);
        selectedTextIndex = -1;
        Refresh(false);
    }
}


// ============ 对外接口 ============
void DrawBoard::ClearTexts() { texts.clear(); Refresh(false); }
void DrawBoard::ClearPics() { lines.clear(); Refresh(false); }

void DrawBoard::AddGate(const wxPoint& center, const wxString& typeName)
{
    GateSnapshot snap{ NameToType(typeName), SnapToStep(center), 1.0 };
    if (m_cmd) {
        m_cmd->Execute(std::make_unique<AddGateCmd>(this, snap));
    }
    else {
        AddGateFromSnapshot(snap);
    }
    Refresh(false);
}

void DrawBoard::SelectGate(const wxPoint& pos)
{
    selectedGateIndex = HitTestGate(pos);
    Refresh(false);
}

void DrawBoard::DeleteSelectedGate()
{
    if (selectedGateIndex >= 0 && selectedGateIndex < (int)components.size()) {
        if (m_cmd) m_cmd->Execute(std::make_unique<DeleteGateCmd>(this, selectedGateIndex));
        else       DeleteGateByIndex(selectedGateIndex);

        selectedGateIndex = -1;
        m_selKind = SelKind::None;
        m_selId = -1;
        NotifySelectionChanged();
        Refresh(false);
    }
}

void DrawBoard::DeleteSelectedWire() {
    if (selectedWireIndex >= 0 && selectedWireIndex < (int)wires.size()) {
        if (m_cmd) m_cmd->Execute(std::make_unique<DeleteWireCmd>(this, selectedWireIndex));
        else       DeleteWireByIndex(selectedWireIndex);

        selectedWireIndex = -1;
        m_selKind = SelKind::None;
        m_selId = -1;
        NotifySelectionChanged();
        Refresh(false);
    }
}

void DrawBoard::DeleteSelection() {
    if (selectedGateIndex >= 0) { DeleteSelectedGate(); return; }
    if (selectedWireIndex >= 0) { DeleteSelectedWire(); return; }
    if (selectedTextIndex >= 0) { DeleteSelectedText(); return; }
    // 什么也没选中就不做事
}

int DrawBoard::HitTestGate(const wxPoint& pt) const
{
    for (int i = (int)components.size() - 1; i >= 0; --i) {
        if (components[i]->Isinside(pt)) return i;
    }
    return -1;
}

// ============ JSON ============
void DrawBoard::SaveToJson(const std::string& filename)
{
    Json::Value root;

    // 1) wires（折线）
    for (const auto& poly : wires) {
        if (poly.size() < 2) continue;
        Json::Value arr(Json::arrayValue);
        for (const auto& p : poly) {
            Json::Value node;
            node["x"] = p.x;
            node["y"] = p.y;
            arr.append(node);
        }
        root["wires"].append(arr);
    }

    // 2) 兼容旧直线
    if (!lines.empty()) {
        for (auto& line : lines) {
            Json::Value obj;
            obj["type"] = "Line";
            obj["x1"] = line.first.x;
            obj["y1"] = line.first.y;
            obj["x2"] = line.second.x;
            obj["y2"] = line.second.y;
            root["oldShapes"].append(obj);
        }
    }

    // 3) 文本
    for (auto& txt : texts) {
        Json::Value obj;
        obj["content"] = std::string(txt.second.ToUTF8());
        obj["x"] = txt.first.x;
        obj["y"] = txt.first.y;
        root["texts"].append(obj);
    }

    // 4) 门
    for (auto& c : components) {
        Json::Value obj;
        obj["name"] = TypeToName(c->m_type);
        wxPoint cen = c->GetCenter();
        obj["x"] = cen.x;
        obj["y"] = cen.y;
        obj["scale"] = c->scale;
        root["gates"].append(obj);
    }

    std::ofstream ofs(filename);
    Json::StreamWriterBuilder w;
    std::unique_ptr<Json::StreamWriter> writer(w.newStreamWriter());
    writer->write(root, &ofs);
}

void DrawBoard::LoadFromJson(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        wxMessageBox("无法打开文件", "错误", wxOK | wxICON_ERROR);
        return;
    }
    Json::Value root;
    ifs >> root;

    // 清空
    wires.clear(); lines.clear(); texts.clear(); components.clear();

    // 1) wires（折线）
    if (root.isMember("wires") && root["wires"].isArray()) {
        for (const auto& arr : root["wires"]) {
            if (!arr.isArray() || arr.size() < 2) continue;
            std::vector<wxPoint> poly;
            poly.reserve(arr.size());
            for (const auto& node : arr) {
                int x = node.get("x", 0).asInt();
                int y = node.get("y", 0).asInt();
                poly.emplace_back(x, y);
            }
            if (poly.size() >= 2) wires.push_back(std::move(poly));
        }
    }

    // 2) 兼容旧数据：shapes 直线
    if (root.isMember("shapes") && root["shapes"].isArray()) {
        for (const auto& obj : root["shapes"]) {
            if (!obj.isObject()) continue;
            if (obj.isMember("type") && std::string(obj["type"].asCString()) == "Line") {
                wxPoint p1(obj["x1"].asInt(), obj["y1"].asInt());
                wxPoint p2(obj["x2"].asInt(), obj["y2"].asInt());
                if (!(p1 == p2)) wires.push_back({ p1, p2 });
            }
        }
    }

    // 兼容 oldShapes
    if (root.isMember("oldShapes") && root["oldShapes"].isArray()) {
        for (const auto& obj : root["oldShapes"]) {
            if (!obj.isObject()) continue;
            if (obj.isMember("type") && std::string(obj["type"].asCString()) == "Line") {
                wxPoint p1(obj["x1"].asInt(), obj["y1"].asInt());
                wxPoint p2(obj["x2"].asInt(), obj["y2"].asInt());
                if (!(p1 == p2)) wires.push_back({ p1, p2 });
            }
        }
    }

    // 3) 文本
    if (root.isMember("texts") && root["texts"].isArray()) {
        for (auto& obj : root["texts"]) {
            wxPoint p(obj.get("x", 0).asInt(), obj.get("y", 0).asInt());
            wxString content = wxString::FromUTF8(obj.get("content", "").asCString());
            texts.push_back({ p, content });
        }
    }

    // 4) 门
    if (root.isMember("gates") && root["gates"].isArray()) {
        for (auto& obj : root["gates"]) {
            wxPoint center(obj.get("x", 0).asInt(), obj.get("y", 0).asInt());
            wxString name = wxString::FromUTF8(obj.get("name", "").asCString());
            auto comp = MakeComponent(NameToType(name), center);
            if (comp) {
                if (obj.isMember("scale")) comp->scale = obj["scale"].asDouble();
                comp->UpdateGeometry();
                components.push_back(std::move(comp));
            }
        }
    }

    Refresh(false);
}

// ============ 小工具 ============
std::array<wxPoint, 4> DrawBoard::GetGateAnchorPoints(const Component* comp) const
{
    std::array<wxPoint, 4> arr;
    for (int i = 0; i < 4; ++i) arr[i] = comp->m_BoundaryPoints[i];
    return arr;
}

ComponentType DrawBoard::NameToType(const wxString& s)
{
    wxString u = s.Upper().Trim(true).Trim(false);

    // 先匹配更具体的
    if (u.Contains("XNOR")) return XNORGATE;        // ★ 新增
    if (u.Contains("NAND")) return NANDGATE;
    if (u.Contains("NOR"))  return NORGATE;
    if (u.Contains("XOR"))  return XORGATE;

    // 基本门
    if (u.Contains("NOT"))  return NOTGATE;
    if (u.Contains("OR"))   return ORGATE;
    if (u.Contains("AND"))  return ANDGATE;

    // 译码器
    if (u.Contains("DECODER38") || u.Contains("DEC3TO8") || u.Contains("3-8"))
        return DECODER38;                              // ★ 新增
    if (u.Contains("DECODER24") || u.Contains("DEC2TO4") || u.Contains("2-4"))
        return DECODER24;                              // ★ 新增

    // 结点
    if (u.Contains("START_NODE") || u.Contains("STARTNODE") || u.Contains("起始")) return NODE_START;
    if (u.Contains("END_NODE") || u.Contains("ENDNODE") || u.Contains("终止")) return NODE_END;
    if (u.Contains("NODE"))  return NODE_BASIC;

    return ANDGATE; // 兜底
}


const char* DrawBoard::TypeToName(ComponentType t)
{
    switch (t) {
    case ANDGATE:     return "AND";
    case ORGATE:      return "OR";
    case NOTGATE:     return "NOT";
    case NANDGATE:    return "NAND";
    case NORGATE:     return "NOR";
    case XORGATE:     return "XOR";
    case XNORGATE:    return "XNOR";       // ★ 新增
    case DECODER24:   return "DECODER24";  // ★ 新增
    case DECODER38:   return "DECODER38";  // ★ 新增
    case NODE_BASIC:  return "NODE";
    case NODE_START:  return "START_NODE";
    case NODE_END:    return "END_NODE";
    default:          return "AND";
    }
}

std::unique_ptr<Component> DrawBoard::MakeComponent(ComponentType t, const wxPoint& center)
{
    switch (t) {
    case ANDGATE:     return std::make_unique<ANDGate>(center);
    case ORGATE:      return std::make_unique<ORGate>(center);
    case NOTGATE:     return std::make_unique<NOTGate>(center);
    case NANDGATE:    return std::make_unique<NANDGate>(center);
    case NORGATE:     return std::make_unique<NORGate>(center);
    case XORGATE:     return std::make_unique<XORGate>(center);
    case XNORGATE:    return std::make_unique<XNORGate>(center);   // ★ 新增
    case DECODER24:   return std::make_unique<Decoder24>(center);  // ★ 新增
    case DECODER38:   return std::make_unique<Decoder38>(center);  // ★ 新增
    case NODE_BASIC:  return std::make_unique<NodeDot>(center);
    case NODE_START:  return std::make_unique<StartNode>(center);
    case NODE_END:    return std::make_unique<EndNode>(center);
    default:          return nullptr;
    }
}

bool DrawBoard::FindNearestPin(const wxPoint& p, wxPoint& out, int* compIdx) const
{
    int bestIdx = -1;
    wxPoint bestPt;
    int bestD2 = SNAP_PIN_RADIUS * SNAP_PIN_RADIUS + 1;

    for (int i = 0; i < (int)components.size(); ++i) {
        auto pins = components[i]->GetPins();
        for (auto& pin : pins) {
            int dx = pin.x - p.x;
            int dy = pin.y - p.y;
            int d2 = dx * dx + dy * dy;
            if (d2 <= SNAP_PIN_RADIUS * SNAP_PIN_RADIUS && d2 < bestD2) {
                bestD2 = d2;
                bestPt = pin;
                bestIdx = i;
            }
        }
    }
    if (bestIdx >= 0) {
        out = bestPt;
        if (compIdx) *compIdx = bestIdx;
        return true;
    }
    return false;
}

std::vector<wxPoint> DrawBoard::MakeManhattan(const wxPoint& a, const wxPoint& b) const
{
    if (a.x == b.x || a.y == b.y) return { a, b }; // 共线
    wxPoint bend(b.x, a.y);                         // 先水平后垂直
    return { a, bend, b };
}

wxPoint DrawBoard::SnapToGrid(const wxPoint& p) const
{
    auto roundTo = [](int v, int g) { int r = (v + g / 2) / g; return r * g; };
    return wxPoint(roundTo(p.x, GRID), roundTo(p.y, GRID));
}

wxPoint DrawBoard::SnapToStep(const wxPoint& p) const
{
    auto roundTo = [](int v, int step) { int r = (v + step / 2) / step; return r * step; };
    return wxPoint(roundTo(p.x, STEP), roundTo(p.y, STEP));
}

void DrawBoard::RerouteWiresForMovedComponent(int compIdx, const std::vector<wxPoint>& prevPins)
{
    if (compIdx < 0 || compIdx >= (int)components.size()) return;

    // 新/旧引脚表
    const std::vector<wxPoint> newPins = components[compIdx]->GetPins();
    if (prevPins.empty() || newPins.empty()) return;

    auto nearlyEqual = [](const wxPoint& a, const wxPoint& b) {
        // 容忍 1 个像素以内的差异，避免栅格/吸附导致的非严格相等
        return (std::abs(a.x - b.x) <= 1) && (std::abs(a.y - b.y) <= 1);
        };

    auto findPinByPoint = [&](const wxPoint& p)->int {
        for (int i = 0; i < (int)prevPins.size(); ++i) {
            if (nearlyEqual(prevPins[i], p)) return i;
        }
        return -1;
        };

    for (auto& poly : wires) {
        if (poly.size() < 2) continue;

        wxPoint start = poly.front();
        wxPoint end = poly.back();
        bool changed = false;

        int idxS = findPinByPoint(start);
        int idxE = findPinByPoint(end);

        if (idxS >= 0 && idxS < (int)newPins.size()) { start = newPins[idxS]; changed = true; }
        if (idxE >= 0 && idxE < (int)newPins.size()) { end = newPins[idxE]; changed = true; }

        if (changed) {
            poly = MakeManhattan(start, end);
        }
    }
}

int DrawBoard::Dist2_PointToSeg(const wxPoint& p, const wxPoint& a, const wxPoint& b)
{
    const int vx = b.x - a.x, vy = b.y - a.y;
    const int wx = p.x - a.x, wy = p.y - a.y;
    const int vv = vx * vx + vy * vy;
    if (vv == 0) { int dx = p.x - a.x, dy = p.y - a.y; return dx * dx + dy * dy; }
    const double t = (wx * vx + wy * vy) / (double)vv;
    double clamped = t < 0 ? 0 : (t > 1 ? 1 : t);
    const double projx = a.x + clamped * vx;
    const double projy = a.y + clamped * vy;
    const double dx = p.x - projx, dy = p.y - projy;
    return int(dx * dx + dy * dy);
}

int DrawBoard::HitTestWire(const wxPoint& pt) const
{
    const int th2 = LINE_HIT_PX * LINE_HIT_PX;
    for (int i = (int)wires.size() - 1; i >= 0; --i) {
        const auto& poly = wires[i];
        for (size_t k = 1; k < poly.size(); ++k) {
            if (Dist2_PointToSeg(pt, poly[k - 1], poly[k]) <= th2) return i;
        }
    }
    return -1;
}

// ===== 统一选择接口：事件 & 外部可控选择 =====
void DrawBoard::NotifySelectionChanged() {
    wxCommandEvent evt(EVT_SELECTION_CHANGED);
    evt.SetInt(static_cast<int>(m_selKind)); // SelKind
    evt.SetExtraLong(m_selId);               // 选中索引
    wxPostEvent(GetParent(), evt);           // 发给父窗口（如 cMain）
}

void DrawBoard::SelectNone() {
    selectedGateIndex = -1;
    selectedWireIndex = -1;
    m_selKind = SelKind::None;
    m_selId = -1;
    Refresh(false);
    NotifySelectionChanged();
}

void DrawBoard::SelectGateByIndex(int idx) {
    if (idx >= 0 && idx < (int)components.size()) {
        selectedGateIndex = idx;
        selectedWireIndex = -1;
        m_selKind = SelKind::Gate;
        m_selId = idx;
        Refresh(false);
        NotifySelectionChanged();
    }
}

void DrawBoard::SelectWireByIndex(int idx) {
    if (idx >= 0 && idx < (int)wires.size()) {
        selectedWireIndex = idx;
        selectedGateIndex = -1;
        m_selKind = SelKind::Wire;
        m_selId = idx;
        Refresh(false);
        NotifySelectionChanged();
    }
}

// ===== 命令层 API =====
long DrawBoard::AddGateFromSnapshot(const GateSnapshot& s) {
    auto comp = MakeComponent(s.type, SnapToStep(s.center));
    if (!comp) return -1;
    comp->scale = s.scale;
    comp->UpdateGeometry();
    components.push_back(std::move(comp));
    Refresh(false);
    return (long)components.size() - 1;
}

std::optional<GateSnapshot> DrawBoard::ExportGateByIndex(long id) const {
    if (id < 0 || id >= (long)components.size()) return std::nullopt;
    const auto& c = components[id];
    GateSnapshot s;
    s.type = c->m_type;
    s.center = c->GetCenter();
    s.scale = c->scale;
    return s;
}

void DrawBoard::DeleteGateByIndex(long id) {
    if (id < 0 || id >= (long)components.size()) return;
    components.erase(components.begin() + id);
    if (selectedGateIndex == id) selectedGateIndex = -1;
    Refresh(false);
}

void DrawBoard::MoveGateTo(long id, const wxPoint& pos) {
    if (id < 0 || id >= (long)components.size()) return;
    // Capture pins BEFORE move
    std::vector<wxPoint> prevPins = components[id]->GetPins();
    components[id]->SetCenter(SnapToStep(pos));
    // After move, reroute wires using prevPins -> new pins mapping
    RerouteWiresForMovedComponent((int)id, prevPins);
    Refresh(false);
}

long DrawBoard::AddWire(const WireSnapshot& w) {
    if (w.poly.size() < 2) return -1;
    wires.push_back(w.poly);
    Refresh(false);
    return (long)wires.size() - 1;
}

std::optional<WireSnapshot> DrawBoard::ExportWireByIndex(long id) const {
    if (id < 0 || id >= (long)wires.size()) return std::nullopt;
    return WireSnapshot{ wires[id] };
}

void DrawBoard::DeleteWireByIndex(long id) {
    if (id < 0 || id >= (long)wires.size()) return;
    wires.erase(wires.begin() + id);
    if (selectedWireIndex == id) selectedWireIndex = -1;
    Refresh(false);
}

// ======= 缩放实现（模型等比例变换） =======
void DrawBoard::ZoomBy(double factor, const wxPoint& anchorDevicePt)
{
    if (factor <= 0.0 || factor == 1.0) return;

    auto Z = [&](int v, int a) -> int {
        double r = a + (v - a) * factor;   // p' = a + (p - a) * s
        return (int)std::lround(r);
        };

    // 1) 连线点（折线）
    for (auto& poly : wires) {
        for (auto& p : poly) {
            p.x = Z(p.x, anchorDevicePt.x);
            p.y = Z(p.y, anchorDevicePt.y);
        }
    }

    // 2) 文本位置
    for (auto& tp : texts) {
        tp.first.x = Z(tp.first.x, anchorDevicePt.x);
        tp.first.y = Z(tp.first.y, anchorDevicePt.y);
    }

    // 3) 元件：中心与尺寸（scale）
    for (auto& up : components) {
        if (!up) continue;
        Component* c = up.get();
        wxPoint cen = c->GetCenter();
        cen.x = Z(cen.x, anchorDevicePt.x);
        cen.y = Z(cen.y, anchorDevicePt.y);
        c->SetCenter(cen);

        c->scale *= factor;      // 让门图标随页面一起变大/变小
        c->UpdateGeometry();     // 引脚等几何量重算，线跟随逻辑可复用
    }

    // 4) 兼容旧的直线容器（如仍在使用）
    for (auto& seg : lines) {
        seg.first.x = Z(seg.first.x, anchorDevicePt.x);
        seg.first.y = Z(seg.first.y, anchorDevicePt.y);
        seg.second.x = Z(seg.second.x, anchorDevicePt.x);
        seg.second.y = Z(seg.second.y, anchorDevicePt.y);
    }

    Refresh(false);
    Update();
}

void DrawBoard::ZoomInCenter()
{
    const wxSize cs = GetClientSize();
    const wxPoint anchor(cs.GetWidth() / 2, cs.GetHeight() / 2);
    ZoomBy(1.1, anchor);         // 每次放大 10%
}

void DrawBoard::ZoomOutCenter()
{
    const wxSize cs = GetClientSize();
    const wxPoint anchor(cs.GetWidth() / 2, cs.GetHeight() / 2);
    ZoomBy(1.0 / 1.1, anchor);   // 每次缩小 10%
}

// 把当前画布导出为 BookShelf .nodes + .nets
bool DrawBoard::ExportAsBookShelf(const std::string& projectName,
    const std::filesystem::path& outDir)
{
    // 1) 生成 nodes（每个元件一个 node）
    std::vector<Node> nodes;
    nodes.reserve(components.size());

    // 建立 component 索引 -> nodeName 的映射
    std::vector<std::string> comp2name(components.size());
    // 用类型计数做唯一命名
    std::map<ComponentType, int> typeCount;

    for (size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        const char* base = TypeToName(c->m_type);               // e.g. "AND"/"NODE"/"START_NODE"... :contentReference[oaicite:4]{index=4}
        int id = ++typeCount[c->m_type];
        std::string name = MakeNodeName(base, id);

        auto [w, h] = AABBwh(c->m_BoundaryPoints);               // 用边界盒宽高做尺寸 :contentReference[oaicite:5]{index=5}
        Node n;
        n.name = name;
        n.width = IsTerminal(c->m_type) ? 0.0 : std::max(1.0, w);
        n.height = IsTerminal(c->m_type) ? 0.0 : std::max(1.0, h);
        n.terminal = IsTerminal(c->m_type);

        nodes.push_back(n);
        comp2name[i] = name;
    }

    // 2) 把所有元件的 pin 摊平做“坐标 -> (compIdx,pinIdx)”索引
    struct PinKey { int x, y; };
    struct PinInfo { int compIdx; int pinIdx; wxPoint pt; };
    struct KeyHash { size_t operator()(const PinKey& k) const { return (size_t(k.x) << 32) ^ size_t(unsigned(k.y)); } };
    struct KeyEq { bool operator()(const PinKey& a, const PinKey& b) const { return a.x == b.x && a.y == b.y; } };

    std::unordered_map<PinKey, std::vector<PinInfo>, KeyHash, KeyEq> pinAt;

    std::vector<std::vector<wxPoint>> allCompPins(components.size());
    for (size_t ci = 0; ci < components.size(); ++ci) {
        allCompPins[ci] = components[ci]->GetPins();            // 每个元件的所有引脚坐标（画布坐标）:contentReference[oaicite:6]{index=6}
        for (int pi = 0; pi < (int)allCompPins[ci].size(); ++pi) {
            wxPoint p = allCompPins[ci][pi];
            PinKey k{ p.x, p.y };
            pinAt[k].push_back(PinInfo{ (int)ci, pi, p });
        }
    }

    // 3) 解析 wires 端点 -> 找到对应的 pin；把“连到一起的 pin”合并成一个网
    //    我们把“pin”作为图节点；每条 wire 把两个端点 pin 连起来；用并查集合并连通分量。
    struct PID { int compIdx; int pinIdx; };
    auto pidLess = [](const PID& a, const PID& b) {
        if (a.compIdx != b.compIdx) return a.compIdx < b.compIdx;
        return a.pinIdx < b.pinIdx;
        };

    // 为所有命中的 pin 分配一个递增 id
    std::map<PID, int, decltype(pidLess)> pid2id(pidLess);
    std::vector<PID> id2pid;

    auto getOrAddPid = [&](int ci, int pi)->int {
        PID k{ ci, pi };
        auto it = pid2id.find(k);
        if (it != pid2id.end()) return it->second;
        int id = (int)id2pid.size();
        id2pid.push_back(k);
        pid2id.emplace(k, id);
        return id;
        };

    // 并查集
    std::vector<int> uf; // 按需增长
    auto ufFind = [&](auto self, int x)->int {
        if (uf[x] == x) return x;
        uf[x] = self(self, uf[x]);
        return uf[x];
        };
    auto ufUnion = [&](int a, int b) {
        int ra = ufFind(ufFind, a), rb = ufFind(ufFind, b);
        if (ra != rb) uf[rb] = ra;
        };

    auto ensureUf = [&](int needSize) {
        int old = (int)uf.size();
        if (needSize > old) {
            uf.resize(needSize);
            for (int i = old; i < needSize; ++i) uf[i] = i;
        }
        };

    auto findPinByPoint = [&](const wxPoint& p)->std::vector<PinInfo> {
        // 先尝试完全相等，再尝试 1px 容忍
        PinKey k{ p.x, p.y };
        auto it = pinAt.find(k);
        if (it != pinAt.end()) return it->second;
        // 容忍 1px（匹配你移动重布线时的容忍逻辑）:contentReference[oaicite:7]{index=7}
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                PinKey k2{ p.x + dx, p.y + dy };
                auto it2 = pinAt.find(k2);
                if (it2 != pinAt.end()) return it2->second;
            }
        return {};
        };

    for (const auto& poly : wires) {
        if (poly.size() < 2) continue;
        wxPoint a = poly.front();
        wxPoint b = poly.back();

        auto pinsA = findPinByPoint(a);
        auto pinsB = findPinByPoint(b);
        if (pinsA.empty() || pinsB.empty()) continue; // 端点没吸到 pin 就忽略该线段

        // 允许端点上多 pin（少见），做笛卡尔连接
        for (const auto& pa : pinsA) {
            for (const auto& pb : pinsB) {
                int idA = getOrAddPid(pa.compIdx, pa.pinIdx);
                int idB = getOrAddPid(pb.compIdx, pb.pinIdx);
                ensureUf(std::max(idA, idB) + 1);
                ufUnion(idA, idB);
            }
        }
    }

    // 4) 把每个连通分量变成一个 Net，填入 pins（计算相对偏移）
    std::unordered_map<int, std::vector<int>> compSets; // rootId -> [pidId...]
    for (int id = 0; id < (int)id2pid.size(); ++id) {
        int root = ufFind(ufFind, id);
        compSets[root].push_back(id);
    }

    std::vector<Net> nets;
    int autoNetIdx = 0;

    for (auto& kv : compSets) {
        const auto& pidList = kv.second;
        if (pidList.size() < 2) continue; // 单引脚不构成网

        Net net;
        net.name = "net_" + std::to_string(autoNetIdx++);

        for (int pid : pidList) {
            const auto [ci, pi] = id2pid[pid];
            const auto& comp = components[ci];
            const wxPoint ccen = comp->GetCenter();
            const wxPoint ppt = allCompPins[ci][pi];

            Pin p;
            p.cellName = comp2name[ci];
            p.pinName = PinNameByIndex(pi);
            p.dx = double(ppt.x - ccen.x);
            p.dy = double(ppt.y - ccen.y);
            net.pins.push_back(std::move(p));
        }
        nets.push_back(std::move(net));
    }

    // 5) 调用通用导出器
    ExportOptions opt;
    opt.outDir = outDir;
    opt.floatPrecision = 6;

    auto [nodesPath, netsPath] =
        bookshelf::ExportBookShelf(projectName, nodes, nets, opt);

    // 你也可以弹一个提示框
    // wxMessageBox(wxString::Format("已导出：\n%s\n%s",
    //     nodesPath.wstring(), netsPath.wstring()), "Export", wxOK | wxICON_INFORMATION);

    return true;
}

// —— 简单栅格排布（无 .pl 时的兜底）：把 N 个节点铺成 R 行 C 列 —— //
static std::vector<wxPoint> AutoGridPlace(int N, const wxSize& canvasSize,
    int cellW = 140, int cellH = 100, int margin = 60)
{
    std::vector<wxPoint> centers;
    if (N <= 0) return centers;
    int cols = std::max(1, (int)std::sqrt((double)N));
    int rows = (N + cols - 1) / cols;

    int x0 = margin, y0 = margin;
    for (int i = 0; i < N; ++i) {
        int r = i / cols, c = i % cols;
        int x = x0 + c * cellW;
        int y = y0 + r * cellH;
        centers.emplace_back(x, y);
    }
    return centers;
}

// —— 根据名字前缀推断类型（AND_1 => AND），走你的 NameToType 再 MakeComponent —— //
static ComponentType GuessTypeFromNodeName(DrawBoard* board, const std::string& nodeName)
{
    // 截到 '_' 前（如果没有 '_' 就用全名）
    std::string base = nodeName;
    size_t k = nodeName.find('_');
    if (k != std::string::npos) base = nodeName.substr(0, k);

    return board->NameToType(wxString::FromUTF8(base.c_str())); // 你的 NameToType()。:contentReference[oaicite:3]{index=3}
}

bool DrawBoard::ImportBookShelf(const std::filesystem::path& nodesPath,
    const std::filesystem::path& netsPath)
{
    BSDesign d = ParseBookShelf(nodesPath, netsPath);

    // 1) 清空当前画布
    wires.clear(); lines.clear(); texts.clear(); components.clear();

    // 2) 生成组件：类型来自名称前缀（AND/NOR/DECODER24/NODE/START_NODE/...）
    const int N = (int)d.nodes.size();
    auto centers = AutoGridPlace(N, GetClientSize()); // 简单自动排布
    std::unordered_map<std::string, int> name2idx;

    for (int i = 0; i < N; ++i) {
        const auto& n = d.nodes[i];
        ComponentType t = GuessTypeFromNodeName(this, n.name);
        // 终端（I/O）优先映射到 START_NODE/END_NODE/NODE：这里如果名字不含提示，就保留 NODE
        if (n.terminal) t = NODE_BASIC; // 也可以换成 START_NODE/END_NODE，看你的语义。:contentReference[oaicite:4]{index=4}

        auto up = MakeComponent(t, centers[i]); // 直接创建你的组件实例。:contentReference[oaicite:5]{index=5}
        if (!up) continue;
        up->UpdateGeometry();
        name2idx[n.name] = (int)components.size();
        components.push_back(std::move(up));
    }

    // 3) 按 .nets 画线
    // 规则：找 O（输出）做源；若没有 O，取第一个 pin 作源；其余视作汇，分别连一条曼哈顿折线
    for (const auto& net : d.nets) {
        if (net.pins.size() < 2) continue;

        auto getAbs = [&](const std::string& cell, double dx, double dy)->wxPoint {
            auto it = name2idx.find(cell);
            if (it == name2idx.end()) return wxPoint(0, 0);
            int idx = it->second;
            wxPoint c = components[idx]->GetCenter();
            return wxPoint((int)std::lround(c.x + dx), (int)std::lround(c.y + dy));
            };

        // 找驱动端
        int src = -1;
        for (int i = 0; i < (int)net.pins.size(); ++i) {
            const auto& p = net.pins[i];
            // 以 pinName 中的 'O' 作为驱动判断（常见写法）；否则留到后面兜底。:contentReference[oaicite:6]{index=6}
            if (!p.pinName.empty() && (p.pinName[0] == 'O' || p.pinName[0] == 'o')) { src = i; break; }
        }
        if (src < 0) src = 0;

        wxPoint s = getAbs(net.pins[src].cellName, net.pins[src].dx, net.pins[src].dy);
        for (int i = 0; i < (int)net.pins.size(); ++i) {
            if (i == src) continue;
            wxPoint t = getAbs(net.pins[i].cellName, net.pins[i].dx, net.pins[i].dy);
            auto poly = MakeManhattan(s, t);          // 你的 L 型走线工具。:contentReference[oaicite:7]{index=7}
            if (poly.size() >= 2 && !(poly.front() == poly.back())) {
                AddWire({ poly });                    // 用画布的“添加线”API。:contentReference[oaicite:8]{index=8}
            }
        }
    }

    Refresh(false);
    return true;
}

void DrawBoard::SimStart() {
    if (!m_sim) return;
    m_sim->BuildNetlist();   // 根据当前图构网
    m_simulating = true;
    m_sim->Run();
    // 20ms 一步（50Hz），可按需调整
    m_simTimer->Start(20);
    Refresh(false);
}

void DrawBoard::SimStop() {
    if (!m_sim) return;
    m_sim->Stop();
    m_simulating = false;
    m_simTimer->Stop();
    Refresh(false);
}

void DrawBoard::SimStep() {
    if (!m_sim) return;
    // 单步不启动定时器
    m_sim->BuildNetlist();
    m_sim->Step();
}

void DrawBoard::OnTimer(wxTimerEvent& e) {
    if (m_sim && m_simulating) {
        // 每 tick 单步推进
        m_sim->Step();
    }
}

bool DrawBoard::IsWireHighForPaint(int wireIndex) const {
    return (m_sim && m_simulating) ? m_sim->IsWireHigh(wireIndex) : false;
}

// ========== 点击起始节点切换电平 ==========
void DrawBoard::ToggleStartNodeAt(const wxPoint& pos)
{
    if (!m_sim) return;
    int hit = HitTestGate(pos);
    if (hit < 0 || hit >= (int)components.size()) return;

    auto* c = components[hit].get();
    if (c->m_type != ComponentType::NODE_START) return;

    bool oldVal = m_sim->GetStartNodeValue(hit);
    m_sim->SetStartNodeValue(hit, !oldVal);
    Refresh(false);
}

void DrawBoard::DrawNodeStates(wxGraphicsContext* gc)
{
    // 不再绘制任何节点状态（完全交给连线颜色反映）
    wxUnusedVar(gc);
}


