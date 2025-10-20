#include "DrawBoard.h"
#include "AppConfig.h"
#include "ResourceManager.h"
#include <fstream>
#include <array>

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

        // 元器件
        for (const auto& g : gates) {
            gc->DrawBitmap(g.bmp, g.pos.x, g.pos.y, g.bmp.GetWidth(), g.bmp.GetHeight());
        }

        // 高亮选中的元件
        if (selectedGateIndex >= 0 && selectedGateIndex < (int)gates.size()) {
            const auto& g = gates[selectedGateIndex];

            // 关键：这里的尺寸必须与实际绘制一致（若你对位图做了按百分比缩放，取缩放后的尺寸）
            wxSize bmpSize = g.bmp.GetSize();

            auto anchors = GetGateAnchorPoints(g.pos, bmpSize);
            DrawSelectionHandles(gc, anchors);

            // （可选）如果仍想保留非常淡的外框提示，可启用这三行：
            // gc->SetPen(wxPen(wxColour(0,150,0), 1, wxPENSTYLE_SHORT_DASH));
            // gc->SetBrush(*wxTRANSPARENT_BRUSH);
            // gc->StrokeRectangle(g.pos.x, g.pos.y, bmpSize.GetWidth(), bmpSize.GetHeight());
        }

        else {
            selectedGateIndex = -1;
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
    for (int x = 0; x < size.GetWidth(); x += gridSize)
        gc->StrokeLine(x, 0, x, size.GetHeight());
    for (int y = 0; y < size.GetHeight(); y += gridSize)
        gc->StrokeLine(0, y, size.GetWidth(), y);
}

void DrawBoard::OnButtonMove(wxMouseEvent& event)
{
    mousePos = event.GetPosition();
    st1->SetLabel(wxString::Format("x: %d", event.GetX()));
    st2->SetLabel(wxString::Format("y: %d", event.GetY()));

    if (isDrawing) currentEnd = mousePos;
    Refresh();
}

