#pragma once

#include "BoardModel.h"
#include "CommandManager.h"
#include "DrawBoard.h"
#include "PropertyPane.h"
#include "ResourceManager.h"

#include <wx/frame.h>
#include <wx/splitter.h>

class MainFrame : public wxFrame {
public:
    MainFrame();

private:
    enum MenuIds {
        ID_AddComponent = wxID_HIGHEST + 1,
        ID_DeleteComponent,
        ID_SaveBoard,
        ID_OpenBoard
    };

    void CreateMenus();
    void CreateToolbar();
    void BindEvents();

    void OnNewBoard(wxCommandEvent &event);
    void OnOpenBoard(wxCommandEvent &event);
    void OnSaveBoard(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);

    void OnAddComponent(wxCommandEvent &event);
    void OnDeleteComponent(wxCommandEvent &event);
    void OnUndo(wxCommandEvent &event);
    void OnRedo(wxCommandEvent &event);

    void UpdateUndoRedoUI();
    void UpdateTitle();
    std::string GenerateComponentId();
    void UpdateIdCounterFromBoard();

    BoardModel m_board;
    CommandManager m_commands;
    DrawBoard *m_drawBoard{nullptr};
    PropertyPane *m_propertyPane{nullptr};
    ResourceManager m_resources;
    wxToolBar *m_toolBar{nullptr};
    int m_componentCounter{0};
};
