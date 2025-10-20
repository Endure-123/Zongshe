#pragma once
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/treectrl.h>
#include "DrawBoard.h"
#include "AppConfig.h"

enum {
    ID_SaveJSON = wxID_HIGHEST + 100,
    ID_LoadJSON
};

class cMain : public wxFrame
{
public:
    cMain();
    ~cMain();

private:
    wxMenuBar* menuBar = nullptr;
    wxMenu* fileMenu = nullptr;
    wxMenu* editMenu = nullptr;
    wxToolBar* m_toolBar = nullptr;

    wxSplitterWindow* m_splitter = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxTreeCtrl* m_treeCtrl = nullptr;

    DrawBoard* drawBoard = nullptr;

    wxString selectedGateName;

    wxBitmap LoadToolBitmap(const wxString& filename, int size = 24);

    void OnExit(wxCommandEvent& evt);
    void OnClearTexts(wxCommandEvent& evt);
    void OnClearPics(wxCommandEvent& evt);
    void OnToolSelected(wxCommandEvent& evt);

    void OnTreeItemActivated(wxTreeEvent& event);
    void OnTreeSelChanged(wxTreeEvent& event);
    void SetSelectedGate(const wxString& label);

    // JSON 保存/加载
    void OnSaveJson(wxCommandEvent& evt);
    void OnLoadJson(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};
