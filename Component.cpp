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
