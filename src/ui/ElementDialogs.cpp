// =============================================================================
//  ElementDialogs.cpp - wxWidgets configuration dialogs shown when the user
//  manually inserts an electrical element (light, outlet, switch, panel).
//
//  Each dialog is small and self-contained; the UiBridge free functions at the
//  bottom construct, run, and copy values back into the model. wx is brought up
//  in embedded mode by ensureWxInitialized() (see UiBridge.cpp).
// =============================================================================
#include "ui/UiBridge.h"
#include "ui/Localization.h"
#include "utilities/StringUtil.h"

#include "models/LightPoint.h"
#include "models/Outlet.h"
#include "models/Switch.h"
#include "models/Panel.h"
#include "models/ProjectSettings.h"

#include <cwctype>

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/combobox.h>
#include <algorithm>
#include <cmath>

namespace electrical {
namespace ui {

// Our privately-linked wx has no top-level window of its own (BricsCAD's windows
// belong to BricsCAD's wx), so dialogs are parented to nullptr.
namespace {

wxString W(const std::string& k) { return wxString(EL_TRW(k).c_str()); }

// Shared helper: build a two-column form on a dialog, returning the grid sizer.
wxFlexGridSizer* formGrid() {
    auto* g = new wxFlexGridSizer(2, 8, 8);
    g->AddGrowableCol(1, 1);
    return g;
}

void addRow(wxWindow* parent, wxFlexGridSizer* g, const wxString& label, wxWindow* ctrl) {
    g->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
    g->Add(ctrl, 1, wxEXPAND);
}

wxSizer* okCancel(wxDialog* dlg) {
    auto* btns = new wxStdDialogButtonSizer();
    auto* ok = new wxButton(dlg, wxID_OK, W("btn.ok"));
    ok->SetDefault();
    btns->AddButton(ok);
    btns->AddButton(new wxButton(dlg, wxID_CANCEL, W("btn.cancel")));
    btns->Realize();
    return btns;
}

// ---- Mounting-height combo (Low / Medium / High + free exact value) --------
// Standard NBR mounting heights (m above finished floor).
struct HeightPreset { const char* key; double meters; };
constexpr HeightPreset kHeights[] = {
    { "height.low",    0.30 },
    { "height.medium", 1.30 },
    { "height.high",   2.00 },
};
constexpr int kHeightCount = 3;

// An editable combo: the user can pick a named level OR type an exact height.
wxComboBox* makeHeightCombo(wxWindow* parent, double h) {
    auto* c = new wxComboBox(parent, wxID_ANY);
    for (const auto& p : kHeights)
        c->Append(W(p.key) + wxString::Format(" (%.2f m)", p.meters));
    int sel = -1;
    for (int i = 0; i < kHeightCount; ++i)
        if (std::fabs(h - kHeights[i].meters) < 0.005) { sel = i; break; }
    if (sel >= 0) c->SetSelection(sel);
    else          c->SetValue(wxString::Format("%.2f", h));  // custom exact value
    return c;
}

// Reads the height: a chosen preset, or the exact metres the user typed.
double readHeightCombo(const wxComboBox* c) {
    const int sel = c->GetSelection();
    if (sel >= 0 && sel < kHeightCount) return kHeights[sel].meters;
    double d;
    if (c->GetValue().ToDouble(&d)) return d;
    return 0.30;
}

// ---- Lighting point -------------------------------------------------------
class LightDialog : public wxDialog {
public:
    explicit LightDialog(const LightPoint& in)
        : wxDialog(nullptr, wxID_ANY, "Lighting point",
                   wxDefaultPosition, wxSize(340, 280)),
          model_(in) {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* g = formGrid();

        lamps_   = new wxSpinCtrl(this, wxID_ANY); lamps_->SetRange(1, 12);
        lamps_->SetValue(model_.lampCount);
        watts_   = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", model_.wattsPerLamp));
        lumens_  = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", model_.lumensPerLamp));
        wall_    = new wxCheckBox(this, wxID_ANY, "Wall-mounted (arandela)");
        wall_->SetValue(model_.wallMounted);
        height_  = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", model_.offsetFromCeiling));
        cmd_     = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(model_.noteLetter).c_str()));
        cmd_->SetMaxLength(1);
        circ_    = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(model_.circuitLabel).c_str()));

