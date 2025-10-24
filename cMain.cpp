#include "cMain.h"
#include "AppConfig.h"
#include "ResourceManager.h"
#include <vector>

// ★ 属性面板与选择事件
#include "PropertyPane.h"
#include "SelectionEvents.h"

wxBEGIN_EVENT_TABLE(cMain, wxFrame)
EVT_MENU(wxID_EXIT, cMain::OnExit)
EVT_MENU(2001, cMain::OnClearTexts)
EVT_MENU(2002, cMain::OnClearPics)
wxEND_EVENT_TABLE()

wxBitmap cMain::LoadToolBitmap(const wxString & filename, int size)
{
    return ResourceManager::LoadBitmap(IMG_DIR, filename, size);
}

cMain::cMain()
    : wxFrame(nullptr, wxID_ANY, "Drawing App", wxDefaultPosition, wxSize(1200, 700))
{
    // 菜单栏
    menuBar = new wxMenuBar();
    fileMenu = new wxMenu();
    fileMenu->Append(ID_SaveJSON, "Save JSON\tCtrl+S");
    fileMenu->Append(ID_LoadJSON, "Load JSON\tCtrl+O");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt+F4");
    menuBar->Append(fileMenu, "&File");

    editMenu = new wxMenu();
    editMenu->Append(2001, "Clear &Texts");
    editMenu->Append(2002, "Clear &Lines");
    menuBar->Append(editMenu, "&Edit");

    SetMenuBar(menuBar);

    // 工具栏
    m_toolBar = CreateToolBar();
    m_toolBar->AddRadioTool(ID_TOOL_ARROW, "Arrow", LoadToolBitmap("Arrow"));
    m_toolBar->AddRadioTool(ID_TOOL_TEXT, "Text", LoadToolBitmap("BigA"));
    m_toolBar->AddRadioTool(ID_TOOL_HAND, "Hand", LoadToolBitmap("hands"));
    m_toolBar->AddTool(ID_TOOL_DELETE, "Delete", LoadToolBitmap("delete"));
    m_toolBar->ToggleTool(ID_TOOL_ARROW, true);
    m_toolBar->Realize();

    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_ARROW);
    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_TEXT);
    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_HAND);
    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_DELETE);

    // 绑定 Save/Load JSON 菜单
    Bind(wxEVT_MENU, &cMain::OnSaveJson, this, ID_SaveJSON);
    Bind(wxEVT_MENU, &cMain::OnLoadJson, this, ID_LoadJSON);

    // ===================== 主体区域：左(树+属性) | 右(画布) =====================
    // 外层左右分割：左侧容器 + 右侧画布
    m_splitter = new wxSplitterWindow(this, wxID_ANY);
    m_splitter->SetMinimumPaneSize(200);

    // --- 右侧：绘图画布 ---
    drawBoard = new DrawBoard(m_splitter);
    drawBoard->pSelectedGateName = &selectedGateName;

    // --- 左侧：面板容器，内部再做上下分割（上资源树 / 下属性面板） ---
    m_leftPanel = new wxPanel(m_splitter, wxID_ANY);
    auto* leftRootSizer = new wxBoxSizer(wxVERTICAL);

    // 左侧内部上下分割器
    auto* leftSplit = new wxSplitterWindow(m_leftPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxSP_3D | wxSP_LIVE_UPDATE);
    leftSplit->SetMinimumPaneSize(120);
    leftSplit->SetSashGravity(1.0);      // 缩放时优先让上半区伸缩，底部属性保持高度

    // 上半：资源树面板
    auto* treePanel = new wxPanel(leftSplit, wxID_ANY);
    auto* treeSizer = new wxBoxSizer(wxVERTICAL);

    // 资源树
    std::vector<wxString> gateNames;
    wxImageList* imgList = ResourceManager::LoadImageList(IMG_DIR, gateNames);

    m_treeCtrl = new wxTreeCtrl(treePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
    wxTreeItemId root = m_treeCtrl->AddRoot("Gates", 0);
    for (size_t i = 0; i < gateNames.size(); ++i) {
        m_treeCtrl->AppendItem(root, gateNames[i], static_cast<int>(i + 1));
    }
    m_treeCtrl->AssignImageList(imgList);
    m_treeCtrl->Expand(root);

    treeSizer->Add(m_treeCtrl, 1, wxEXPAND | wxALL, 2);
    treePanel->SetSizer(treeSizer);

    // 下半：属性面板（宽度天然与左侧一致）
    m_prop = new PropertyPane(leftSplit, drawBoard);

    // 从底部起固定 260 像素给属性面板
    leftSplit->SplitHorizontally(treePanel, m_prop, -260);

    // 左侧容器装入上下分割器
    leftRootSizer->Add(leftSplit, 1, wxEXPAND);
    m_leftPanel->SetSizer(leftRootSizer);

    // 最外层左右分割：左侧(树+属性) | 右侧(画布)
    m_splitter->SplitVertically(m_leftPanel, drawBoard, 300);

    // ===================== （可选）使用 AUI 托管 Center =====================
    // 如果你的工程已在用 AUI，这里仅让它接管整个左右分割窗口即可。
    m_aui.SetManagedWindow(this);
    m_aui.AddPane(m_splitter, wxAuiPaneInfo().CenterPane());
    m_aui.Update();

    // 资源树事件
    m_treeCtrl->Bind(wxEVT_TREE_ITEM_ACTIVATED, &cMain::OnTreeItemActivated, this);
    m_treeCtrl->Bind(wxEVT_TREE_SEL_CHANGED, &cMain::OnTreeSelChanged, this);

    // DrawBoard 选择变化 → 刷新属性面板
    Bind(EVT_SELECTION_CHANGED, [this](wxCommandEvent&) {
        if (m_prop) m_prop->RebuildBySelection();
        });

    CreateStatusBar();
    SetStatusText("Ready - 当前工具: Arrow");

    // 初始化为空态页面
    if (m_prop) m_prop->RebuildBySelection();
}

