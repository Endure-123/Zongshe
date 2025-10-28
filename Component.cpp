#include <wx/graphics.h>
#include <cmath>
#include "Component.h"

// ============ AND ============
void ANDGate::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 3));

    wxGraphicsPath path = gc->CreatePath();

    // 主体：左矩形 + 右半圆
    path.MoveToPoint(m_center.x + 20 * scale, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y + 20 * scale);
    path.AddLineToPoint(m_center.x + 20 * scale, m_center.y + 20 * scale);
    path.AddArc(m_center.x + 20 * scale, m_center.y, 20 * scale, M_PI / 2, -M_PI / 2, false);

    // 输入两根
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y + 10 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y + 10 * scale);
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y - 10 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 10 * scale);

    // 输出
    path.MoveToPoint(m_center.x + 40 * scale, m_center.y);
    path.AddLineToPoint(m_center.x + 60 * scale, m_center.y);

    gc->StrokePath(path);

    // 选中定位点
    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }

    delete gc;
}

bool ANDGate::Isinside(const wxPoint& point) const {
    int left = m_BoundaryPoints[0].x;
    int right = m_BoundaryPoints[2].x;
    int top = m_BoundaryPoints[0].y;
    int bottom = m_BoundaryPoints[1].y;
    return (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom);
}

void ANDGate::UpdateGeometry() {
    m_BoundaryPoints[0] = wxPoint(m_center.x - 40 * scale, m_center.y - 20 * scale); // 左上
    m_BoundaryPoints[1] = wxPoint(m_center.x - 40 * scale, m_center.y + 20 * scale); // 左下
    m_BoundaryPoints[2] = wxPoint(m_center.x + 60 * scale, m_center.y - 20 * scale); // 右上
    m_BoundaryPoints[3] = wxPoint(m_center.x + 60 * scale, m_center.y + 20 * scale); // 右下
    pout = wxPoint(m_center.x + 60 * scale, m_center.y);
}

// ============ OR ============
void ORGate::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 3));

    wxGraphicsPath path = gc->CreatePath();

    // OR 外形（近似 IEEE 风格）
    path.MoveToPoint(m_center.x, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 20 * scale);
    path.AddQuadCurveToPoint(m_center.x, m_center.y, m_center.x - 20 * scale, m_center.y + 20 * scale);
    path.AddLineToPoint(m_center.x - 10 * scale, m_center.y + 20 * scale);
    path.AddQuadCurveToPoint(m_center.x + 20 * scale, m_center.y + 20 * scale, m_center.x + 40 * scale, m_center.y);
    path.AddQuadCurveToPoint(m_center.x + 20 * scale, m_center.y - 20 * scale, m_center.x, m_center.y - 20 * scale);

    // 输入两根
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y + 10 * scale);
    path.AddLineToPoint(m_center.x - 13 * scale, m_center.y + 10 * scale);
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y - 10 * scale);
    path.AddLineToPoint(m_center.x - 13 * scale, m_center.y - 10 * scale);

    // 输出
    path.MoveToPoint(m_center.x + 40 * scale, m_center.y);
    path.AddLineToPoint(m_center.x + 60 * scale, m_center.y);

    gc->StrokePath(path);

    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }

    delete gc;
}

bool ORGate::Isinside(const wxPoint& point) const {
    int left = m_BoundaryPoints[0].x;
    int right = m_BoundaryPoints[2].x;
    int top = m_BoundaryPoints[0].y;
    int bottom = m_BoundaryPoints[1].y;
    return (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom);
}