        // Total power is NOT an input: the POTENCIA notation ("2x32W") and the load
        // (powerVA) are both derived from lamp count x watts per lamp. A read-only
        // preview keeps that derivation visible without making it editable.
        preview_ = new wxStaticText(this, wxID_ANY, wxEmptyString);

        addRow(this, g, W("light.lampCount"),  lamps_);
        addRow(this, g, W("light.lampWatts"),  watts_);
        addRow(this, g, "Lumens per lamp",     lumens_);
        addRow(this, g, "Height below ceiling (m)", height_);
        addRow(this, g, W("light.power"),      preview_);
        addRow(this, g, W("switch.command"),   cmd_);
        addRow(this, g, W("light.circuit"),    circ_);
        root->Add(g, 1, wxEXPAND | wxALL, 12);
        root->Add(wall_, 0, wxLEFT | wxBOTTOM, 12);
        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        root->Add(okCancel(this), 0, wxEXPAND | wxALL, 8);
        SetSizerAndFit(root);

        Bind(wxEVT_BUTTON, &LightDialog::onOk, this, wxID_OK);
        lamps_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { refreshPreview(); });
        watts_->Bind(wxEVT_TEXT,     [this](wxCommandEvent&) { refreshPreview(); });
        refreshPreview();
    }
    LightPoint result() const {
        LightPoint m = model_;
        m.lampCount   = lamps_->GetValue();
        double d;
        if (watts_->GetValue().ToDouble(&d))  m.wattsPerLamp = d;
        if (lumens_->GetValue().ToDouble(&d)) m.lumensPerLamp = d;
        if (height_->GetValue().ToDouble(&d)) m.offsetFromCeiling = std::max(0.0, d);
        m.wallMounted = wall_->GetValue();
        // Total power and its notation are derived, never typed.
        m.powerVA      = m.lampCount * m.wattsPerLamp;
        m.powerText    = powerNotation(m.lampCount, m.wattsPerLamp);
        m.noteLetter   = wideToUtf8(cmd_->GetValue().ToStdWstring());
        m.circuitLabel = wideToUtf8(circ_->GetValue().ToStdWstring());
        return m;
    }

    // "2x32W" - the POTENCIA block attribute, built from count x individual watts.
    static std::string powerNotation(int lampCount, double wattsPerLamp) {
        return std::to_string(lampCount) + "x" +
               std::to_string(static_cast<int>(wattsPerLamp)) + "W";
    }

private:
    // Mirrors what will be stamped as POTENCIA, plus the resulting total load.
    void refreshPreview() {
        double w = 0.0;
        watts_->GetValue().ToDouble(&w);
        const int n = lamps_->GetValue();
        preview_->SetLabel(wxString::Format("%s  (= %.0f W)",
            wxString(utf8ToWide(powerNotation(n, w)).c_str()), n * w));
    }

    // A positive lamp count and individual power, a single-letter command and an
    // integer circuit number. Total power is derived, so there is nothing to check.
    void onOk(wxCommandEvent& e) {
        double w = 0.0;
        if (!watts_->GetValue().ToDouble(&w) || w <= 0.0) {
            wxMessageBox("Power per lamp must be a positive number (e.g. 32).",
                         "Invalid power", wxOK | wxICON_WARNING, this);
            return;
        }
        if (lamps_->GetValue() <= 0) {
            wxMessageBox("Lamp count must be a positive number (e.g. 2).",
                         "Invalid lamp count", wxOK | wxICON_WARNING, this);
            return;
        }
        const wxString c = cmd_->GetValue();
        if (c.length() != 1 || !std::iswalpha(static_cast<wint_t>(c[0].GetValue()))) {
            wxMessageBox("Command must be a single letter (e.g. a).",
                         "Invalid command", wxOK | wxICON_WARNING, this);
            return;
        }
        long dummy;
        if (!circ_->GetValue().ToLong(&dummy)) {
            wxMessageBox("Circuit must be an integer number (e.g. 3).",
                         "Invalid circuit", wxOK | wxICON_WARNING, this);
            return;
        }
        e.Skip();   // let wxDialog close with wxID_OK
    }

    LightPoint  model_;
    wxSpinCtrl* lamps_;
    wxTextCtrl *watts_, *lumens_, *height_, *cmd_, *circ_;
    wxStaticText* preview_;
    wxCheckBox* wall_;
};

