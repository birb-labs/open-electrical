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
               wxDefaultPosition, wxSize(400, 520), wxDEFAULT_DIALOG_STYLE),
      room_(initial) {

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* s = new wxFlexGridSizer(2, 6, 8);
    s->AddGrowableCol(1, 1);

    auto row = [&](const wxString& label, wxWindow* ctrl) {
        s->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        s->Add(ctrl, 1, wxEXPAND);
    };

    name_ = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(room_.name).c_str()));
    row(W("room.name"), name_);

    usage_ = new wxChoice(this, wxID_ANY);
    for (const char* u : { "Living room", "Bedroom", "Kitchen", "Bathroom",
                           "Hallway", "Service area", "Garage", "Office",
                           "Dining room", "Balcony", "Closet", "Other" })
        usage_->Append(u);
    usage_->SetSelection(static_cast<int>(room_.usage));
    row(W("room.type"), usage_);

    usageDetail_ = new wxTextCtrl(this, wxID_ANY,
                                  wxString(utf8ToWide(room_.usageDetail).c_str()));
    row(W("room.usageDetail"), usageDetail_);

    function_ = new wxChoice(this, wxID_ANY);
    for (const char* f : { "Residential", "Commercial", "Industrial" })
        function_->Append(f);
    function_->SetSelection(static_cast<int>(room_.function));
    row(W("room.function"), function_);

    ceiling_ = new wxTextCtrl(this, wxID_ANY,
                              wxString::Format("%.2f", room_.ceilingHeight));
    row(W("room.ceilingHeight"), ceiling_);

    workPlane_ = new wxTextCtrl(this, wxID_ANY,
                                wxString::Format("%.2f", room_.workPlaneHeight));
    row(W("room.workPlane"), workPlane_);

    // Reflectances shown as percentages (0-100); stored as fractions 0..1.
    ceilRefl_  = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", room_.ceilingReflectance * 100.0));
    row(W("room.ceilingRefl"), ceilRefl_);
    wallRefl_  = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", room_.wallReflectance * 100.0));
    row(W("room.wallRefl"), wallRefl_);
    floorRefl_ = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", room_.floorReflectance * 100.0));
    row(W("room.floorRefl"), floorRefl_);

    contrast_ = new wxChoice(this, wxID_ANY);
    contrast_->Append(W("contrast.low"));
    contrast_->Append(W("contrast.medium"));
    contrast_->Append(W("contrast.high"));
    contrast_->SetSelection(static_cast<int>(room_.contrast));
    row(W("room.contrast"), contrast_);

    mf_ = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", room_.maintenanceFactor));
    row(W("room.mf"), mf_);
    cu_ = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", room_.utilizationFactor));
    row(W("room.cu"), cu_);

    area_ = new wxStaticText(this, wxID_ANY,
                             wxString::Format("%.2f m2", room_.areaM2));
    row(W("room.area"), area_);

    root->Add(s, 1, wxEXPAND | wxALL, 12);

    auto* btns = new wxStdDialogButtonSizer();
    auto* ok = new wxButton(this, wxID_OK, W("btn.ok"));
    ok->SetDefault();
    btns->AddButton(ok);
    btns->AddButton(new wxButton(this, wxID_CANCEL, W("btn.cancel")));
    btns->Realize();
    root->Add(btns, 0, wxEXPAND | wxALL, 8);

    SetSizerAndFit(root);
}

void RoomDialog::onOk(wxCommandEvent& e) {
    // Reflectance in [0,100] %, positive heights and MF, CU >= 0. A bad field
    // aborts the close with a message so the user can fix it.
    auto readPercent = [&](wxTextCtrl* c, const char* what, double& out) -> bool {
        double v;
        if (!c->GetValue().ToDouble(&v) || v < 0.0 || v > 100.0) {
            wxMessageBox(wxString::Format("%s must be a percentage between 0 and 100.", what),
                         "Invalid value", wxOK | wxICON_WARNING, this);
            return false;
        }
        out = v / 100.0;
        return true;
    };
    auto readPositive = [&](wxTextCtrl* c, const char* what, double& out) -> bool {
        double v;
        if (!c->GetValue().ToDouble(&v) || v <= 0.0) {
            wxMessageBox(wxString::Format("%s must be a positive number.", what),
                         "Invalid value", wxOK | wxICON_WARNING, this);
            return false;
        }
        out = v;
        return true;
    };

    double ceil, wp, mf, rc, rw, rf, cu;
    if (!readPositive(ceiling_,   "Ceiling height", ceil)) return;
    if (!readPositive(workPlane_, "Work plane height", wp)) return;
    if (!readPercent(ceilRefl_,   "Ceiling reflectance", rc)) return;
    if (!readPercent(wallRefl_,   "Wall reflectance", rw)) return;
    if (!readPercent(floorRefl_,  "Floor reflectance", rf)) return;
    if (!readPositive(mf_,        "Maintenance factor", mf)) return;
    {   // CU may be 0 (= derive automatically), so it is validated separately.
        double v;
        if (!cu_->GetValue().ToDouble(&v) || v < 0.0) {
            wxMessageBox("Utilization factor must be 0 (auto) or a positive number.",
                         "Invalid value", wxOK | wxICON_WARNING, this);
            return;
        }
        cu = v;
    }

    room_.name        = wideToUtf8(name_->GetValue().ToStdWstring());
    room_.usage       = static_cast<RoomUsage>(std::max(0, usage_->GetSelection()));
    room_.usageDetail = wideToUtf8(usageDetail_->GetValue().ToStdWstring());
    room_.function    = static_cast<RoomFunction>(std::max(0, function_->GetSelection()));
    room_.ceilingHeight      = ceil;
    room_.workPlaneHeight    = wp;
    room_.ceilingReflectance = rc;
    room_.wallReflectance    = rw;
    room_.floorReflectance   = rf;
    room_.contrast           = static_cast<TaskContrast>(std::max(0, contrast_->GetSelection()));
    room_.maintenanceFactor  = mf;
    room_.utilizationFactor  = cu;
    e.Skip();
}

} // namespace ui
} // namespace electrical
