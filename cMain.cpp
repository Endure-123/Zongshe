#include "cMain.h"
#include "AppConfig.h"
#include "ResourceManager.h"
#include <vector>

// ★ 新增：对话框/消息框/文件系统
#include <wx/dir.h>
#include <wx/msgdlg.h>
#include <filesystem>

// ★ 属性面板与选择事件
#include "PropertyPane.h"
#include "SelectionEvents.h"

// ★ 新增：导出菜单的 ID（也会在 cMain.h 里补一个同名 ID）
#ifndef ID_Menu_ExportBookShelf
#define ID_Menu_ExportBookShelf (wxID_HIGHEST + 1001)
#endif

wxBEGIN_EVENT_TABLE(cMain, wxFrame)
EVT_MENU(wxID_EXIT, cMain::OnExit)
EVT_MENU(wxID_UNDO, cMain::OnUndo)
EVT_MENU(wxID_REDO, cMain::OnRedo)
EVT_MENU(2001, cMain::OnClearTexts)
EVT_MENU(2002, cMain::OnClearPics)
wxEND_EVENT_TABLE()

wxBitmap cMain::LoadToolBitmap(const wxString& filename, int size)
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

    // ★ 新增：Export as BookShelf… 菜单项（快捷键 Ctrl+E）
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_Menu_ExportBookShelf,
        "Export as &BookShelf...\tCtrl+E",
        "Export current design to BookShelf (.nodes/.nets)");

    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt+F4");
    menuBar->Append(fileMenu, "&File");

    fileMenu->Append(ID_Menu_ImportBookShelf,
        "Import &BookShelf...\tCtrl+I",
        "Import from BookShelf (.nodes + .nets)");
    Bind(wxEVT_MENU, &cMain::OnImportBookShelf, this, ID_Menu_ImportBookShelf);


    editMenu = new wxMenu();
    editMenu->Append(2001, "Clear &Texts");
    editMenu->Append(2002, "Clear &Lines");
    editMenu->Prepend(wxID_REDO, "重做\tCtrl+Y");
    editMenu->Prepend(wxID_UNDO, "撤销\tCtrl+Z");
    menuBar->Append(editMenu, "&Edit");

    SetMenuBar(menuBar);

    // 工具栏
    m_toolBar = CreateToolBar();
    m_toolBar->AddRadioTool(ID_TOOL_ARROW, "Arrow", LoadToolBitmap("Arrow"));
    m_toolBar->AddRadioTool(ID_TOOL_TEXT, "Text", LoadToolBitmap("BigA"));
    m_toolBar->AddRadioTool(ID_TOOL_HAND, "Hand", LoadToolBitmap("hands"));
    m_toolBar->AddTool(ID_TOOL_DELETE, "Delete", LoadToolBitmap("delete"));
    m_toolBar->AddTool(wxID_UNDO, "Undo", LoadToolBitmap("Undo"));
    m_toolBar->AddTool(wxID_REDO, "Redo", LoadToolBitmap("Redo"));
    m_toolBar->AddTool(ID_TOOL_ZOOMIN, "Zoom In", LoadToolBitmap("ZoomIn"));
    m_toolBar->AddTool(ID_TOOL_ZOOMOUT, "Zoom Out", LoadToolBitmap("ZoomOut"));
    m_toolBar->ToggleTool(ID_TOOL_ARROW, true);
    m_toolBar->Realize();

    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_ARROW);
    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_TEXT);
    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_HAND);
    Bind(wxEVT_TOOL, &cMain::OnToolSelected, this, ID_TOOL_DELETE);
    Bind(wxEVT_TOOL, &cMain::OnUndo, this, wxID_UNDO);
    Bind(wxEVT_TOOL, &cMain::OnRedo, this, wxID_REDO);
    Bind(wxEVT_TOOL, &cMain::OnZoomIn, this, ID_TOOL_ZOOMIN);
    Bind(wxEVT_TOOL, &cMain::OnZoomOut, this, ID_TOOL_ZOOMOUT);

    // 绑定 Save/Load JSON 菜单
    Bind(wxEVT_MENU, &cMain::OnSaveJson, this, ID_SaveJSON);
    Bind(wxEVT_MENU, &cMain::OnLoadJson, this, ID_LoadJSON);

    // ★ 新增：绑定导出菜单
    Bind(wxEVT_MENU, &cMain::OnExportBookShelf, this, ID_Menu_ExportBookShelf);

    // ===================== 主体区域：左(树+属性) | 右(画布) =====================
    // 外层左右分割：左侧容器 + 右侧画布
    m_splitter = new wxSplitterWindow(this, wxID_ANY);
    m_splitter->SetMinimumPaneSize(200);

    // --- 右侧：绘图画布 ---
    drawBoard = new DrawBoard(m_splitter);
    drawBoard->SetCommandManager(&m_cmdMgr);  // ★ 交给画布

    // 撤销/重做状态变化时更新工具栏按钮可用状态
    m_cmdMgr.SetOnChanged([this](bool canUndo, bool canRedo) {
        UpdateUndoRedoUI(canUndo, canRedo);
        });

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

    // 树控件：纯文字
    m_treeCtrl = new wxTreeCtrl(
        treePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE
    );

    // 根节点（可见根更直观）
    wxTreeItemId root = m_treeCtrl->AddRoot("资源");

    // 01 基础逻辑门
    wxTreeItemId catBasic = m_treeCtrl->AppendItem(root, "01 基础逻辑门");
    m_treeCtrl->AppendItem(catBasic, "与门");   // AND
    m_treeCtrl->AppendItem(catBasic, "非门");   // NOT
    m_treeCtrl->AppendItem(catBasic, "或门");   // OR

    // 02 复合逻辑门
    wxTreeItemId catCombo = m_treeCtrl->AppendItem(root, "02 复合逻辑门");
    m_treeCtrl->AppendItem(catCombo, "与非门"); // NAND
    m_treeCtrl->AppendItem(catCombo, "或非门"); // NOR
    m_treeCtrl->AppendItem(catCombo, "异或门"); // XOR
    m_treeCtrl->AppendItem(catCombo, "同或门"); // XNOR

    // 03 结点
    wxTreeItemId catNodes = m_treeCtrl->AppendItem(root, "03 结点");
    m_treeCtrl->AppendItem(catNodes, "普通结点");
    m_treeCtrl->AppendItem(catNodes, "起始节点");
    m_treeCtrl->AppendItem(catNodes, "终止节点");

    // 04 扩展元器件
    wxTreeItemId catExt = m_treeCtrl->AppendItem(root, "04 扩展元器件");
    m_treeCtrl->AppendItem(catExt, "3-8译码器");
    m_treeCtrl->AppendItem(catExt, "2-4译码器");

    // 全部展开
    m_treeCtrl->ExpandAll();

    // 加入布局
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
    // 分类节点：不选择
    static const wxArrayString kCategories = {
        "资源", "01 基础逻辑门", "02 复合逻辑门", "03 结点", "04 扩展元器件"
    };
    if (kCategories.Index(label) != wxNOT_FOUND) {
        selectedGateName.Clear();
        SetStatusText("这是分类节点。");
        return;
    }

    // 中文 → 内部元件名
    wxString internal;
    if (label == "与门")       internal = "AND";
    else if (label == "或门")       internal = "OR";
    else if (label == "非门")       internal = "NOT";
    else if (label == "与非门")     internal = "NAND";
    else if (label == "或非门")     internal = "NOR";         // ★ 新增
    else if (label == "异或门")     internal = "XOR";         // ★ 新增
    else if (label == "同或门")     internal = "XNOR";        // ★ 新增
    else if (label == "普通结点")   internal = "NODE";
    else if (label == "起始节点")   internal = "START_NODE";
    else if (label == "终止节点")   internal = "END_NODE";
    else if (label == "3-8译码器")  internal = "DECODER38";   // ★ 新增
    else if (label == "2-4译码器")  internal = "DECODER24";   // ★ 新增
    else {
        selectedGateName.Clear();
        drawBoard->pSelectedGateName->Clear();
        SetStatusText("未知条目：" + label);
        return;
    }

    selectedGateName = internal;
    *drawBoard->pSelectedGateName = internal;
    m_toolBar->ToggleTool(ID_TOOL_ARROW, false);
    SetStatusText("已选择元件：" + label + "（" + internal + "），请在画布点击放置。");
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

