// =============================================================================
//  ConfigDialog.h - wxWidgets project-settings dialog (EL-CONFIG).
//
//  A tabbed dialog (Scale / Network / Installation / Utility / Language). All
//  strings come from Localization so the whole dialog re-labels when the UI
//  language changes. Commands and identifiers stay English regardless.
// =============================================================================
#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>

#include "models/ProjectSettings.h"

namespace electrical {
namespace ui {

class ConfigDialog : public wxDialog {
public:
    ConfigDialog(wxWindow* parent, const ProjectSettings& initial);

    // The edited settings (valid after ShowModal() == wxID_OK).
    const ProjectSettings& result() const { return settings_; }

private:
    ProjectSettings settings_;

    // Controls.
    wxChoice*    unit_       = nullptr;
    wxTextCtrl*  voltageLL_  = nullptr;
    wxTextCtrl*  voltageLN_  = nullptr;
    wxTextCtrl*  frequency_  = nullptr;
    wxChoice*    phases_     = nullptr;
    wxChoice*    grounding_  = nullptr;
    wxChoice*    installType_= nullptr;
    wxChoice*    conduitMat_ = nullptr;
    wxTextCtrl*  wireGauge_  = nullptr;
    wxChoice*    utility_    = nullptr;
    wxChoice*    language_   = nullptr;

    wxPanel* buildScalePage(wxNotebook*);
    wxPanel* buildNetworkPage(wxNotebook*);
    wxPanel* buildInstallPage(wxNotebook*);
    wxPanel* buildUtilityPage(wxNotebook*);

    void loadFromSettings();
    void onAddProvider(wxCommandEvent&);
    void onOk(wxCommandEvent&);

    wxDECLARE_EVENT_TABLE();
};

} // namespace ui
} // namespace electrical
