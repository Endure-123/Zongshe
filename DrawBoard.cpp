#include "DrawBoard.h"
#include <fstream>
#include "SelectionEvents.h"  // ★ 新增：选择事件头

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

    m_isDragging = false;
    m_draggingIndex = -1;

    isDrawingLine = false;
    isInsertingText = false;
    isDrawing = false;

    // 统一选择状态初始化
    m_selKind = SelKind::None;
    m_selId = -1;
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

        // ====== A) 已保存的线（从 lines → wires 的折线绘制）======
        gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
        for (const auto& poly : wires) {                 // ★ 用 wires
            for (size_t i = 1; i < poly.size(); ++i) {
                gc->StrokeLine(poly[i - 1].x, poly[i - 1].y,
                    poly[i].x, poly[i].y);
            }
        }

        // ====== A1) 如果有选中的连线，绘制两端两点定位 ======
        if (selectedWireIndex >= 0 && selectedWireIndex < (int)wires.size()) {
            const auto& poly = wires[selectedWireIndex];
            if (poly.size() >= 2) {
                const wxPoint& p0 = poly.front();
                const wxPoint& p1 = poly.back();

                gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
                gc->SetBrush(*wxWHITE_BRUSH);
                const int r = 5; // 半径
                gc->DrawEllipse(p0.x - r, p0.y - r, 2 * r, 2 * r);
                gc->DrawEllipse(p1.x - r, p1.y - r, 2 * r, 2 * r);
            }
        }

        // ====== B) 临时预览线（曼哈顿折线 + 吸附）======
        if (isRouting) {                                 // ★ 用 isRouting
            wxPoint hover = mousePos;
            wxPoint snapEnd;
            if (FindNearestPin(mousePos, snapEnd, nullptr)) {
                hover = snapEnd;                        // 终点吸附到最近引脚
            }
            else {
                hover = SnapToGrid(mousePos);           // 可选：吸到网格
            }

            auto preview = MakeManhattan(lineStart, hover); // 生成 L 形折线
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

        // ⭐ 矢量门绘制
        for (int i = 0; i < (int)components.size(); ++i) {
            components[i]->SetSelected(i == selectedGateIndex); // 让门自行绘制四点定位
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
    int gridSize = 20;
    for (int x = 0; x < size.GetWidth(); x += gridSize)  gc->StrokeLine(x, 0, x, size.GetHeight());
    for (int y = 0; y < size.GetHeight(); y += gridSize) gc->StrokeLine(0, y, size.GetWidth(), y);
}

void DrawBoard::OnButtonMove(wxMouseEvent& event)
{
    mousePos = event.GetPosition();
    st1->SetLabel(wxString::Format("x: %d", event.GetX()));
    st2->SetLabel(wxString::Format("y: %d", event.GetY()));

    if (isDrawing) currentEnd = mousePos;

    // 拖动组件
    if (m_isDragging && m_draggingIndex >= 0 && m_draggingIndex < (int)components.size()) {
        const wxPoint delta = mousePos - m_dragStartMouse;
        wxPoint target = m_dragStartCenter + delta;
        wxPoint snapped = SnapToStep(target);                       // 半格吸附
        if (snapped != components[m_draggingIndex]->GetCenter()) {
            components[m_draggingIndex]->SetCenter(snapped);
            RerouteWiresForMovedComponent(m_draggingIndex);         // 线端点跟随并重算
            Refresh(false);
        }
        return;

    }
    Refresh();
}

void DrawBoard::OnLeftDown(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();

    // 插入模式：由外部设置 pSelectedGateName（如按钮）决定要插哪个门
    if (pSelectedGateName && !pSelectedGateName->IsEmpty()) {
        AddGate(pos, *pSelectedGateName);
        Refresh();
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
            // 记录移动前引脚坐标
            preMovePins = components[hit]->GetPins();
            if (!HasCapture()) CaptureMouse();
            Refresh();

            // ★ 选中 Gate 并通知
            m_selKind = SelKind::Gate;
            m_selId = selectedGateIndex;
            NotifySelectionChanged();
        }
        else {
            selectedGateIndex = -1;
            m_draggingIndex = -1;
            m_isDragging = false;

            // ★ 试图命中线条
            int w = HitTestWire(pos);
            if (w >= 0) {
                selectedWireIndex = w;
                m_selKind = SelKind::Wire;
                m_selId = selectedWireIndex;
            }
            else {
                selectedWireIndex = -1;
                m_selKind = SelKind::None;
                m_selId = -1;
            }
            Refresh();
            NotifySelectionChanged();
        }
    }

    // 画线
    if (currentTool == ID_TOOL_HAND) {
        // 开始画线
        wxPoint pos = event.GetPosition();

        // 1) 优先吸附到引脚
        wxPoint snap;
        if (FindNearestPin(pos, snap, nullptr)) {
            lineStart = snap;
        }
        else {
            // 2) 吸附网格（可选）
            lineStart = SnapToGrid(pos);
        }
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
                Refresh();
            }
        }
    }
}

