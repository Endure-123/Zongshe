// PropertyPane.cpp
#include "PropertyPane.h"
#include "DrawBoard.h"
#include <algorithm>

PropertyPane::PropertyPane(wxWindow* parent, DrawBoard* board)
    : wxPanel(parent, wxID_ANY), m_board(board)
{
    m_mgr.SetManagedWindow(this);

    m_pg = new wxPropertyGridManager(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER | wxPGMAN_DEFAULT_STYLE
    );
    m_mgr.AddPane(m_pg, wxAuiPaneInfo().CenterPane());
    m_mgr.Update();

    Bind(wxEVT_PG_CHANGED, &PropertyPane::OnPropChanged, this);
    BuildEmptyPage();
}

void PropertyPane::RebuildBySelection() {
    switch (m_board->GetSelectionKind()) {
    case SelKind::Gate: BuildGatePage(m_board->GetSelectionId()); break;
    case SelKind::Wire: BuildWirePage(m_board->GetSelectionId()); break;
    default:            BuildEmptyPage(); break;
    }
}

void PropertyPane::BuildEmptyPage() {
    m_pg->Clear();
    m_pg->AddPage("Canvas");

    m_pg->Append(new wxPropertyCategory("Board/Canvas"));
    m_pg->Append(new wxIntProperty("Grid Size (preview)", "grid_size", 20));
    m_pg->Append(new wxBoolProperty("Snap To Grid (preview)", "snap", true));

    auto* hint = new wxLongStringProperty(
        "Hint", "hint",
        "未选中任何对象。\n"
        "- 单击元件以编辑其属性\n"
        "- 单击连线以查看端点\n"
        "- 修改 Gate 的 Position X/Y 将即时作用于画布"
    );
    // ← 用 SetFlag 代替 ChangeFlag
    hint->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
    m_pg->Append(hint);
}

void PropertyPane::BuildGatePage(long id) {
    m_pg->Clear();
    m_pg->AddPage("Gate");

    if (id < 0 || id >= (long)m_board->components.size()) {
        BuildEmptyPage(); return;
    }
    auto* c = m_board->components[(size_t)id].get();

    m_pg->Append(new wxPropertyCategory("General"));
    auto* pid = m_pg->Append(new wxIntProperty("Index", "idx", (int)id));
    pid->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);

    wxString typeName = wxString::FromUTF8(m_board->TypeToName(c->m_type));
    auto* ptype = m_pg->Append(new wxStringProperty("Type", "type", typeName));
    ptype->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);

    m_pg->Append(new wxPropertyCategory("Geometry"));
    wxPoint center = c->GetCenter();
    m_pg->Append(new wxIntProperty("Position X", "pos_x", center.x));
    m_pg->Append(new wxIntProperty("Position Y", "pos_y", center.y));
    // 若要可编辑 scale，可按需添加
    // m_pg->Append(new wxFloatProperty("Scale", "scale", c->scale));
}

void PropertyPane::BuildWirePage(long id) {
    m_pg->Clear();
    m_pg->AddPage("Wire");

    if (id < 0 || id >= (long)m_board->wires.size()) {
        BuildEmptyPage(); return;
    }
    const auto& poly = m_board->wires[(size_t)id];

    m_pg->Append(new wxPropertyCategory("General"));
    auto* pid = m_pg->Append(new wxIntProperty("Index", "idx", (int)id));
    pid->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);

    auto* pvc = m_pg->Append(new wxIntProperty("Vertex Count", "vcount", (int)poly.size()));
    pvc->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);

    if (!poly.empty()) {
        m_pg->Append(new wxPropertyCategory("Endpoints (Read-only)"));
        const wxPoint& p0 = poly.front();
        const wxPoint& p1 = poly.back();

        auto* psx = m_pg->Append(new wxIntProperty("Start X", "sx", p0.x));
        auto* psy = m_pg->Append(new wxIntProperty("Start Y", "sy", p0.y));
        auto* pex = m_pg->Append(new wxIntProperty("End  X", "ex", p1.x));
        auto* pey = m_pg->Append(new wxIntProperty("End  Y", "ey", p1.y));

        psx->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
        psy->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
        pex->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
        pey->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
    }
}

void PropertyPane::OnPropChanged(wxPropertyGridEvent& e) {
    const wxString key = e.GetPropertyName();
    const wxVariant v = e.GetPropertyValue();

    switch (m_board->GetSelectionKind()) {
    case SelKind::Gate: {
        const int idx = (int)m_board->GetSelectionId();
        if (idx < 0 || idx >= (int)m_board->components.size()) return;
        auto* c = m_board->components[(size_t)idx].get();

        if (key == "pos_x" || key == "pos_y") {
            wxPoint center = c->GetCenter();
            if (key == "pos_x") center.x = v.GetInteger();
            else                center.y = v.GetInteger();
            c->SetCenter(center);
            c->UpdateGeometry();
            m_board->Refresh(false);
        }
        // else if (key == "scale") { c->scale = v.GetDouble(); c->UpdateGeometry(); m_board->Refresh(false); }
        break;
    }
    case SelKind::Wire:
        // 目前线没有可写属性
        break;
    default:
        break;
    }
}
