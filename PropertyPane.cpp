#include "PropertyPane.h"

#include "Commands.h"

#include <cmath>
#include <memory>

#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/property.h>
#include <wx/sizer.h>
#include <wx/valnum.h>

wxBEGIN_EVENT_TABLE(PropertyPane, wxPanel)
    EVT_PG_CHANGED(wxID_ANY, PropertyPane::OnPropertyChanged)
wxEND_EVENT_TABLE()

PropertyPane::PropertyPane(wxWindow *parent, BoardModel &board, CommandManager &commands)
    : wxPanel(parent), m_board(board), m_commands(commands) {
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    m_propertyGrid = new wxPropertyGridManager(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                              wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER);
    sizer->Add(m_propertyGrid, 1, wxEXPAND);
    SetSizer(sizer);

    m_listenerToken = m_board.AddListener([this]() { OnBoardChanged(); });
    OnBoardChanged();
}

PropertyPane::~PropertyPane() { m_board.RemoveListener(m_listenerToken); }

void PropertyPane::OnBoardChanged() { RefreshProperties(); }

void PropertyPane::RefreshProperties() {
    m_updating = true;
    m_propertyGrid->Clear();
    m_propertyKeys.clear();

    const ComponentData *component = m_board.GetSelectedComponent();
    if (!component) {
        m_updating = false;
        return;
    }

    m_propertyGrid->Append(new wxPropertyCategory(component->id));
    auto *typeProperty = new wxStringProperty("Type", wxPG_LABEL, component->type);
    typeProperty->ChangeFlag(wxPGPropertyFlags::ReadOnly, true);
    m_propertyGrid->Append(typeProperty);

    auto *xProperty = new wxFloatProperty("Position X", wxPG_LABEL, component->position.x);
    m_propertyGrid->Append(xProperty);
    auto *yProperty = new wxFloatProperty("Position Y", wxPG_LABEL, component->position.y);
    m_propertyGrid->Append(yProperty);

    if (!component->properties.empty()) {
        m_propertyGrid->Append(new wxPropertyCategory("Properties"));
        for (const auto &entry : component->properties) {
            auto *prop = new wxStringProperty(entry.first, wxPG_LABEL, entry.second);
            m_propertyKeys[prop] = entry.first;
            m_propertyGrid->Append(prop);
        }
    }

    m_updating = false;
}

void PropertyPane::OnPropertyChanged(wxPropertyGridEvent &event) {
    if (m_updating) {
        return;
    }

    wxPGProperty *property = event.GetProperty();
    if (!property) {
        return;
    }

    ComponentData *component = m_board.GetSelectedComponent();
    if (!component) {
        return;
    }

    const wxString name = property->GetLabel();
    if (name == "Position X" || name == "Position Y") {
        BoardPoint newPosition = component->position;
        if (name == "Position X") {
            newPosition.x = property->GetValue().GetDouble();
        } else {
            newPosition.y = property->GetValue().GetDouble();
        }

        if (std::abs(newPosition.x - component->position.x) > 0.01 ||
            std::abs(newPosition.y - component->position.y) > 0.01) {
            m_commands.Execute(std::make_unique<MoveComponentCommand>(m_board, component->id, component->position,
                                                                      newPosition));
        }
        return;
    }

    auto it = m_propertyKeys.find(property);
    if (it == m_propertyKeys.end()) {
        return;
    }

    const std::string &key = it->second;
    const std::string newValue = property->GetValueAsString().ToStdString();
    m_commands.Execute(std::make_unique<UpdatePropertyCommand>(m_board, component->id, key, newValue));
}
