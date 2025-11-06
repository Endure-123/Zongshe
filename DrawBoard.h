#pragma once

#include "BoardModel.h"
#include "CommandManager.h"

#include <string>

#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>

class DrawBoard : public wxPanel {
public:
    DrawBoard(wxWindow *parent, BoardModel &board, CommandManager &commands);
    ~DrawBoard() override;

private:
    void OnPaint(wxPaintEvent &event);
    void OnLeftDown(wxMouseEvent &event);
    void OnLeftUp(wxMouseEvent &event);
    void OnMotion(wxMouseEvent &event);
    void OnLeaveWindow(wxMouseEvent &event);

    wxRect GetComponentRect(const ComponentData &component) const;
    std::string HitTest(const wxPoint &point) const;

    BoardModel &m_board;
    CommandManager &m_commands;
    BoardModel::ListenerToken m_listenerToken{0};

    bool m_dragging{false};
    wxPoint m_dragStart;
    BoardPoint m_boardStart;
    std::string m_draggingId;

    wxDECLARE_EVENT_TABLE();
};