void ORGate::UpdateGeometry() {
    m_BoundaryPoints[0] = wxPoint(m_center.x - 40 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[1] = wxPoint(m_center.x - 40 * scale, m_center.y + 22 * scale);
    m_BoundaryPoints[2] = wxPoint(m_center.x + 60 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[3] = wxPoint(m_center.x + 60 * scale, m_center.y + 22 * scale);
    pout = wxPoint(m_center.x + 60 * scale, m_center.y);
}

// ============ NOT ============
void NOTGate::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 3));

    wxGraphicsPath path = gc->CreatePath();

    // 三角形主体
    path.MoveToPoint(m_center.x - 30 * scale, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 30 * scale, m_center.y + 20 * scale);
    path.AddLineToPoint(m_center.x + 30 * scale, m_center.y);
    path.AddLineToPoint(m_center.x - 30 * scale, m_center.y - 20 * scale);

    // 输出气泡
    path.AddCircle(m_center.x + 35 * scale, m_center.y, 5 * scale);

    // 输入线
    path.MoveToPoint(m_center.x - 50 * scale, m_center.y);
    path.AddLineToPoint(m_center.x - 30 * scale, m_center.y);

    // 输出线
    path.MoveToPoint(m_center.x + 40 * scale, m_center.y);
    path.AddLineToPoint(m_center.x + 60 * scale, m_center.y);

    gc->StrokePath(path);

    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }

    delete gc;
}

bool NOTGate::Isinside(const wxPoint& point) const {
    int left = m_BoundaryPoints[0].x;
    int right = m_BoundaryPoints[2].x;
    int top = m_BoundaryPoints[0].y;
    int bottom = m_BoundaryPoints[1].y;
    return (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom);
}

void NOTGate::UpdateGeometry() {
    m_BoundaryPoints[0] = wxPoint(m_center.x - 50 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[1] = wxPoint(m_center.x - 50 * scale, m_center.y + 22 * scale);
    m_BoundaryPoints[2] = wxPoint(m_center.x + 60 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[3] = wxPoint(m_center.x + 60 * scale, m_center.y + 22 * scale);
    pout = wxPoint(m_center.x + 60 * scale, m_center.y);
}

// ============ NAND ============
void NANDGate::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 3));

    wxGraphicsPath path = gc->CreatePath();

    // AND 主体
    path.MoveToPoint(m_center.x + 20 * scale, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y + 20 * scale);
    path.AddLineToPoint(m_center.x + 20 * scale, m_center.y + 20 * scale);
    path.AddArc(m_center.x + 20 * scale, m_center.y, 20 * scale, M_PI / 2, -M_PI / 2, false);

    // 输入
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y + 10 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y + 10 * scale);
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y - 10 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 10 * scale);

    // 反相气泡：略右移 & 略小，避免与 AND 半圆描边重合
    path.AddCircle(m_center.x + 47 * scale, m_center.y, 4 * scale);

    // 输出线：从气泡右端再右一点起笔
    path.MoveToPoint(m_center.x + 51 * scale, m_center.y);
    path.AddLineToPoint(m_center.x + 66 * scale, m_center.y);

    gc->StrokePath(path);

    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }
    delete gc;
}

bool NANDGate::Isinside(const wxPoint& point) const {
    int left = m_BoundaryPoints[0].x;
    int right = m_BoundaryPoints[2].x;
    int top = m_BoundaryPoints[0].y;
    int bottom = m_BoundaryPoints[1].y;
    return (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom);
}

void NANDGate::UpdateGeometry() {
    m_BoundaryPoints[0] = wxPoint(m_center.x - 40 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[1] = wxPoint(m_center.x - 40 * scale, m_center.y + 22 * scale);
    m_BoundaryPoints[2] = wxPoint(m_center.x + 66 * scale, m_center.y - 22 * scale); // 65 -> 66
    m_BoundaryPoints[3] = wxPoint(m_center.x + 66 * scale, m_center.y + 22 * scale); // 65 -> 66
    pout = wxPoint(m_center.x + 66 * scale, m_center.y);
}


// ============ NOR ============
void NORGate::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 3));

    wxGraphicsPath path = gc->CreatePath();

    // OR 主体
    path.MoveToPoint(m_center.x, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 20 * scale);
    path.AddQuadCurveToPoint(m_center.x, m_center.y, m_center.x - 20 * scale, m_center.y + 20 * scale);
    path.AddLineToPoint(m_center.x - 10 * scale, m_center.y + 20 * scale);
    path.AddQuadCurveToPoint(m_center.x + 20 * scale, m_center.y + 20 * scale, m_center.x + 40 * scale, m_center.y);
    path.AddQuadCurveToPoint(m_center.x + 20 * scale, m_center.y - 20 * scale, m_center.x, m_center.y - 20 * scale);

    // 输入
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y + 10 * scale);
    path.AddLineToPoint(m_center.x - 13 * scale, m_center.y + 10 * scale);
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y - 10 * scale);
    path.AddLineToPoint(m_center.x - 13 * scale, m_center.y - 10 * scale);

    // 输出气泡
    path.AddCircle(m_center.x + 45 * scale, m_center.y, 5 * scale);

    // 输出线
    path.MoveToPoint(m_center.x + 50 * scale, m_center.y);
    path.AddLineToPoint(m_center.x + 65 * scale, m_center.y);

    gc->StrokePath(path);

    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }

    delete gc;
}

