// =============================================================================
//  MainPalette.h - The dockable main palette content (wxWidgets).
//
//  The palette is a wxPanel of command buttons grouped by workflow stage. It is
//  hosted differently per platform (see PaletteHost): on Windows it lives in a
//  BcUiPanelMFC docking pane; on Linux it is docked as a native wxWindow. The
//  panel itself is identical on both.
// =============================================================================
#pragma once

#include <wx/wx.h>

namespace electrical {
namespace ui {

class MainPalettePanel : public wxPanel {
public:
    explicit MainPalettePanel(wxWindow* parent);

    // Rebuilds dynamic content (e.g. project summary) after model changes.
    void refresh();

private:
    wxStaticText* summary_ = nullptr;

    // Sends a command string to the active document command line.
    void run(const wxString& command);

    wxButton* addButton(wxSizer* sizer, const std::string& labelKey,
                        const wxString& command);
};

} // namespace ui
} // namespace electrical
