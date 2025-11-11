#pragma once
#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/propgrid/manager.h>

class DrawBoard;

class PropertyPane : public wxPanel {
public:
    PropertyPane(wxWindow* parent, DrawBoard* board);

    // 由外部在“选择变化”时调用，重建属性页
    void RebuildBySelection();
    DrawBoard* m_board = nullptr;
private:
    void BuildEmptyPage();
    void BuildGatePage(long id);
    void BuildWirePage(long id);
    void OnPropChanged(wxPropertyGridEvent& e);

    wxAuiManager m_mgr;
    wxPropertyGridManager* m_pg = nullptr;
    
};