bool NORGate::Isinside(const wxPoint& point) const {
    int left = m_BoundaryPoints[0].x;
    int right = m_BoundaryPoints[2].x;
    int top = m_BoundaryPoints[0].y;
    int bottom = m_BoundaryPoints[1].y;
    return (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom);
}

void NORGate::UpdateGeometry() {
    m_BoundaryPoints[0] = wxPoint(m_center.x - 40 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[1] = wxPoint(m_center.x - 40 * scale, m_center.y + 22 * scale);
    m_BoundaryPoints[2] = wxPoint(m_center.x + 65 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[3] = wxPoint(m_center.x + 65 * scale, m_center.y + 22 * scale);
    pout = wxPoint(m_center.x + 65 * scale, m_center.y);
}

// ============ XOR ============
void XORGate::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 3));

    wxGraphicsPath path = gc->CreatePath();

    // 先画 OR 的主体
    path.MoveToPoint(m_center.x, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale, m_center.y - 20 * scale);
    path.AddQuadCurveToPoint(m_center.x, m_center.y, m_center.x - 20 * scale, m_center.y + 20 * scale);
    path.AddLineToPoint(m_center.x - 10 * scale, m_center.y + 20 * scale);
    path.AddQuadCurveToPoint(m_center.x + 20 * scale, m_center.y + 20 * scale, m_center.x + 40 * scale, m_center.y);
    path.AddQuadCurveToPoint(m_center.x + 20 * scale, m_center.y - 20 * scale, m_center.x, m_center.y - 20 * scale);

    // 画前缘的“第二条曲线”（与 OR 输入侧平行、向左偏移）
    double offset = 6.0 * scale;
    path.MoveToPoint(m_center.x - offset, m_center.y - 20 * scale);
    path.AddLineToPoint(m_center.x - 20 * scale - offset, m_center.y - 20 * scale);
    path.AddQuadCurveToPoint(m_center.x - offset, m_center.y, m_center.x - 20 * scale - offset, m_center.y + 20 * scale);

    // 输入两根
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y + 10 * scale);
    path.AddLineToPoint(m_center.x - 13 * scale, m_center.y + 10 * scale);
    path.MoveToPoint(m_center.x - 40 * scale, m_center.y - 10 * scale);
    path.AddLineToPoint(m_center.x - 13 * scale, m_center.y - 10 * scale);

    // 输出
    path.MoveToPoint(m_center.x + 40 * scale, m_center.y);
    path.AddLineToPoint(m_center.x + 60 * scale, m_center.y);

    gc->StrokePath(path);

    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }

    delete gc;
}

bool XORGate::Isinside(const wxPoint& point) const {
    int left = m_BoundaryPoints[0].x;
    int right = m_BoundaryPoints[2].x;
    int top = m_BoundaryPoints[0].y;
    int bottom = m_BoundaryPoints[1].y;
    return (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom);
}

void XORGate::UpdateGeometry() {
    m_BoundaryPoints[0] = wxPoint(m_center.x - 46 * scale, m_center.y - 22 * scale); // 比 OR 再往左一点，为前缘双曲线预留空间
    m_BoundaryPoints[1] = wxPoint(m_center.x - 46 * scale, m_center.y + 22 * scale);
    m_BoundaryPoints[2] = wxPoint(m_center.x + 60 * scale, m_center.y - 22 * scale);
    m_BoundaryPoints[3] = wxPoint(m_center.x + 60 * scale, m_center.y + 22 * scale);
    pout = wxPoint(m_center.x + 60 * scale, m_center.y);
}

