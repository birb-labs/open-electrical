#include "ui/MainPalette.h"
#include "ui/Localization.h"
#include "services/ProjectContext.h"
#include "utilities/StringUtil.h"

#include <wx/statline.h>

namespace electrical {
namespace ui {

static wxString W(const std::string& k) { return wxString(EL_TRW(k).c_str()); }

MainPalettePanel::MainPalettePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, W("palette.title"));
    wxFont f = title->GetFont(); f.MakeBold(); f.SetPointSize(f.GetPointSize() + 1);
    title->SetFont(f);
    root->Add(title, 0, wxALL, 8);

    summary_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    root->Add(summary_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 4);

    // Setup.
    addButton(root, "menu.config", "EL-CONFIG");
    addButton(root, "menu.rooms",  "EL-ROOM");
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 4);

    // Elements.
    auto* grid = new wxGridSizer(2, 4, 4);
    const struct { const char* label; wxString cmd; } elemBtns[] = {
        { "Light",  "EL-LIGHT"  }, { "Outlet", "EL-OUTLET" },
        { "Switch", "EL-SWITCH" }, { "Panel",  "EL-PANEL"  },
    };
    for (const auto& e : elemBtns) {
        auto* btn = new wxButton(this, wxID_ANY, e.label);
        const wxString cmd = e.cmd;
        btn->Bind(wxEVT_BUTTON, [this, cmd](wxCommandEvent&) { run(cmd); });
        grid->Add(btn, 0, wxEXPAND);
    }
    root->Add(grid, 0, wxEXPAND | wxALL, 6);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 4);

    // Distribution / routing / reports.
    addButton(root, "menu.circuits", "EL-CIRCUIT-AUTO");
    addButton(root, "config.conduitMat", "EL-CONDUIT-AUTO");
    addButton(root, "menu.reports",  "EL-REPORT");

    SetSizer(root);
    refresh();
}

wxButton* MainPalettePanel::addButton(wxSizer* sizer, const std::string& labelKey,
                                      const wxString& command) {
    auto* b = new wxButton(this, wxID_ANY, W(labelKey));
    b->Bind(wxEVT_BUTTON, [this, command](wxCommandEvent&) { run(command); });
    sizer->Add(b, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    return b;
}

void MainPalettePanel::run(const wxString& command) {
    // Queue the command on the active document so it runs in document context.
    const std::wstring line = command.ToStdWstring() + L" ";
    if (acDocManager && acDocManager->curDocument())
        acDocManager->sendStringToExecute(acDocManager->curDocument(),
                                          line.c_str(), false, true, false);
}

void MainPalettePanel::refresh() {
    ProjectData& p = ProjectContext::instance().project();
    summary_->SetLabel(wxString::Format(
        "Rooms: %d   Elements: %d   Circuits: %d",
        static_cast<int>(p.rooms.size()),
        static_cast<int>(p.elements.size()),
        static_cast<int>(p.circuits.size())));
    Layout();
}

} // namespace ui
} // namespace electrical