// ---- Outlet ---------------------------------------------------------------
// Up to three individually-configured modules (height / power / purpose). Module
// 1 is always present; modules 2 and 3 are toggled on via their "include" box
// (this is the add/remove control). Two special combined outlet+switch symbols
// are offered separately and, when chosen, override the per-module list.
class OutletDialog : public wxDialog {
public:
    explicit OutletDialog(const Outlet& in)
        : wxDialog(nullptr, wxID_ANY, "Outlet",
                   wxDefaultPosition, wxSize(460, 360)),
          model_(in) {
        auto* root = new wxBoxSizer(wxVERTICAL);

        auto* top = formGrid();
        special_ = new wxChoice(this, wxID_ANY);
        special_->Append("None (plain outlet, use modules below)");
        special_->Append("Outlet + 2-section switch (combined)");
        special_->Append("Duplex + 1-section switch (combined)");
        special_->SetSelection(model_.kind == OutletKind::WithSwitch2 ? 1
                             : model_.kind == OutletKind::DuplexWithSwitch1 ? 2 : 0);
        circ_ = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(model_.circuitLabel).c_str()));
        addRow(this, top, "Combined type", special_);
        addRow(this, top, W("light.circuit"), circ_);
        root->Add(top, 0, wxEXPAND | wxALL, 12);

        root->Add(new wxStaticText(this, wxID_ANY,
            "Modules (chain, wall to room). Height fill: <0.6 low, <1.5 medium, else high."),
            0, wxLEFT | wxRIGHT, 12);

        // One row per module: include? | height | VA | purpose.
        auto* grid = new wxFlexGridSizer(4, 6, 8);
        grid->AddGrowableCol(1, 1);
        grid->Add(new wxStaticText(this, wxID_ANY, "Module"));
        grid->Add(new wxStaticText(this, wxID_ANY, "Height"));
        grid->Add(new wxStaticText(this, wxID_ANY, "VA"));
        grid->Add(new wxStaticText(this, wxID_ANY, "Purpose"));
        for (int i = 0; i < 3; ++i) {
            const bool present = i < static_cast<int>(model_.modules.size());
            const OutletModule m = present ? model_.modules[i] : OutletModule{};
            include_[i] = new wxCheckBox(this, wxID_ANY, wxString::Format("%d", i + 1));
            include_[i]->SetValue(present || i == 0);
            if (i == 0) include_[i]->Enable(false);   // module 1 is mandatory
            height_[i]  = makeHeightCombo(this, m.mountingHeight);
            va_[i]      = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", m.powerVA));
            purpose_[i] = new wxChoice(this, wxID_ANY);
            purpose_[i]->Append("General (TUG)");
            purpose_[i]->Append("Specific (TUE)");
            purpose_[i]->SetSelection(static_cast<int>(m.purpose));
            grid->Add(include_[i], 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(height_[i], 1, wxEXPAND);
            grid->Add(va_[i], 0);
            grid->Add(purpose_[i], 0);
        }
        root->Add(grid, 1, wxEXPAND | wxALL, 12);

        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        root->Add(okCancel(this), 0, wxEXPAND | wxALL, 8);
        SetSizerAndFit(root);
    }
    Outlet result() const {
        Outlet m = model_;
        const int special = special_->GetSelection();
        if (special == 1 || special == 2) {
            m.setKind(special == 1 ? OutletKind::WithSwitch2
                                   : OutletKind::DuplexWithSwitch1);
            double va = 100.0, d;
            if (va_[0]->GetValue().ToDouble(&d)) va = d;
            for (auto& mod : m.modules) {
                mod.powerVA = va;
                mod.purpose = static_cast<OutletPurpose>(std::max(0, purpose_[0]->GetSelection()));
            }
        } else {
            std::vector<OutletModule> mods;
            for (int i = 0; i < 3; ++i) {
                if (!include_[i]->GetValue()) continue;
                OutletModule mod;
                mod.mountingHeight = readHeightCombo(height_[i]);
                double d; if (va_[i]->GetValue().ToDouble(&d)) mod.powerVA = d;
                mod.purpose = static_cast<OutletPurpose>(std::max(0, purpose_[i]->GetSelection()));
                mods.push_back(mod);
            }
            if (mods.empty()) mods.emplace_back();
            m.modules = std::move(mods);
            m.kind = m.modules.size() == 3 ? OutletKind::Triplex
                   : m.modules.size() == 2 ? OutletKind::Duplex
                                           : OutletKind::Single;
        }
        m.powerVA = m.totalVA();
        m.circuitLabel = wideToUtf8(circ_->GetValue().ToStdWstring());
        return m;
    }
