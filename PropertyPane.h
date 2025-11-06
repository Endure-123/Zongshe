#pragma once

#include "BoardModel.h"
#include "CommandManager.h"

#include <map>
#include <wx/panel.h>
#include <wx/propgrid/manager.h>

class PropertyPane : public wxPanel {
public:
    PropertyPane(wxWindow *parent, BoardModel &board, CommandManager &commands);
    ~PropertyPane() override;

private:
    void RefreshProperties();
    void OnBoardChanged();
    void OnPropertyChanged(wxPropertyGridEvent &event);

    BoardModel &m_board;
    CommandManager &m_commands;
    BoardModel::ListenerToken m_listenerToken{0};
    wxPropertyGridManager *m_propertyGrid{nullptr};
    std::map<wxPGProperty *, std::string> m_propertyKeys;
    bool m_updating{false};

    wxDECLARE_EVENT_TABLE();
};
