#include "ui/ConfigDialog.h"
#include "ui/Localization.h"
#include "utilities/StringUtil.h"

#include <wx/statline.h>
#include <algorithm>
#include <cmath>

namespace electrical {
namespace ui {

enum {
    ID_AddProvider = wxID_HIGHEST + 100,
};

wxBEGIN_EVENT_TABLE(ConfigDialog, wxDialog)
    EVT_BUTTON(ID_AddProvider, ConfigDialog::onAddProvider)
    EVT_BUTTON(wxID_OK,        ConfigDialog::onOk)
wxEND_EVENT_TABLE()

static wxString W(const std::string& key) {
    return wxString(EL_TRW(key).c_str());
}
static wxString WS(const std::wstring& s) { return wxString(s.c_str()); }

ConfigDialog::ConfigDialog(wxWindow* parent, const ProjectSettings& initial)
    : wxDialog(parent, wxID_ANY, W("config.title"),
               wxDefaultPosition, wxSize(460, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      settings_(initial) {

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* nb   = new wxNotebook(this, wxID_ANY);

    nb->AddPage(buildScalePage(nb),    W("config.scale"));
    nb->AddPage(buildNetworkPage(nb),  W("config.network"));
    nb->AddPage(buildInstallPage(nb),  W("config.installation"));
    nb->AddPage(buildUtilityPage(nb),  W("config.utility"));

    root->Add(nb, 1, wxEXPAND | wxALL, 8);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

    auto* btns = new wxStdDialogButtonSizer();
    auto* ok = new wxButton(this, wxID_OK, W("btn.ok"));
    ok->SetDefault();
    btns->AddButton(ok);
    btns->AddButton(new wxButton(this, wxID_CANCEL, W("btn.cancel")));
    btns->Realize();
    root->Add(btns, 0, wxEXPAND | wxALL, 8);

    SetSizer(root);
    loadFromSettings();
    Layout();
}

wxPanel* ConfigDialog::buildScalePage(wxNotebook* nb) {
    auto* p = new wxPanel(nb);
    auto* s = new wxFlexGridSizer(2, 8, 8);
    s->AddGrowableCol(1, 1);

    s->Add(new wxStaticText(p, wxID_ANY, W("config.unit")), 0, wxALIGN_CENTER_VERTICAL);
    unit_ = new wxChoice(p, wxID_ANY);
    unit_->Append(W("unit.m"));  unit_->Append(W("unit.cm"));
    unit_->Append(W("unit.dm")); unit_->Append(W("unit.mm"));
    s->Add(unit_, 1, wxEXPAND);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(s, 0, wxEXPAND | wxALL, 12);
    p->SetSizer(outer);
    return p;
}

wxPanel* ConfigDialog::buildNetworkPage(wxNotebook* nb) {
    auto* p = new wxPanel(nb);
    auto* s = new wxFlexGridSizer(2, 8, 8);
    s->AddGrowableCol(1, 1);

    auto row = [&](const std::string& key, wxWindow* ctrl) {
        s->Add(new wxStaticText(p, wxID_ANY, W(key)), 0, wxALIGN_CENTER_VERTICAL);
        s->Add(ctrl, 1, wxEXPAND);
    };

    voltageLL_ = new wxTextCtrl(p, wxID_ANY);
    voltageLN_ = new wxTextCtrl(p, wxID_ANY);
    frequency_ = new wxTextCtrl(p, wxID_ANY);
    phases_    = new wxChoice(p, wxID_ANY);
    phases_->Append(W("phases.1")); phases_->Append(W("phases.2")); phases_->Append(W("phases.3"));
    grounding_ = new wxChoice(p, wxID_ANY);
    for (const char* g : { "TN-S", "TN-C", "TN-C-S", "TT", "IT" })
        grounding_->Append(g);

    row("config.voltageLL", voltageLL_);
    row("config.voltageLN", voltageLN_);
    row("config.frequency", frequency_);
    row("config.phases",    phases_);
    row("config.grounding", grounding_);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(s, 0, wxEXPAND | wxALL, 12);
    p->SetSizer(outer);
    return p;
}

wxPanel* ConfigDialog::buildInstallPage(wxNotebook* nb) {
    auto* p = new wxPanel(nb);
    auto* s = new wxFlexGridSizer(2, 8, 8);
    s->AddGrowableCol(1, 1);

    s->Add(new wxStaticText(p, wxID_ANY, W("config.installType")), 0, wxALIGN_CENTER_VERTICAL);
    installType_ = new wxChoice(p, wxID_ANY);
    installType_->Append(W("config.surface")); installType_->Append(W("config.concealed"));
    s->Add(installType_, 1, wxEXPAND);

    s->Add(new wxStaticText(p, wxID_ANY, W("config.conduitMat")), 0, wxALIGN_CENTER_VERTICAL);
    conduitMat_ = new wxChoice(p, wxID_ANY);
    for (const char* m : { "PVC (rigid)", "PVC (corrugated)", "Steel", "Aluminum" })
        conduitMat_->Append(m);
    s->Add(conduitMat_, 1, wxEXPAND);

    s->Add(new wxStaticText(p, wxID_ANY, W("config.wireGauge")), 0, wxALIGN_CENTER_VERTICAL);
    wireGauge_ = new wxTextCtrl(p, wxID_ANY);
    s->Add(wireGauge_, 1, wxEXPAND);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(s, 0, wxEXPAND | wxALL, 12);
    p->SetSizer(outer);
    return p;
}

wxPanel* ConfigDialog::buildUtilityPage(wxNotebook* nb) {
    auto* p = new wxPanel(nb);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* s = new wxFlexGridSizer(2, 8, 8);
    s->AddGrowableCol(1, 1);

    s->Add(new wxStaticText(p, wxID_ANY, W("config.utility")), 0, wxALIGN_CENTER_VERTICAL);
    utility_ = new wxChoice(p, wxID_ANY);
    s->Add(utility_, 1, wxEXPAND);

    s->Add(new wxStaticText(p, wxID_ANY, W("config.language")), 0, wxALIGN_CENTER_VERTICAL);
    language_ = new wxChoice(p, wxID_ANY);
    language_->Append("English (en)");
    language_->Append("Portugues (pt-BR)");
    language_->Append("Espanol (es)");
    s->Add(language_, 1, wxEXPAND);

    outer->Add(s, 0, wxEXPAND | wxALL, 12);
    auto* add = new wxButton(p, ID_AddProvider, W("config.addProvider"));
    outer->Add(add, 0, wxLEFT | wxBOTTOM, 12);
    p->SetSizer(outer);
    return p;
}

void ConfigDialog::loadFromSettings() {
    unit_->SetSelection(static_cast<int>(settings_.unit));
    voltageLL_->SetValue(wxString::Format("%.0f", settings_.voltageLineToLine));
    voltageLN_->SetValue(wxString::Format("%.0f", settings_.voltageLineToNeutral));
    frequency_->SetValue(wxString::Format("%.0f", settings_.frequency));
    phases_->SetSelection(static_cast<int>(settings_.phases));
    grounding_->SetSelection(static_cast<int>(settings_.grounding));
    installType_->SetSelection(static_cast<int>(settings_.installation));
    conduitMat_->SetSelection(static_cast<int>(settings_.conduitMaterial));
    wireGauge_->SetValue(wxString::Format("%.1f", settings_.defaultWireGaugeMM2));

    utility_->Clear();
    int sel = 0, i = 0;
    for (const auto& prov : settings_.providers) {
        utility_->Append(WS(utf8ToWide(prov.name)));
        if (prov.name == settings_.utilityProvider) sel = i;
        ++i;
    }
    if (utility_->GetCount()) utility_->SetSelection(sel);

    if (settings_.uiLanguage == "pt-BR") language_->SetSelection(1);
    else if (settings_.uiLanguage == "es") language_->SetSelection(2);
    else language_->SetSelection(0);
}

void ConfigDialog::onAddProvider(wxCommandEvent&) {
    wxTextEntryDialog dlg(this, W("config.addProvider"), W("config.utility"));
    if (dlg.ShowModal() != wxID_OK) return;
    const wxString name = dlg.GetValue().Trim();
    if (name.IsEmpty()) return;

    PowerUtility prov;
    prov.name = wideToUtf8(name.ToStdWstring());
    prov.userDefined = true;
    settings_.providers.push_back(prov);
    utility_->Append(name);
    utility_->SetSelection(utility_->GetCount() - 1);
}


void ConfigDialog::onOk(wxCommandEvent& e) {
    // Pull control values back into settings_.
    settings_.unit = static_cast<Unit>(std::max(0, unit_->GetSelection()));

    double d;
    if (voltageLL_->GetValue().ToDouble(&d)) settings_.voltageLineToLine = d;
    if (voltageLN_->GetValue().ToDouble(&d)) settings_.voltageLineToNeutral = d;
    if (frequency_->GetValue().ToDouble(&d)) settings_.frequency = d;
    if (wireGauge_->GetValue().ToDouble(&d)) settings_.defaultWireGaugeMM2 = d;

    settings_.phases       = static_cast<Phases>(std::max(0, phases_->GetSelection()));
    settings_.grounding    = static_cast<GroundingSystem>(std::max(0, grounding_->GetSelection()));
    settings_.installation = static_cast<InstallationType>(std::max(0, installType_->GetSelection()));
    settings_.conduitMaterial = static_cast<ConduitMaterial>(std::max(0, conduitMat_->GetSelection()));

    const int u = utility_->GetSelection();
    if (u >= 0 && u < static_cast<int>(settings_.providers.size()))
        settings_.utilityProvider = settings_.providers[u].name;

    switch (language_->GetSelection()) {
        case 1: settings_.uiLanguage = "pt-BR"; break;
        case 2: settings_.uiLanguage = "es";    break;
        default: settings_.uiLanguage = "en";   break;
    }

    e.Skip();   // let the default handler close with wxID_OK
}

} // namespace ui
} // namespace electrical
