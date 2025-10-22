#include "DrawBoard.h"
#include <fstream>

// 选中锚点配色
const wxColour DrawBoard::HANDLE_FILL_RGB(255, 255, 255);
const wxColour DrawBoard::HANDLE_STROKE_RGB(0, 0, 0);

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

        // 已保存的线
        gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
        for (const auto& line : lines) {
            gc->StrokeLine(line.first.x, line.first.y, line.second.x, line.second.y);
        }

        // 临时预览线
        if (isDrawing) {
            gc->SetPen(wxPen(wxColour(0, 0, 255), 2, wxPENSTYLE_DOT));
            gc->StrokeLine(currentStart.x, currentStart.y, mousePos.x, mousePos.y);
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
        components[m_draggingIndex]->SetCenter(m_dragStartCenter + delta);
        Refresh(false);
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
            if (!HasCapture()) CaptureMouse();
            Refresh();
        }
        else {
            selectedGateIndex = -1;
            m_draggingIndex = -1;
            m_isDragging = false;
            Refresh();
        }
    }

    // 画线
    if (currentTool == ID_TOOL_HAND && isDrawingLine) {
        currentStart = pos;
        currentEnd = currentStart;
        isDrawing = true;
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
        Refresh();
    }

    if (isDrawingLine && isDrawing) {
        currentEnd = event.GetPosition();
        lines.emplace_back(std::make_pair(currentStart, currentEnd));
        isDrawing = false;
        Refresh();
    }
}

// ============ 对外接口 ============
void DrawBoard::ClearTexts() { texts.clear(); Refresh(); }
void DrawBoard::ClearPics() { lines.clear(); Refresh(); }

void DrawBoard::AddGate(const wxPoint& center, const wxString& typeName)
{
    ComponentType t = NameToType(typeName);
    auto comp = MakeComponent(t, center);
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

    // 线
    for (auto& line : lines) {
        Json::Value obj;
        obj["type"] = "Line";
        obj["x1"] = line.first.x;
        obj["y1"] = line.first.y;
        obj["x2"] = line.second.x;
        obj["y2"] = line.second.y;
        root["shapes"].append(obj);
    }

    // 文本
    for (auto& txt : texts) {
        Json::Value obj;
        obj["content"] = std::string(txt.second.mb_str());
        obj["x"] = txt.first.x;
        obj["y"] = txt.first.y;
        root["texts"].append(obj);
    }

    // 门
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

    lines.clear();
    texts.clear();
    components.clear();

    // 线
    for (auto& obj : root["shapes"]) {
        wxPoint p1(obj["x1"].asInt(), obj["y1"].asInt());
        wxPoint p2(obj["x2"].asInt(), obj["y2"].asInt());
        lines.push_back({ p1, p2 });
    }

    // 文本
    for (auto& obj : root["texts"]) {
        wxPoint p(obj["x"].asInt(), obj["y"].asInt());
        wxString content = wxString::FromUTF8(obj["content"].asCString());
        texts.push_back({ p, content });
    }

    // 门
    for (auto& obj : root["gates"]) {
        wxPoint center(obj["x"].asInt(), obj["y"].asInt());
        wxString name = wxString::FromUTF8(obj["name"].asCString());
        auto comp = MakeComponent(NameToType(name), center);
        if (comp) {
            if (obj.isMember("scale")) comp->scale = obj["scale"].asDouble();
            comp->UpdateGeometry();
            components.push_back(std::move(comp));
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
    wxString u = s.Upper();
    if (u.Contains("AND"))  return ANDGATE;
    if (u.Contains("OR") && !u.Contains("XOR") && !u.Contains("NOR")) return ORGATE;
    if (u.Contains("NOT"))  return NOTGATE;
    if (u.Contains("NAND")) return NANDGATE;
    if (u.Contains("NOR"))  return NORGATE;
    if (u.Contains("XOR"))  return XORGATE;
    return ANDGATE; // 默认
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
    default:       return nullptr;
    }
}
