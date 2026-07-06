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
    wxTextCtrl* name_    = nullptr;
    wxChoice*   usage_   = nullptr;
    wxChoice*   function_= nullptr;
    wxTextCtrl* ceiling_ = nullptr;
    wxStaticText* area_  = nullptr;

    void onOk(wxCommandEvent&);
    wxDECLARE_EVENT_TABLE();
};

} // namespace ui
} // namespace electrical
