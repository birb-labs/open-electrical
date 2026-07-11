// =============================================================================
//  RoomDialog.h - wxWidgets room-attributes dialog (EL-ROOM).
// =============================================================================
#pragma once

#include <wx/wx.h>
#include "models/Room.h"

namespace electrical {
namespace ui {

class RoomDialog : public wxDialog {
public:
    RoomDialog(wxWindow* parent, const Room& initial);
    const Room& result() const { return room_; }

private:
    Room        room_;
    wxTextCtrl* name_       = nullptr;
    wxChoice*   usage_      = nullptr;   // "type" combo (Sala, Quarto...)
    wxTextCtrl* usageDetail_= nullptr;   // free-text specific use
    wxChoice*   function_   = nullptr;
    wxTextCtrl* ceiling_    = nullptr;
    wxStaticText* area_     = nullptr;

    // Luminotechnical parameters (per room).
    wxTextCtrl* ceilRefl_   = nullptr;   // % (0-100)
    wxTextCtrl* wallRefl_   = nullptr;
    wxTextCtrl* floorRefl_  = nullptr;
    wxChoice*   contrast_   = nullptr;   // Low / Medium / High
    wxTextCtrl* workPlane_  = nullptr;   // m
    wxTextCtrl* mf_         = nullptr;   // maintenance factor
    wxTextCtrl* cu_         = nullptr;   // utilization factor (0 = auto)

    void onOk(wxCommandEvent&);
    wxDECLARE_EVENT_TABLE();
};

} // namespace ui
} // namespace electrical