void DrawBoard::OnLeftUp(wxMouseEvent& event)
{
    if (m_isDragging) {
        m_isDragging = false;
        m_draggingIndex = -1;
        if (HasCapture()) ReleaseMouse();
        preMovePins.clear();
        Refresh();
    }

    if (isDrawingLine && isDrawing) {
        currentEnd = event.GetPosition();
        lines.emplace_back(std::make_pair(currentStart, currentEnd));
        isDrawing = false;
        Refresh();
    }

    if (isRouting) {
        wxPoint pos = event.GetPosition();
        wxPoint endPt;
        if (!FindNearestPin(pos, endPt, nullptr)) {
            endPt = SnapToGrid(pos); // 可选
        }

        auto poly = MakeManhattan(lineStart, endPt);

        // 避免“起点=终点”或重复点导致 0 长度线段
        if (poly.size() >= 2 && !(poly.front() == poly.back())) {
            wires.push_back(std::move(poly));
        }
        isRouting = false;
        Refresh();

        // ★ 画完线后默认选中新线并通知
        if (!wires.empty()) {
            selectedWireIndex = (int)wires.size() - 1;
            m_selKind = SelKind::Wire;
            m_selId = selectedWireIndex;
            NotifySelectionChanged();
        }
        return;
    }
}

// ============ 对外接口 ============
void DrawBoard::ClearTexts() { texts.clear(); Refresh(); }
void DrawBoard::ClearPics() { lines.clear(); Refresh(); }

void DrawBoard::AddGate(const wxPoint& center, const wxString& typeName)
{
    ComponentType t = NameToType(typeName);
    auto comp = MakeComponent(t, SnapToStep(center));
    if (comp) {
        components.push_back(std::move(comp));
        Refresh();
    }
    else {
        wxMessageBox("未知的元件类型：" + typeName, "错误", wxOK | wxICON_ERROR);
    }
}

void DrawBoard::SelectGate(const wxPoint& pos)
{
    selectedGateIndex = HitTestGate(pos);
    Refresh();
}

