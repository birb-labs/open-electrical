#include "ui/UiBridge.h"
#include "ui/ConfigDialog.h"
#include "ui/RoomDialog.h"
#include "ui/PaletteHost.h"

#include <wx/wx.h>
#include <wx/app.h>

namespace electrical {
namespace ui {

namespace {
bool g_wxReady = false;
}

// ---------------------------------------------------------------------------
//  wxWidgets bootstrap
//
//  BricsCAD hosts a wxWidgets event loop already; we only need wx initialized
//  in this module's address space. wxEntryStart sets up the wx runtime without
//  taking over the main loop. Modal dialogs then pump their own nested loop, so
//  ShowModal() works even though we never call wxApp::OnRun().
// ---------------------------------------------------------------------------
bool ensureWxInitialized() {
    if (g_wxReady) return true;
    if (wxTheApp == nullptr) {
#if defined(_ELECTRICAL_WIN)
        if (!wxEntryStart(::GetModuleHandle(nullptr)))
            return false;
#else
        int argc = 0;
        if (!wxEntryStart(argc, static_cast<char**>(nullptr)))
            return false;
#endif
    }
    if (wxTheApp && !wxTheApp->IsInitialized())
        wxTheApp->CallOnInit();
    g_wxReady = true;
    return true;
}

void shutdownWx() {
    if (!g_wxReady) return;
    PaletteHost::instance().destroy();
    // Only tear down wx if we started it.
    wxEntryCleanup();
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
    if (!ensureWxInitialized()) return false;
    ConfigDialog dlg(hostParent(), settings);
    if (dlg.ShowModal() == wxID_OK) {
        settings = dlg.result();
        return true;
    }
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

// ---------------------------------------------------------------------------
//  Palette
// ---------------------------------------------------------------------------
void togglePalette() {
    if (!ensureWxInitialized()) return;
    PaletteHost::instance().toggle();
}

void refreshPalette() {
    if (!g_wxReady) return;
    PaletteHost::instance().refresh();
}

} // namespace ui
} // namespace electrical