private:
    Outlet      model_;
    wxChoice*   special_;
    wxTextCtrl* circ_;
    wxCheckBox* include_[3];
    wxComboBox* height_[3];
    wxTextCtrl* va_[3];
    wxChoice*   purpose_[3];
};

// ---- Switch ---------------------------------------------------------------
class SwitchDialog : public wxDialog {
public:
    explicit SwitchDialog(const Switch& in)
        : wxDialog(nullptr, wxID_ANY, "Switch",
                   wxDefaultPosition, wxSize(320, 220)),
          model_(in) {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* g = formGrid();

        model_.syncCommands();

        // "Normal" switches carry 1-3 sections (chosen with the spinner); the
        // special variants are always single-section, single-command.
        type_ = new wxChoice(this, wxID_ANY);
        type_->Append("Normal (1-3 sections)");
        type_->Append("Three-way (paralelo)");
        type_->Append("Four-way (intermediario)");
        type_->Append("Bell push button (campainha)");
        type_->SetSelection(model_.kind == SwitchKind::ThreeWay ? 1
                          : model_.kind == SwitchKind::FourWay  ? 2
                          : model_.kind == SwitchKind::Bell     ? 3 : 0);

        sections_ = new wxSpinCtrl(this, wxID_ANY);
        sections_->SetRange(1, 3);
        sections_->SetValue(model_.sectionCount());
        height_ = makeHeightCombo(this, model_.mountingHeight);

        addRow(this, g, "Type", type_);
        addRow(this, g, W("switch.sections"), sections_);
        addRow(this, g, W("height.label") + " (m)", height_);
        for (int i = 0; i < 3; ++i) {
            const std::string v = i < static_cast<int>(model_.commands.size())
                                ? model_.commands[i] : std::string();
            cmd_[i] = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(v).c_str()));
            cmd_[i]->SetMaxLength(1);
            addRow(this, g, W("switch.command") + wxString::Format(" %d", i + 1), cmd_[i]);
        }
        root->Add(g, 1, wxEXPAND | wxALL, 12);
        root->Add(new wxStaticText(this, wxID_ANY,
            "One lowercase command letter per section (a, b, c)."),
            0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        root->Add(okCancel(this), 0, wxEXPAND | wxALL, 8);
        SetSizerAndFit(root);

        // Show/hide the command fields as the section count changes.
        type_->Bind(wxEVT_CHOICE,   [this](wxCommandEvent&) { refresh(); });
        sections_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { refresh(); });
        refresh();
    }
    Switch result() const {
        Switch m = model_;
        m.kind = kindFromUi();
        m.gangs = m.sectionCount();
        m.syncCommands();
        for (size_t i = 0; i < m.commands.size(); ++i)
            m.commands[i] = wideToUtf8(cmd_[i]->GetValue().ToStdWstring());
        // Keep the legacy single-letter field in sync with the first command.
        m.noteLetter = m.commands.empty() ? std::string() : m.commands.front();
        m.mountingHeight = readHeightCombo(height_);
        return m;
    }