void DrawBoard::DeleteSelectedGate()
{
    if (selectedGateIndex >= 0 && selectedGateIndex < (int)components.size()) {
        components.erase(components.begin() + selectedGateIndex);
        selectedGateIndex = -1;
        Refresh();

        // ★ 清空统一选择并通知
        m_selKind = SelKind::None;
        m_selId = -1;
        NotifySelectionChanged();
    }
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

    // ====== 1) 连线（折线）：wires ======
    // 格式： "wires": [ [ {"x":..,"y":..}, {"x":..,"y":..}, ... ],  ... ]
    for (const auto& poly : wires) {
        if (poly.size() < 2) continue; // 忽略无效折线
        Json::Value arr(Json::arrayValue);
        for (const auto& p : poly) {
            Json::Value node;
            node["x"] = p.x;
            node["y"] = p.y;
            arr.append(node);
        }
        root["wires"].append(arr);
    }

    // ====== 2) 兼容旧直线（可选）：如仍在用 lines，则也序列化到 oldShapes，便于调试/过渡 ======
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

    // ====== 3) 文本 ======
    for (auto& txt : texts) {
        Json::Value obj;
        obj["content"] = std::string(txt.second.mb_str());
        obj["x"] = txt.first.x;
        obj["y"] = txt.first.y;
        root["texts"].append(obj);
    }

    // ====== 4) 门元件 ======
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
    wires.clear();
    lines.clear();
    texts.clear();
    components.clear();

    // ====== 1) 先尝试读取新格式：wires（折线）======
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

    // ====== 2) 兼容旧数据：shapes（x1,y1,x2,y2 直线）======
    // 若文件中仍存在旧的 shapes，就把它们转成两点的折线追加到 wires
    if (root.isMember("shapes") && root["shapes"].isArray()) {
        for (const auto& obj : root["shapes"]) {
            // 只认旧的 "Line" 类型
            if (!obj.isObject()) continue;
            if (obj.isMember("type") && std::string(obj["type"].asCString()) == "Line") {
                wxPoint p1(obj["x1"].asInt(), obj["y1"].asInt());
                wxPoint p2(obj["x2"].asInt(), obj["y2"].asInt());
                if (!(p1 == p2)) {
                    wires.push_back({ p1, p2 });
                }
            }
        }
    }

    // （可选）兼容我们在 SaveToJson 中额外写的 oldShapes
    if (root.isMember("oldShapes") && root["oldShapes"].isArray()) {
        for (const auto& obj : root["oldShapes"]) {
            if (!obj.isObject()) continue;
            if (obj.isMember("type") && std::string(obj["type"].asCString()) == "Line") {
                wxPoint p1(obj["x1"].asInt(), obj["y1"].asInt());
                wxPoint p2(obj["x2"].asInt(), obj["y2"].asInt());
                if (!(p1 == p2)) {
                    wires.push_back({ p1, p2 });
                }
            }
        }
    }

    // ====== 3) 文本 ======
    if (root.isMember("texts") && root["texts"].isArray()) {
        for (auto& obj : root["texts"]) {
            wxPoint p(obj.get("x", 0).asInt(), obj.get("y", 0).asInt());
            wxString content = wxString::FromUTF8(obj.get("content", "").asCString());
            texts.push_back({ p, content });
        }
    }

    // ====== 4) 门 ======
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

    Refresh();
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

    // 先匹配更具体的（避免 "NAND" 被 "AND" 抢先匹配）
    if (u.Contains("NAND")) return NANDGATE;
    if (u.Contains("NOR"))  return NORGATE;
    if (u.Contains("XOR"))  return XORGATE;

    // 再匹配基本门
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
    case ANDGATE:  return "AND";
    case ORGATE:   return "OR";
    case NOTGATE:  return "NOT";
    case NANDGATE: return "NAND";
    case NORGATE:  return "NOR";
    case XORGATE:  return "XOR";
    case NODE_BASIC: return "NODE";
    case NODE_START: return "START_NODE";
    case NODE_END:   return "END_NODE";
    default:       return "AND";
    }
}

