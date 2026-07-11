#include "ui/UiBridge.h"
#include "ui/ConfigDialog.h"
#include "ui/RoomDialog.h"
#include "ui/PaletteHost.h"
#include "utilities/Diag.h"

#include <wx/wx.h>
#include <wx/app.h>
#include <wx/choicdlg.h>
#include <wx/image.h>

namespace electrical {
namespace ui {

// Minimal wxApp for our privately-linked (static) wxWidgets. BricsCAD runs its
// OWN wx event loop; we do NOT start a second one. We only need a wxApp object
// to exist so wx GUI classes (dialogs, frames) can be created. Following the
// BRX brxWxSample, the app factory is registered with wxIMPLEMENT_APP_NO_MAIN
// and the runtime is brought up with wxInitialize() (NOT wxEntryStart, which is
// for taking over the process - passing a null argv there corrupts wx/GTK state
// and crashes when the first top-level window is created).
class OeWxApp : public wxApp {
public:
    bool OnInit() override { return true; }
};

} // namespace ui
} // namespace electrical

// Registers the app factory + wxGetApp(); defines no main().
wxIMPLEMENT_APP_NO_MAIN(electrical::ui::OeWxApp);

namespace electrical {
namespace ui {

namespace {
bool g_wxReady = false;
}

// ---------------------------------------------------------------------------
//  wxWidgets bootstrap (embedded mode - coexists with BricsCAD's own wx)
// ---------------------------------------------------------------------------
bool ensureWxInitialized() {
    EL_DIAG_LOG("wx:ensureWxInitialized enter");
    if (g_wxReady) return true;

    // wxInitialize() creates our wxApp (via the factory above) and initialises
    // the GUI toolkit for use, without entering a main loop. GTK is already
    // initialised by BricsCAD; re-initialising is safe (idempotent).
    EL_DIAG_LOG("wx: calling wxInitialize()");
    if (!wxInitialize()) {
        EL_DIAG_LOG("wx: wxInitialize FAILED");
        return false;
    }
    EL_DIAG_LOG("wx: after wxInitialize");
    wxInitAllImageHandlers();
    EL_DIAG_LOG("wx: initialized OK");
    g_wxReady = true;
    return true;
}

void shutdownWx() {
    if (!g_wxReady) return;
    PaletteHost::instance().destroy();
    wxUninitialize();
    g_wxReady = false;
}

// Resolves a wxWindow* parent from the host's top window, if any.
static wxWindow* hostParent() {
    return wxTheApp ? wxTheApp->GetTopWindow() : nullptr;
}

// ---------------------------------------------------------------------------
//  Dialogs
// ---------------------------------------------------------------------------
bool showConfigDialog(ProjectSettings& settings) {
    EL_DIAG_LOG("wx:showConfigDialog enter");
    if (!ensureWxInitialized()) return false;
    EL_DIAG_LOG("wx:showConfigDialog before ConfigDialog ctor");
    ConfigDialog dlg(hostParent(), settings);
    EL_DIAG_LOG("wx:showConfigDialog before ShowModal");
    if (dlg.ShowModal() == wxID_OK) {
        settings = dlg.result();
        return true;
    }
    EL_DIAG_LOG("wx:showConfigDialog after ShowModal (cancel)");
    return false;
}

bool showRoomDialog(Room& room) {
    if (!ensureWxInitialized()) return false;
    RoomDialog dlg(hostParent(), room);
    if (dlg.ShowModal() == wxID_OK) {
        room = dlg.result();
        return true;
    }
    return false;
}

bool showRoomPicker(const std::vector<std::string>& items, int& outIndex) {
    if (!ensureWxInitialized()) return false;
    wxArrayString choices;
    for (const auto& s : items) choices.Add(wxString::FromUTF8(s.c_str()));
    wxSingleChoiceDialog dlg(hostParent(), "Select a room to remove:", "EL-DEL-ROOM", choices);
    if (dlg.ShowModal() == wxID_OK) { outIndex = dlg.GetSelection(); return true; }
    return false;
}

// ---------------------------------------------------------------------------
//  Palette
// ---------------------------------------------------------------------------
void togglePalette() {
    EL_DIAG_LOG("ui:togglePalette enter");
    if (!ensureWxInitialized()) { EL_DIAG_LOG("ui:togglePalette wx init FAILED"); return; }
    EL_DIAG_LOG("ui:togglePalette before PaletteHost::toggle");
    PaletteHost::instance().toggle();
    EL_DIAG_LOG("ui:togglePalette after PaletteHost::toggle");
}

void refreshPalette() {
    if (!g_wxReady) return;
    PaletteHost::instance().refresh();
}

} // namespace ui
} // namespace electrical