private:
    SwitchKind kindFromUi() const {
        switch (type_->GetSelection()) {
            case 1: return SwitchKind::ThreeWay;
            case 2: return SwitchKind::FourWay;
            case 3: return SwitchKind::Bell;
            default: break;
        }
        switch (sections_->GetValue()) {
            case 2:  return SwitchKind::TwoSection;
            case 3:  return SwitchKind::ThreeSection;
            default: return SwitchKind::OneSection;
        }
    }
    // Only the "Normal" type has a section count; the command fields beyond the
    // active section count are disabled (this is the add/remove of sections).
    void refresh() {
        const bool normal = type_->GetSelection() == 0;
        sections_->Enable(normal);
        const int n = normal ? sections_->GetValue() : 1;
        for (int i = 0; i < 3; ++i) cmd_[i]->Enable(i < n);
    }

    Switch      model_;
    wxChoice*   type_;
    wxSpinCtrl* sections_;
    wxComboBox* height_;
    wxTextCtrl* cmd_[3];
};

// ---- Panel ----------------------------------------------------------------
class PanelDialog : public wxDialog {
public:
    explicit PanelDialog(const Panel& in)
        : wxDialog(nullptr, wxID_ANY, "Load panel",
                   wxDefaultPosition, wxSize(320, 210)),
          model_(in) {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* g = formGrid();

        name_ = new wxTextCtrl(this, wxID_ANY, wxString(utf8ToWide(model_.name).c_str()));
        main_ = new wxCheckBox(this, wxID_ANY, "Main board (QGBT)");
        main_->SetValue(model_.isMain);
        embedded_ = new wxCheckBox(this, wxID_ANY, "Embedded (embutido, flush in the wall)");
        embedded_->SetValue(model_.embedded);
        height_ = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", model_.mountingHeight));

        addRow(this, g, "Name", name_);
        addRow(this, g, "Mounting height (m)", height_);
        root->Add(g, 1, wxEXPAND | wxALL, 12);
        root->Add(main_, 0, wxLEFT | wxBOTTOM, 12);
        root->Add(embedded_, 0, wxLEFT | wxBOTTOM, 12);
        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        root->Add(okCancel(this), 0, wxEXPAND | wxALL, 8);
        SetSizerAndFit(root);
    }
    Panel result() const {
        Panel m = model_;
        m.name     = wideToUtf8(name_->GetValue().ToStdWstring());
        m.isMain   = main_->GetValue();
        m.embedded = embedded_->GetValue();
        double d;
        if (height_->GetValue().ToDouble(&d)) m.mountingHeight = d;
        return m;
    }
private:
    Panel     model_;
    wxTextCtrl* name_;
    wxCheckBox* main_;
    wxCheckBox* embedded_;
    wxTextCtrl* height_;
};

// ---- Lighting run (EL-CALC-LIGHT): pick a room + a luminaire ----------------
// Every other luminotechnical parameter (reflectances, contrast, MF, CU, heights)
// is read from the Room; here the user only chooses which room(s) to process and
// the luminaire flux/power. Picking a catalogue entry fills the flux/power fields;
// "Custom" lets them type any value.
class LightingRunDialog : public wxDialog {
public:
    LightingRunDialog(const std::vector<std::string>& roomNames,
                      const LightingRunOptions& opt)
        : wxDialog(nullptr, wxID_ANY, W("light.run.title"),
                   wxDefaultPosition, wxSize(400, 260)) {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* g = formGrid();

        room_ = new wxChoice(this, wxID_ANY);
        room_->Append(W("light.run.allRooms"));
        for (const auto& n : roomNames) room_->Append(wxString(utf8ToWide(n).c_str()));
        room_->SetSelection(opt.roomIndex < 0 ? 0 : opt.roomIndex + 1);

        preset_ = new wxChoice(this, wxID_ANY);
        for (const auto& l : kLuminaires()) preset_->Append(wxString(l.label));
        preset_->Append(W("light.run.customLuminaire"));
        preset_->SetSelection(0);

        lumens_ = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", opt.lumens));
        watts_  = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.0f", opt.watts));

        addRow(this, g, W("light.run.room"),      room_);
        addRow(this, g, W("light.run.luminaire"), preset_);
        addRow(this, g, W("light.run.lumens"),    lumens_);
        addRow(this, g, W("light.run.watts"),     watts_);
        root->Add(g, 1, wxEXPAND | wxALL, 12);
        root->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        root->Add(okCancel(this), 0, wxEXPAND | wxALL, 8);
        SetSizerAndFit(root);

        preset_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { onPreset(); });
        Bind(wxEVT_BUTTON, &LightingRunDialog::onOk, this, wxID_OK);
        onPreset();
    }

    LightingRunOptions result() const {
        LightingRunOptions o;
        o.roomIndex = room_->GetSelection() - 1;   // 0 = "All" -> -1
        double d;
        if (lumens_->GetValue().ToDouble(&d)) o.lumens = d;
        if (watts_ ->GetValue().ToDouble(&d)) o.watts  = d;
        return o;
    }