// Component.cpp 末尾新增：返回每个门的可连接引脚（输入 + 输出），顺序随意
std::vector<wxPoint> ANDGate::GetPins() const {
    return {
        wxPoint(m_center.x - 40 * scale, m_center.y - 10 * scale), // IN1
        wxPoint(m_center.x - 40 * scale, m_center.y + 10 * scale), // IN2
        wxPoint(m_center.x + 60 * scale, m_center.y)               // OUT
    };
}

std::vector<wxPoint> ORGate::GetPins() const {
    return {
        wxPoint(m_center.x - 40 * scale, m_center.y - 10 * scale),
        wxPoint(m_center.x - 40 * scale, m_center.y + 10 * scale),
        wxPoint(m_center.x + 60 * scale, m_center.y)
    };
}

std::vector<wxPoint> NOTGate::GetPins() const {
    return {
        wxPoint(m_center.x - 50 * scale, m_center.y),              // IN
        wxPoint(m_center.x + 60 * scale, m_center.y)               // OUT
    };
}

std::vector<wxPoint> NANDGate::GetPins() const {
    return {
        wxPoint(m_center.x - 40 * scale, m_center.y - 10 * scale),
        wxPoint(m_center.x - 40 * scale, m_center.y + 10 * scale),
        wxPoint(m_center.x + 66 * scale, m_center.y)               // OUT（注意 66）
    };
}

std::vector<wxPoint> NORGate::GetPins() const {
    return {
        wxPoint(m_center.x - 40 * scale, m_center.y - 10 * scale),
        wxPoint(m_center.x - 40 * scale, m_center.y + 10 * scale),
        wxPoint(m_center.x + 65 * scale, m_center.y)               // OUT（注意 65）
    };
}

std::vector<wxPoint> XORGate::GetPins() const {
    return {
        wxPoint(m_center.x - 40 * scale, m_center.y - 10 * scale),
        wxPoint(m_center.x - 40 * scale, m_center.y + 10 * scale),
        wxPoint(m_center.x + 60 * scale, m_center.y)
    };
}

// ===================== Nodes =====================

static inline int sqr(int v) { return v * v; }

// ------ 普通结点：实心小圆点 ------
void NodeDot::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;
    const double r = 5.0 * scale;

    gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
    gc->SetBrush(*wxBLACK_BRUSH);
    gc->DrawEllipse(m_center.x - r, m_center.y - r, 2 * r, 2 * r);

    // 选中定位点（沿用四点小圈）
    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++)
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
    }
    delete gc;
}

bool NodeDot::Isinside(const wxPoint& p) const {
    const int R = int(7 * scale);
    return sqr(p.x - m_center.x) + sqr(p.y - m_center.y) <= R * R;
}

void NodeDot::UpdateGeometry() {
    const int b = int(10 * scale);
    m_BoundaryPoints[0] = wxPoint(m_center.x - b, m_center.y - b);
    m_BoundaryPoints[1] = wxPoint(m_center.x - b, m_center.y + b);
    m_BoundaryPoints[2] = wxPoint(m_center.x + b, m_center.y - b);
    m_BoundaryPoints[3] = wxPoint(m_center.x + b, m_center.y + b);
}

std::vector<wxPoint> NodeDot::GetPins() const {
    // 允许多线汇聚：把中心点当成公共端
    return { m_center };
}

// ------ 起始节点：方框里一个实心圆，连线接在方框右边 ------
void StartNode::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;

    // 尺寸
    const double halfW = 10.0 * scale;   // 方框半宽（更小更像示意图）
    const double halfH = 10.0 * scale;   // 方框半高
    const double rBox = 2.5 * scale;   // 方框圆角
    const double rDot = 5.0 * scale;   // 内圆半径

    // 1) 方框
    gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
    gc->SetBrush(*wxWHITE_BRUSH);