void cMain::UpdateUndoRedoUI(bool canUndo, bool canRedo) {
    if (!m_toolBar) return;
    m_toolBar->EnableTool(wxID_UNDO, canUndo);
    m_toolBar->EnableTool(wxID_REDO, canRedo);
}

void cMain::OnUndo(wxCommandEvent&) { m_cmdMgr.Undo(); }
void cMain::OnRedo(wxCommandEvent&) { m_cmdMgr.Redo(); }

void cMain::OnZoomIn(wxCommandEvent&) { if (drawBoard) drawBoard->ZoomInCenter(); }
void cMain::OnZoomOut(wxCommandEvent&) { if (drawBoard) drawBoard->ZoomOutCenter(); }

// ★ 新增：导出为 BookShelf 的菜单处理函数
void cMain::OnExportBookShelf(wxCommandEvent&)
{
    if (!drawBoard) {
        wxMessageBox("DrawBoard is not ready.", "Export", wxOK | wxICON_ERROR, this);
        return;
    }

    // 选择输出目录（默认 MyOwn/output）
    wxDirDialog dlg(this, "Choose output folder",
        "MyOwn/output",
        // 如果希望在对话框中可以新建目录，可以去掉 wxDD_DIR_MUST_EXIST
        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

    if (dlg.ShowModal() != wxID_OK) return;

    std::filesystem::path outDir = dlg.GetPath().ToStdWstring();

    // 兜底：如果不存在就创建
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    const bool ok = drawBoard->ExportAsBookShelf("MyOwn", outDir);
    if (ok) {
        wxMessageBox("Exported successfully to:\n" + dlg.GetPath(),
            "Export", wxOK | wxICON_INFORMATION, this);
    }
    else {
        wxMessageBox("Export failed.", "Export", wxOK | wxICON_ERROR, this);
    }
}

void cMain::OnImportBookShelf(wxCommandEvent&)
{
    if (!drawBoard) {
        wxMessageBox("DrawBoard is not ready.", "Import", wxOK | wxICON_ERROR, this);
        return;
    }

    // 选 .nodes
    wxFileDialog dlgNodes(this, "Choose .nodes", "", "",
        "BookShelf nodes (*.nodes)|*.nodes|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlgNodes.ShowModal() != wxID_OK) return;

    // 选 .nets（默认同目录）
    wxFileDialog dlgNets(this, "Choose .nets", dlgNodes.GetDirectory(), "",
        "BookShelf nets (*.nets)|*.nets|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlgNets.ShowModal() != wxID_OK) return;

    std::filesystem::path nodesPath = dlgNodes.GetPath().ToStdWstring();
    std::filesystem::path netsPath = dlgNets.GetPath().ToStdWstring();

    if (drawBoard->ImportBookShelf(nodesPath, netsPath)) {
        wxMessageBox("Imported successfully.", "Import", wxOK | wxICON_INFORMATION, this);
    }
    else {
        wxMessageBox("Import failed.", "Import", wxOK | wxICON_ERROR, this);
    }
}
