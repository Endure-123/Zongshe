#include "DrawBoard.h"

#include "Commands.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>

namespace {
constexpr int kComponentWidth = 110;
constexpr int kComponentHeight = 60;
constexpr int kCornerRadius = 6;
}

wxBEGIN_EVENT_TABLE(DrawBoard, wxPanel)
    EVT_PAINT(DrawBoard::OnPaint)
    EVT_LEFT_DOWN(DrawBoard::OnLeftDown)
    EVT_LEFT_UP(DrawBoard::OnLeftUp)
    EVT_MOTION(DrawBoard::OnMotion)
    EVT_LEAVE_WINDOW(DrawBoard::OnLeaveWindow)
wxEND_EVENT_TABLE()

DrawBoard::DrawBoard(wxWindow *parent, BoardModel &board, CommandManager &commands)
    : wxPanel(parent), m_board(board), m_commands(commands) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_listenerToken = m_board.AddListener([this]() { Refresh(); });
}

DrawBoard::~DrawBoard() { m_board.RemoveListener(m_listenerToken); }

void DrawBoard::OnPaint(wxPaintEvent &) {
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    const ComponentData *selected = m_board.GetSelectedComponent();

    for (const auto &component : m_board.GetComponents()) {
        const wxRect rect = GetComponentRect(component);

        wxColour fill = wxColour(230, 230, 240);
        wxPen border(wxColour(90, 90, 120), 2);
        if (selected && selected->id == component.id) {
            fill = wxColour(200, 220, 255);
            border.SetColour(wxColour(30, 90, 180));
        }

        dc.SetBrush(wxBrush(fill));
        dc.SetPen(border);
        dc.DrawRoundedRectangle(rect, kCornerRadius);

        dc.SetTextForeground(wxColour(40, 40, 60));
        dc.DrawLabel(wxString::Format("%s\n(%0.0f, %0.0f)", component.type, component.position.x,
                                       component.position.y), rect, wxALIGN_CENTER);
    }
}

void DrawBoard::OnLeftDown(wxMouseEvent &event) {
    SetFocus();
    const wxPoint pos = event.GetPosition();
    const std::string hit = HitTest(pos);

    if (!hit.empty()) {
        ComponentData *component = m_board.FindComponent(hit);
        if (component) {
            m_board.SelectComponent(hit);
            m_dragging = true;
            m_dragStart = pos;
            m_boardStart = component->position;
            m_draggingId = hit;
            CaptureMouse();
        }
    } else {
        m_board.ClearSelection();
    }
}

void DrawBoard::OnLeftUp(wxMouseEvent &event) {
    if (!m_dragging) {
        return;
    }

    if (HasCapture()) {
        ReleaseMouse();
    }

    const wxPoint current = event.GetPosition();
    const wxPoint delta = current - m_dragStart;
    const BoardPoint newPosition{m_boardStart.x + delta.x, m_boardStart.y + delta.y};

    const double distance = std::hypot(newPosition.x - m_boardStart.x, newPosition.y - m_boardStart.y);
    if (distance > 0.5) {
        m_board.SetComponentPosition(m_draggingId, m_boardStart);
        m_commands.Execute(std::make_unique<MoveComponentCommand>(m_board, m_draggingId, m_boardStart, newPosition));
    } else {
        m_board.SetComponentPosition(m_draggingId, m_boardStart);
    }

    m_board.SelectComponent(m_draggingId);

    m_dragging = false;
    m_draggingId.clear();
}

void DrawBoard::OnMotion(wxMouseEvent &event) {
    if (!m_dragging || !event.Dragging()) {
        return;
    }

    const wxPoint current = event.GetPosition();
    const wxPoint delta = current - m_dragStart;
    const BoardPoint newPosition{m_boardStart.x + delta.x, m_boardStart.y + delta.y};

    m_board.SetComponentPosition(m_draggingId, newPosition);
}

void DrawBoard::OnLeaveWindow(wxMouseEvent &event) {
    if (m_dragging && HasCapture()) {
        ReleaseMouse();
    }
    m_dragging = false;
    m_draggingId.clear();
    event.Skip();
}

wxRect DrawBoard::GetComponentRect(const ComponentData &component) const {
    const wxPoint topLeft(static_cast<int>(std::round(component.position.x)),
                          static_cast<int>(std::round(component.position.y)));
    return {topLeft, wxSize(kComponentWidth, kComponentHeight)};
}

std::string DrawBoard::HitTest(const wxPoint &point) const {
    for (const auto &component : m_board.GetComponents()) {
        if (GetComponentRect(component).Contains(point)) {
            return component.id;
        }
    }
    return {};
}
