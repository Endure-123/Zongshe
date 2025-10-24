#pragma once

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/treectrl.h>
#include <wx/aui/aui.h>      // AUI
#include "DrawBoard.h"
#include "AppConfig.h"

enum {
    ID_SaveJSON = wxID_HIGHEST + 100,
    ID_LoadJSON
};

// 前向声明：属性面板，避免头文件循环依赖
class PropertyPane;

class cMain : public wxFrame
{
public:
    cMain();
    ~cMain();

private:
    // AUI 布局管理器（管理 CenterPane / 左下属性面板等）
    wxAuiManager m_aui;

    wxMenuBar* menuBar = nullptr;
    wxMenu* fileMenu = nullptr;
    wxMenu* editMenu = nullptr;
    wxToolBar* m_toolBar = nullptr;

    wxSplitterWindow* m_splitter = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxTreeCtrl* m_treeCtrl = nullptr;

    DrawBoard* drawBoard = nullptr;

    // 属性面板指针
    PropertyPane* m_prop = nullptr;

    wxString selectedGateName;

    wxBitmap LoadToolBitmap(const wxString& filename, int size = 24);

    void OnExit(wxCommandEvent& evt);
    void OnClearTexts(wxCommandEvent& evt);
    void OnClearPics(wxCommandEvent& evt);
    void OnToolSelected(wxCommandEvent& evt);

    void OnTreeItemActivated(wxTreeEvent& event);
    void OnTreeSelChanged(wxTreeEvent& event);
    void SetSelectedGate(const wxString& label);

    wxAuiManager m_mgr;
    wxSplitterWindow* m_leftSplitter = nullptr;
    wxPanel* m_treePanel = nullptr;
    PropertyPane* m_propPane = nullptr;

    // JSON 保存/加载
    void OnSaveJson(wxCommandEvent& evt);
    void OnLoadJson(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};