std::unique_ptr<Component> DrawBoard::MakeComponent(ComponentType t, const wxPoint& center)
{
    switch (t) {
    case ANDGATE:  return std::make_unique<ANDGate>(center);
    case ORGATE:   return std::make_unique<ORGate>(center);
    case NOTGATE:  return std::make_unique<NOTGate>(center);
    case NANDGATE: return std::make_unique<NANDGate>(center);
    case NORGATE:  return std::make_unique<NORGate>(center);
    case XORGATE:  return std::make_unique<XORGate>(center);
    case NODE_BASIC: return std::make_unique<NodeDot>(center);
    case NODE_START: return std::make_unique<StartNode>(center);
    case NODE_END:   return std::make_unique<EndNode>(center);
    default:       return nullptr;
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
    // 共线：水平或垂直，直接一段
    if (a.x == b.x || a.y == b.y) {
        return { a, b };
    }

    // 方案 A：先水平后垂直（a.x → b.x, a.y → b.y）
    // 拐点1 = (b.x, a.y)
    wxPoint bend(b.x, a.y);
    return { a, bend, b };

    // 若想优先垂直后水平，就改成：
    // wxPoint bend(a.x, b.y);
    // return { a, bend, b };
}

wxPoint DrawBoard::SnapToGrid(const wxPoint& p) const
{
    auto roundTo = [](int v, int g) {
        int r = (v + g / 2) / g;
        return r * g;
        };
    return wxPoint(roundTo(p.x, GRID), roundTo(p.y, GRID));
}

wxPoint DrawBoard::SnapToStep(const wxPoint& p) const
{
    auto roundTo = [](int v, int step) {
        int r = (v + step / 2) / step;
        return r * step;
        };
    return wxPoint(roundTo(p.x, STEP), roundTo(p.y, STEP));
}

void DrawBoard::RerouteWiresForMovedComponent(int compIdx)
{
    if (compIdx < 0 || compIdx >= (int)components.size()) return;

    // 移动后的引脚坐标
    const auto newPins = components[compIdx]->GetPins();

    // 遍历所有折线：如果端点等于 preMovePins 的某个点，就替换为对应的新引脚
    for (auto& poly : wires) {
        if (poly.size() < 2) continue;

        bool changed = false;
        wxPoint start = poly.front();
        wxPoint end = poly.back();

        auto findPinIdx = [](const std::vector<wxPoint>& pins, const wxPoint& p) -> int {
            for (int i = 0; i < (int)pins.size(); ++i) {
                if (pins[i] == p) return i;
            }
            return -1;
            };

        int sIdx = findPinIdx(preMovePins, start);
        int eIdx = findPinIdx(preMovePins, end);

        if (sIdx >= 0) { start = newPins[sIdx]; changed = true; }
        if (eIdx >= 0) { end = newPins[eIdx]; changed = true; }

        if (changed) {
            // 重新生成 L 形折线，保持正交
            poly = MakeManhattan(start, end);
        }
    }

    // 为连续拖动做准备
    preMovePins = newPins;
}

void DrawBoard::DeleteSelectedWire() {
    if (selectedWireIndex >= 0 && selectedWireIndex < (int)wires.size()) {
        wires.erase(wires.begin() + selectedWireIndex);
        selectedWireIndex = -1;
        Refresh(false);

        // ★ 清空统一选择并通知
        m_selKind = SelKind::None;
        m_selId = -1;
        NotifySelectionChanged();
    }
}

void DrawBoard::DeleteSelection() {
    if (selectedGateIndex >= 0) {
        DeleteSelectedGate();
        return;
    }
    if (selectedWireIndex >= 0) {
        DeleteSelectedWire();
        return;
    }
    // 什么也没选中就不做事
}

int DrawBoard::Dist2_PointToSeg(const wxPoint& p, const wxPoint& a, const wxPoint& b)
{
    // 向量法：投影到 AB，夹在 [0,1] 区间内；返回平方距离避免开方
    const int vx = b.x - a.x;
    const int vy = b.y - a.y;
    const int wx = p.x - a.x;
    const int wy = p.y - a.y;

    const int vv = vx * vx + vy * vy;
    if (vv == 0) { // 退化为点
        const int dx = p.x - a.x;
        const int dy = p.y - a.y;
        return dx * dx + dy * dy;
    }
    // t = (w·v)/|v|^2
    const double t = (wx * vx + wy * vy) / (double)vv;
    double clamped = t < 0 ? 0 : (t > 1 ? 1 : t);

    const double projx = a.x + clamped * vx;
    const double projy = a.y + clamped * vy;
    const double dx = p.x - projx;
    const double dy = p.y - projy;
    return int(dx * dx + dy * dy);
}

int DrawBoard::HitTestWire(const wxPoint& pt) const
{
    const int th2 = LINE_HIT_PX * LINE_HIT_PX; // 命中阈值的平方
    // 从上往下优先“最上层”（与门的命中逻辑一致，倒序遍历更像“后画的在上面”）
    for (int i = (int)wires.size() - 1; i >= 0; --i) {
        const auto& poly = wires[i];
        for (size_t k = 1; k < poly.size(); ++k) {
            if (Dist2_PointToSeg(pt, poly[k - 1], poly[k]) <= th2) {
                return i;
            }
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
