#include "MainFrame.h"

#include "BoardSerializer.h"
#include "Commands.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>

#include <wx/artprov.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/toolbar.h>

namespace {
constexpr int kInitialComponentOffset = 50;
constexpr int kComponentSpacing = 40;
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Zongshe Board Editor", wxDefaultPosition, wxSize(1200, 800)) {
    m_commands.SetOnStackChanged([this]() { UpdateUndoRedoUI(); UpdateTitle(); });
    m_board.AddListener([this]() { UpdateUndoRedoUI(); UpdateTitle(); });

    CreateMenus();
    CreateToolbar();

    auto *splitter = new wxSplitterWindow(this);
    m_propertyPane = new PropertyPane(splitter, m_board, m_commands);
    m_drawBoard = new DrawBoard(splitter, m_board, m_commands);
    splitter->SplitVertically(m_propertyPane, m_drawBoard, 320);
    splitter->SetMinimumPaneSize(220);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(splitter, 1, wxEXPAND);
    SetSizer(sizer);

    BindEvents();
    UpdateUndoRedoUI();
    UpdateTitle();
}

void MainFrame::CreateMenus() {
    auto *fileMenu = new wxMenu();
    fileMenu->Append(wxID_NEW, "新建(&N)");
    fileMenu->Append(ID_OpenBoard, "打开 JSON...(&O)");
    fileMenu->Append(ID_SaveBoard, "保存 JSON...(&S)");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "退出(&Q)");

    auto *editMenu = new wxMenu();
    editMenu->Append(wxID_UNDO, "撤销(&U)");
    editMenu->Append(wxID_REDO, "重做(&R)");
    editMenu->AppendSeparator();
    editMenu->Append(ID_AddComponent, "添加元件(&A)");
    editMenu->Append(ID_DeleteComponent, "删除元件(&D)");

    auto *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "文件(&F)");
    menuBar->Append(editMenu, "编辑(&E)");
    SetMenuBar(menuBar);
}

void MainFrame::CreateToolbar() {
    m_toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
    m_toolBar->AddTool(wxID_NEW, "新建", m_resources.GetBitmap("select.png"));
    m_toolBar->AddTool(ID_OpenBoard, "打开", m_resources.GetBitmap("Arrow.png"));
    m_toolBar->AddTool(ID_SaveBoard, "保存", m_resources.GetBitmap("BigA.png"));
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(wxID_UNDO, "撤销", m_resources.GetBitmap("Undo.png"));
    m_toolBar->AddTool(wxID_REDO, "重做", m_resources.GetBitmap("Redo.png"));
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(ID_AddComponent, "添加", m_resources.GetBitmap("drawLine.png"));
    m_toolBar->AddTool(ID_DeleteComponent, "删除", m_resources.GetBitmap("delete.png"));
    m_toolBar->Realize();
}

void MainFrame::BindEvents() {
    Bind(wxEVT_MENU, &MainFrame::OnNewBoard, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnOpenBoard, this, ID_OpenBoard);
    Bind(wxEVT_MENU, &MainFrame::OnSaveBoard, this, ID_SaveBoard);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);

    Bind(wxEVT_MENU, &MainFrame::OnAddComponent, this, ID_AddComponent);
    Bind(wxEVT_MENU, &MainFrame::OnDeleteComponent, this, ID_DeleteComponent);
    Bind(wxEVT_MENU, &MainFrame::OnUndo, this, wxID_UNDO);
    Bind(wxEVT_MENU, &MainFrame::OnRedo, this, wxID_REDO);
}

void MainFrame::OnNewBoard(wxCommandEvent &) {
    m_commands.Clear();
    m_board.Clear();
    m_componentCounter = 0;
}

void MainFrame::OnOpenBoard(wxCommandEvent &) {
    wxFileDialog dialog(this, "打开 JSON 文件", wxEmptyString, wxEmptyString, "JSON files (*.json)|*.json|All files|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    try {
        BoardSerializer::LoadFromFile(dialog.GetPath().ToStdString(), m_board);
        m_commands.Clear();
        UpdateIdCounterFromBoard();
    } catch (const std::exception &ex) {
        wxMessageBox(wxString::FromUTF8(ex.what()), "打开失败", wxICON_ERROR | wxOK, this);
    }
}

void MainFrame::OnSaveBoard(wxCommandEvent &) {
    wxFileDialog dialog(this, "保存为 JSON", wxEmptyString, wxEmptyString, "JSON files (*.json)|*.json",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    try {
        BoardSerializer::SaveToFile(m_board, dialog.GetPath().ToStdString());
    } catch (const std::exception &ex) {
        wxMessageBox(wxString::FromUTF8(ex.what()), "保存失败", wxICON_ERROR | wxOK, this);
    }
}

void MainFrame::OnExit(wxCommandEvent &) { Close(true); }

void MainFrame::OnAddComponent(wxCommandEvent &) {
    ComponentData data;
    data.id = GenerateComponentId();
    data.type = "Gate";
    const int offset = kInitialComponentOffset + (m_componentCounter % 10) * kComponentSpacing;
    data.position = BoardPoint{static_cast<double>(offset), static_cast<double>(offset)};
    data.properties["label"] = data.id;

    m_commands.Execute(std::make_unique<AddComponentCommand>(m_board, data));
}

void MainFrame::OnDeleteComponent(wxCommandEvent &) {
    const ComponentData *component = m_board.GetSelectedComponent();
    if (!component) {
        return;
    }

    m_commands.Execute(std::make_unique<RemoveComponentCommand>(m_board, component->id));
}

void MainFrame::OnUndo(wxCommandEvent &) { m_commands.Undo(); }

void MainFrame::OnRedo(wxCommandEvent &) { m_commands.Redo(); }

void MainFrame::UpdateUndoRedoUI() {
    if (m_toolBar) {
        m_toolBar->EnableTool(wxID_UNDO, m_commands.CanUndo());
        m_toolBar->EnableTool(wxID_REDO, m_commands.CanRedo());
        m_toolBar->EnableTool(ID_DeleteComponent, m_board.GetSelectedComponent() != nullptr);
    }

    if (wxMenuBar *bar = GetMenuBar()) {
        if (wxMenu *edit = bar->GetMenu(1)) {
            edit->Enable(wxID_UNDO, m_commands.CanUndo());
            edit->Enable(wxID_REDO, m_commands.CanRedo());
            edit->Enable(ID_DeleteComponent, m_board.GetSelectedComponent() != nullptr);
        }
    }
}

void MainFrame::UpdateTitle() {
    const size_t count = m_board.GetComponents().size();
    SetTitle(wxString::Format("Zongshe Board Editor - %zu component(s)", count));
}

std::string MainFrame::GenerateComponentId() {
    ++m_componentCounter;
    return "component_" + std::to_string(m_componentCounter);
}

void MainFrame::UpdateIdCounterFromBoard() {
    int maxId = 0;
    for (const auto &component : m_board.GetComponents()) {
        const std::string &id = component.id;
        auto pos = id.find_last_of('_');
        if (pos != std::string::npos) {
            const std::string number = id.substr(pos + 1);
            if (!number.empty() && std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c); })) {
                maxId = std::max(maxId, std::stoi(number));
            }
        }
    }
    m_componentCounter = maxId;
}