void DrawBoard::OnLeftDown(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();

    // 插入模式
    if (pSelectedGateName && !pSelectedGateName->IsEmpty()) {
        wxBitmap bmp = ResourceManager::LoadBitmapByPercent(IMG_DIR, *pSelectedGateName, GATE_DRAW_SCALE);
        if (bmp.IsOk()) {
            wxPoint drawPos(pos.x - bmp.GetWidth() / 2, pos.y - bmp.GetHeight() / 2);
            AddGate(drawPos, bmp, *pSelectedGateName);
        }
        Refresh();
        return;
    }

    // Arrow 工具：选择元器件
    if (currentTool == ID_TOOL_ARROW) {
        SelectGate(pos);
        return;
    }

    // Hand 工具：画线
    if (currentTool == ID_TOOL_HAND && isDrawingLine) {
        currentStart = pos;
        currentEnd = currentStart;
        isDrawing = true;
    }

    // Text 工具
    else if (currentTool == ID_TOOL_TEXT && isInsertingText) {
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
    if (isDrawingLine && isDrawing) {
        currentEnd = event.GetPosition();
        lines.emplace_back(std::make_pair(currentStart, currentEnd));
        isDrawing = false;
        Refresh();
    }
}

void DrawBoard::ClearTexts() { texts.clear(); Refresh(); }
void DrawBoard::ClearPics() { lines.clear(); Refresh(); }

void DrawBoard::AddGate(const wxPoint& pos, const wxBitmap& bmp, const wxString& name)
{
    GateInfo g{ pos, name, bmp };
    gates.push_back(g);
    Refresh();
}

void DrawBoard::SelectGate(const wxPoint& pos)
{
    selectedGateIndex = -1;
    for (int i = (int)gates.size() - 1; i >= 0; --i) {
        wxPoint gPos = gates[i].pos;
        wxBitmap& bmp = gates[i].bmp;
        wxRect rect(gPos.x, gPos.y, bmp.GetWidth(), bmp.GetHeight());
        if (rect.Contains(pos)) {
            selectedGateIndex = i;
            Refresh();
            return;
        }
    }
}

void DrawBoard::DeleteSelectedGate()
{
    if (selectedGateIndex >= 0 && selectedGateIndex < (int)gates.size()) {
        gates.erase(gates.begin() + selectedGateIndex);
        selectedGateIndex = -1;
        Refresh();
    }
}

// ---------------- JSON 保存/加载 ----------------
void DrawBoard::SaveToJson(const std::string& filename)
{
    Json::Value root;

    for (auto& line : lines) {
        Json::Value obj;
        obj["type"] = "Line";
        obj["x1"] = line.first.x;
        obj["y1"] = line.first.y;
        obj["x2"] = line.second.x;
        obj["y2"] = line.second.y;
        root["shapes"].append(obj);
    }

    for (auto& txt : texts) {
        Json::Value obj;
        obj["content"] = std::string(txt.second.mb_str());
        obj["x"] = txt.first.x;
        obj["y"] = txt.first.y;
        root["texts"].append(obj);
    }

    for (auto& g : gates) {
        Json::Value obj;
        obj["name"] = std::string(g.name.mb_str());
        obj["x"] = g.pos.x;
        obj["y"] = g.pos.y;
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
    gates.clear();

    for (auto& obj : root["shapes"]) {
        wxPoint p1(obj["x1"].asInt(), obj["y1"].asInt());
        wxPoint p2(obj["x2"].asInt(), obj["y2"].asInt());
        lines.push_back({ p1, p2 });
    }

    for (auto& obj : root["texts"]) {
        wxPoint p(obj["x"].asInt(), obj["y"].asInt());
        wxString content = wxString::FromUTF8(obj["content"].asCString());
        texts.push_back({ p, content });
    }

    for (auto& obj : root["gates"]) {
        wxPoint p(obj["x"].asInt(), obj["y"].asInt());
        wxString name = wxString::FromUTF8(obj["name"].asCString());
        wxBitmap bmp = ResourceManager::LoadBitmapByPercent(IMG_DIR, name, GATE_DRAW_SCALE);
        AddGate(p, bmp, name);
    }

    Refresh();
}

// 计算四角锚点：左上、右上、左下、右下
std::array<wxPoint, 4> DrawBoard::GetGateAnchorPoints(const wxPoint& topLeft, const wxSize& bmpSize) const
{
    int x0 = topLeft.x;
    int y0 = topLeft.y;
    int x1 = topLeft.x + bmpSize.GetWidth();
    int y1 = topLeft.y + bmpSize.GetHeight();

    // 外扩/内缩，正=往外，负=往内
    x0 -= HANDLE_OFFSET_PX;
    y0 -= HANDLE_OFFSET_PX;
    x1 += HANDLE_OFFSET_PX;
    y1 += HANDLE_OFFSET_PX;

    return { wxPoint{x0,y0}, wxPoint{x1,y0}, wxPoint{x0,y1}, wxPoint{x1,y1} };
}

void DrawBoard::DrawSelectionHandles(wxGraphicsContext* gc, const std::array<wxPoint, 4>& anchors)
{
    if (!gc) return;

    gc->SetPen(wxPen(HANDLE_STROKE_RGB, HANDLE_STROKE_PX));
    gc->SetBrush(wxBrush(HANDLE_FILL_RGB));

    const double r = HANDLE_RADIUS_PX;
    for (const auto& p : anchors) {
        const double cx = static_cast<double>(p.x);
        const double cy = static_cast<double>(p.y);
        gc->DrawEllipse(cx - r, cy - r, 2 * r, 2 * r);
    }
}

std::vector<wxPoint> DrawBoard::GetSelectedGateAnchors() const
{
    if (selectedGateIndex >= 0 && selectedGateIndex < (int)gates.size()) {
        const auto& g = gates[selectedGateIndex];
        wxSize bmpSize = g.bmp.GetSize();
        auto arr = GetGateAnchorPoints(g.pos, bmpSize);
        return std::vector<wxPoint>(arr.begin(), arr.end());
    }
    return {};
}