#if wxCHECK_VERSION(3,1,0)
    gc->DrawRoundedRectangle(m_center.x - halfW, m_center.y - halfH,
        2 * halfW, 2 * halfH, rBox);
#else
    wxGraphicsPath rectPath = gc->CreatePath();
    rectPath.AddRectangle(m_center.x - halfW, m_center.y - halfH, 2 * halfW, 2 * halfH);
    gc->StrokePath(rectPath);
#endif

    // 2) 内部实心圆（靠右放置，视觉上“跟着方框右边”）
    const double dotCx = m_center.x + (halfW - rDot - 1.5 * scale);
    const double dotCy = m_center.y;
    gc->SetBrush(*wxBLACK_BRUSH);          // 颜色不做强制要求
    gc->DrawEllipse(dotCx - rDot, dotCy - rDot, 2 * rDot, 2 * rDot);

    // 不画外伸的引脚了——连线会直接接在方框右边界中心

    // 3) 选中时四角定位点
    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; ++j) {
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
        }
    }

    delete gc;
}

bool StartNode::Isinside(const wxPoint& p) const {
    // 命中判定：按方框边界
    const int left = m_BoundaryPoints[0].x;
    const int top = m_BoundaryPoints[0].y;
    const int right = m_BoundaryPoints[2].x;
    const int bottom = m_BoundaryPoints[1].y;
    return (p.x >= left && p.x <= right && p.y >= top && p.y <= bottom);
}

void StartNode::UpdateGeometry() {
    // 与 drawSelf 中的 halfW/halfH 保持一致
    const int halfW = int(10.0 * scale);
    const int halfH = int(10.0 * scale);

    m_BoundaryPoints[0] = wxPoint(m_center.x - halfW, m_center.y - halfH); // 左上
    m_BoundaryPoints[1] = wxPoint(m_center.x - halfW, m_center.y + halfH); // 左下
    m_BoundaryPoints[2] = wxPoint(m_center.x + halfW, m_center.y - halfH); // 右上
    m_BoundaryPoints[3] = wxPoint(m_center.x + halfW, m_center.y + halfH); // 右下
}

std::vector<wxPoint> StartNode::GetPins() const {
    // 连接点：方框右边界的中心（不外伸）
    const int halfW = int(10.0 * scale);
    return { wxPoint(m_center.x + halfW, m_center.y) };
}

// ------ 终止节点：空心圆（信号终点，只连入） ------
void EndNode::drawSelf(wxMemoryDC& memDC) {
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (!gc) return;

    const double r = 8.0 * scale;
    gc->SetPen(wxPen(wxColour(0, 0, 0), 2));
    gc->SetBrush(*wxWHITE_BRUSH);
    gc->DrawEllipse(m_center.x - r, m_center.y - r, 2 * r, 2 * r);

    // 内圈（更像终端标记）
    gc->DrawEllipse(m_center.x - r / 2, m_center.y - r / 2, r, r);

    if (m_isSelected) {
        gc->SetPen(wxPen(wxColour(128, 128, 128), 2));
        for (int j = 0; j < 4; j++)
            gc->DrawEllipse(m_BoundaryPoints[j].x - 4, m_BoundaryPoints[j].y - 4, 8, 8);
    }
    delete gc;
}

bool EndNode::Isinside(const wxPoint& p) const {
    const int R = int(10 * scale);
    return sqr(p.x - m_center.x) + sqr(p.y - m_center.y) <= R * R;
}

void EndNode::UpdateGeometry() {
    const int b = int(12 * scale);
    m_BoundaryPoints[0] = wxPoint(m_center.x - b, m_center.y - b);
    m_BoundaryPoints[1] = wxPoint(m_center.x - b, m_center.y + b);
    m_BoundaryPoints[2] = wxPoint(m_center.x + b, m_center.y - b);
    m_BoundaryPoints[3] = wxPoint(m_center.x + b, m_center.y + b);
}

std::vector<wxPoint> EndNode::GetPins() const {
    // 连接点放在圆心略左
    return { wxPoint(m_center.x - int(8 * scale), m_center.y) };
}
