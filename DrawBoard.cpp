#include "DrawBoard.h"
#include <fstream>
#include "SelectionEvents.h"
#include "EditCommands.h"

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
}

DrawBoard::~DrawBoard() {}

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
        gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
        for (const auto& poly : wires) {
            for (size_t i = 1; i < poly.size(); ++i) {
                gc->StrokeLine(poly[i - 1].x, poly[i - 1].y,
                    poly[i].x, poly[i].y);
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
        obj["content"] = std::string(txt.second.mb_str());
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
    if (u.Contains("NAND")) return NANDGATE;
    if (u.Contains("NOR"))  return NORGATE;
    if (u.Contains("XOR"))  return XORGATE;

    // 基本门
    if (u.Contains("NOT"))  return NOTGATE;
    if (u.Contains("OR"))   return ORGATE;
    if (u.Contains("AND"))  return ANDGATE;

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