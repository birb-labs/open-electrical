#include "ui/RoomDialog.h"
#include "ui/Localization.h"
#include "utilities/StringUtil.h"

#include <algorithm>

namespace electrical {
namespace ui {

wxBEGIN_EVENT_TABLE(RoomDialog, wxDialog)
    EVT_BUTTON(wxID_OK, RoomDialog::onOk)
wxEND_EVENT_TABLE()

static wxString W(const std::string& k) { return wxString(EL_TRW(k).c_str()); }

RoomDialog::RoomDialog(wxWindow* parent, const Room& initial)
    : wxDialog(parent, wxID_ANY, W("menu.rooms"),
               wxDefaultPosition, wxSize(380, 300), wxDEFAULT_DIALOG_STYLE),
      room_(initial) {

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* s = new wxFlexGridSizer(2, 8, 8);
    s->AddGrowableCol(1, 1);

    auto row = [&](const wxString& label, wxWindow* ctrl) {
        s->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        s->Add(ctrl, 1, wxEXPAND);
    };

    name_ = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(room_.name).c_str()));
    row("Name", name_);

    usage_ = new wxChoice(this, wxID_ANY);
    for (const char* u : { "Living room", "Bedroom", "Kitchen", "Bathroom",
                           "Hallway", "Service area", "Garage", "Office",
                           "Dining room", "Balcony", "Closet", "Other" })
        usage_->Append(u);
    usage_->SetSelection(static_cast<int>(room_.usage));
    row("Usage", usage_);

    function_ = new wxChoice(this, wxID_ANY);
    for (const char* f : { "Residential", "Commercial", "Industrial" })
        function_->Append(f);
    function_->SetSelection(static_cast<int>(room_.function));
    row("Function", function_);

    ceiling_ = new wxTextCtrl(this, wxID_ANY,
                              wxString::Format("%.2f", room_.ceilingHeight));
    row("Ceiling height (m)", ceiling_);

    area_ = new wxStaticText(this, wxID_ANY,
                             wxString::Format("%.2f m2", room_.areaM2));
    row("Area (auto)", area_);

    root->Add(s, 1, wxEXPAND | wxALL, 12);

    auto* btns = new wxStdDialogButtonSizer();
    auto* ok = new wxButton(this, wxID_OK, W("btn.ok"));
    ok->SetDefault();
    btns->AddButton(ok);
    btns->AddButton(new wxButton(this, wxID_CANCEL, W("btn.cancel")));
    btns->Realize();
    root->Add(btns, 0, wxEXPAND | wxALL, 8);

    SetSizer(root);
    Layout();
}

void RoomDialog::onOk(wxCommandEvent& e) {
    room_.name     = wideToUtf8(name_->GetValue().ToStdWstring());
    room_.usage    = static_cast<RoomUsage>(std::max(0, usage_->GetSelection()));
    room_.function = static_cast<RoomFunction>(std::max(0, function_->GetSelection()));
    double d;
    if (ceiling_->GetValue().ToDouble(&d)) room_.ceilingHeight = d;
    e.Skip();
}

} // namespace ui
} // namespace electrical