private:
    struct Cat { const char* label; double lumens; double watts; };
    static const std::vector<Cat>& kLuminaires() {
        static const std::vector<Cat> c = {
            { "LED 18W - 1800 lm",           1800.0, 18.0 },
            { "LED 24W - 2400 lm",           2400.0, 24.0 },
            { "LED panel 36W - 3600 lm",     3600.0, 36.0 },
            { "Fluorescent 2x16W - 1600 lm", 1600.0, 32.0 },
            { "Fluorescent 2x32W - 3200 lm", 3200.0, 64.0 },
        };
        return c;
    }
    void onPreset() {
        const int sel = preset_->GetSelection();
        const auto& c = kLuminaires();
        const bool custom = sel < 0 || sel >= static_cast<int>(c.size());
        lumens_->Enable(custom);
        watts_->Enable(custom);
        if (!custom) {
            lumens_->SetValue(wxString::Format("%.0f", c[sel].lumens));
            watts_->SetValue(wxString::Format("%.0f", c[sel].watts));
        }
    }
    void onOk(wxCommandEvent& e) {
        double lm, w;
        if (!lumens_->GetValue().ToDouble(&lm) || lm <= 0.0) {
            wxMessageBox("Luminous flux must be a positive number (lumens).",
                         "Invalid luminaire", wxOK | wxICON_WARNING, this);
            return;
        }
        if (!watts_->GetValue().ToDouble(&w) || w <= 0.0) {
            wxMessageBox("Luminaire power must be a positive number (W).",
                         "Invalid luminaire", wxOK | wxICON_WARNING, this);
            return;
        }
        e.Skip();
    }

    wxChoice*   room_;
    wxChoice*   preset_;
    wxTextCtrl* lumens_;
    wxTextCtrl* watts_;
};

} // namespace

// ---- UiBridge entry points ------------------------------------------------
// Note: luminotechnical parameters (reflectances, contrast, MF, CU, heights) live
// per Room and are edited in the EL-ROOM dialog, not globally. EL-CALC-LIGHT only
// prompts for the room selection and the luminaire (showLightingRunDialog).

bool showLightingRunDialog(const std::vector<std::string>& roomNames,
                           LightingRunOptions& opt) {
    if (!ensureWxInitialized()) return false;
    LightingRunDialog dlg(roomNames, opt);
    if (dlg.ShowModal() == wxID_OK) { opt = dlg.result(); return true; }
    return false;
}

bool showLightDialog(LightPoint& light) {
    if (!ensureWxInitialized()) return false;
    LightDialog dlg(light);
    if (dlg.ShowModal() == wxID_OK) { light = dlg.result(); return true; }
    return false;
}

bool showOutletDialog(Outlet& outlet) {
    if (!ensureWxInitialized()) return false;
    OutletDialog dlg(outlet);
    if (dlg.ShowModal() == wxID_OK) { outlet = dlg.result(); return true; }
    return false;
}

bool showSwitchDialog(Switch& sw) {
    if (!ensureWxInitialized()) return false;
    SwitchDialog dlg(sw);
    if (dlg.ShowModal() == wxID_OK) { sw = dlg.result(); return true; }
    return false;
}

bool showPanelDialog(Panel& panel) {
    if (!ensureWxInitialized()) return false;
    PanelDialog dlg(panel);
    if (dlg.ShowModal() == wxID_OK) { panel = dlg.result(); return true; }
    return false;
}

} // namespace electrical (ui)
} // namespace electrical
