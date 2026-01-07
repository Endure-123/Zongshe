// PropertyPane.cpp
#include "PropertyPane.h"
#include "DrawBoard.h"
#include <algorithm>
#include "Simulator.h"

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
    // ★ 优化：在重建前先取消绑定，防止重建过程触发不必要的 OnPropChanged
    Unbind(wxEVT_PG_CHANGED, &PropertyPane::OnPropChanged, this);

    switch (m_board->GetSelectionKind()) {
    case SelKind::Gate: BuildGatePage(m_board->GetSelectionId()); break;
    case SelKind::Wire: BuildWirePage(m_board->GetSelectionId()); break;
    default:            BuildEmptyPage(); break;
    }

    // ★ 优化：重建后再绑定
    Bind(wxEVT_PG_CHANGED, &PropertyPane::OnPropChanged, this);
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
    hint->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
    m_pg->Append(hint);
}

void PropertyPane::BuildGatePage(long id) {
    m_pg->Clear();
    m_pg->AddPage("Gate");

    if (id < 0 || id >= (long)m_board->components.size()) {
        BuildEmptyPage();
        return;
    }
    auto* c = m_board->components[(size_t)id].get();
    if (!c) return;

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

    // ========== ★ 优化：节点电平状态 ==========
    if (m_board->IsSimulating() && (c->m_type == ComponentType::NODE_START || c->m_type == ComponentType::NODE_END))
    {
        m_pg->Append(new wxPropertyCategory("Signal"));

        // ★ 优化：起始节点 (START_NODE) 改为可写的布尔值 (复选框)
        if (c->m_type == ComponentType::NODE_START) {
            bool val = m_board->m_sim->GetStartNodeValue(id);
            // "start_val" 是 key，"Value" 是显示名称
            auto* pVal = m_pg->Append(new wxBoolProperty("Value", "start_val", val));
            // 设置为复选框样式
            pVal->SetAttribute(wxPG_BOOL_USE_CHECKBOX, true);
        }
        // 终止节点 (END_NODE) 保持只读
        else if (c->m_type == ComponentType::NODE_END) {
            bool val = false;
            // 逻辑不变：查找连接到此节点的网络的电平
            for (const auto& net : m_board->m_sim->m_nets) {
                for (const auto& ld : net.loads) {
                    if (ld.compIdx == id) {
                        val = net.value;
                        break; // 找到一个即可
                    }
                }
            }
            auto* plevel = m_pg->Append(new wxStringProperty("Current Level", "logic_level",
                val ? "1 (High)" : "0 (Low)"));
            plevel->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
        }
    }
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

    // ========== ★ 优化：显示导线的逻辑电平 ==========
    m_pg->Append(new wxPropertyCategory("Simulation"));
    wxString levelStr = "N/A (Stopped)";

    if (m_board->IsSimulating() && m_board->m_sim) {
        // 使用 IsWireHigh (已在上一轮修复)
        bool level = m_board->m_sim->IsWireHigh(id);
        levelStr = level ? "1 (High)" : "0 (Low)";
    }

    auto* plevel = m_pg->Append(new wxStringProperty("Logic Level", "logic_level", levelStr));
    plevel->ChangeFlag(wxPGFlags(wxPG_PROP_READONLY), true);
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

            // ★ TODO: 为支持 Undo/Redo，此处应使用 Command 模式
            // 示例 (需要修改 cMain 和 DrawBoard 以传递 m_cmdMgr):
            // wxPoint oldPos = c->GetCenter();
            // if (m_cmd && oldPos != center) {
            //    m_cmd->Execute(std::make_unique<MoveGateCmd>(m_board, idx, oldPos, center));
            // } 

            // 当前的直接修改 (无 Undo):
            c->SetCenter(center);
            c->UpdateGeometry();
            m_board->Refresh(false);
        }
        // ========== ★ 优化：处理 START_NODE 值变化 ==========
        else if (key == "start_val") {
            if (c->m_type == ComponentType::NODE_START && m_board->IsSimulating() && m_board->m_sim) {
                bool newVal = v.GetBool();

                // ★ TODO: 此处也应使用 Command 模式
                // 示例: m_cmd->Execute(std::make_unique<SetNodeValueCmd>(...));

                // 当前的直接修改 (无 Undo):
                m_board->m_sim->SetStartNodeValue(idx, newVal);
                m_board->SimStep(); // 立即触发一次 Settle
                m_board->Refresh(false);
            }
        }
        break;
    }
    case SelKind::Wire:
        // 目前线没有可写属性
        break;
    default:
        break;
    }
}