cMain::~cMain()
{
    // 释放 AUI 管理器
    m_aui.UnInit();
}

void cMain::OnExit(wxCommandEvent& evt) { Close(true); }
void cMain::OnClearTexts(wxCommandEvent& evt) { drawBoard->ClearTexts(); }
void cMain::OnClearPics(wxCommandEvent& evt) { drawBoard->ClearPics(); }

void cMain::OnToolSelected(wxCommandEvent& evt)
{
    int id = evt.GetId();
    if (id == ID_TOOL_ARROW) {
        selectedGateName.Clear();
        drawBoard->pSelectedGateName->Clear();
        drawBoard->currentTool = ID_TOOL_ARROW;
        drawBoard->isDrawingLine = false;
        drawBoard->isInsertingText = false;
        drawBoard->SetCursor(wxCURSOR_ARROW);
        SetStatusText("当前工具: Arrow (选择)");
    }
    else if (id == ID_TOOL_TEXT) {
        selectedGateName.Clear();
        drawBoard->currentTool = ID_TOOL_TEXT;
        drawBoard->isDrawingLine = false;
        drawBoard->isInsertingText = true;
        drawBoard->SetCursor(wxCURSOR_IBEAM);
        SetStatusText("当前工具: Text (插入文本)");
    }
    else if (id == ID_TOOL_HAND) {
        selectedGateName.Clear();
        drawBoard->currentTool = ID_TOOL_HAND;
        drawBoard->isDrawingLine = true;
        drawBoard->isInsertingText = false;
        drawBoard->SetCursor(wxCURSOR_HAND);
        SetStatusText("当前工具: Hand (画线)");
    }
    else if (id == ID_TOOL_DELETE) {
        drawBoard->DeleteSelection();
        selectedGateName.Clear();
        drawBoard->pSelectedGateName->Clear();
        SetStatusText("已删除选中的对象");
        m_toolBar->ToggleTool(ID_TOOL_ARROW, true);
        drawBoard->currentTool = ID_TOOL_ARROW;
        drawBoard->SetCursor(wxCURSOR_ARROW);
    }
}

void cMain::SetSelectedGate(const wxString& label)
{
    if (label == "Gates") {
        selectedGateName.Clear();
        SetStatusText("这是分类节点，不是元件。");
        return;
    }
    selectedGateName = label;
    m_toolBar->ToggleTool(ID_TOOL_ARROW, false);
    SetStatusText("已选择元件: " + label + "，请在画布点击放置。");
}

void cMain::OnTreeItemActivated(wxTreeEvent& event)
{
    SetSelectedGate(m_treeCtrl->GetItemText(event.GetItem()));
}

void cMain::OnTreeSelChanged(wxTreeEvent& event)
{
    SetSelectedGate(m_treeCtrl->GetItemText(event.GetItem()));
}

// ---------------- JSON 保存/加载 ----------------
void cMain::OnSaveJson(wxCommandEvent& evt)
{
    wxFileDialog dlg(this, "保存为 JSON", "", "", "JSON files (*.json)|*.json",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;
    drawBoard->SaveToJson(std::string(dlg.GetPath().mb_str()));
}

void cMain::OnLoadJson(wxCommandEvent& evt)
{
    wxFileDialog dlg(this, "加载 JSON", "", "", "JSON files (*.json)|*.json",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;
    drawBoard->LoadFromJson(std::string(dlg.GetPath().mb_str()));
}
