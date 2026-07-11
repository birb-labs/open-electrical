// =============================================================================
//  UiBridge.h - Thin façade the command layer calls to show UI.
//
//  This isolates every wxWidgets include behind a handful of free functions so
//  the commands/services never pull in wx headers. It also owns the one-time
//  wxWidgets bootstrap (wxInitialize, embedded mode) needed to run wx windows
//  inside the host, following the brxWxSample integration pattern.
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace electrical {

class ProjectSettings;
class Room;
class LightPoint;
class Outlet;
class Switch;
class Panel;

namespace ui {

// Ensures wxWidgets is initialized and attached to the host event loop. Safe to
// call repeatedly; the first call does the real work. Returns false if wx could
// not be initialized (in which case callers fall back to DCL/command prompts).
bool ensureWxInitialized();

// Called on module unload to tear wxWidgets down cleanly.
void shutdownWx();

// ---- Dialogs (modal, wxWidgets) -------------------------------------------
// Shows the project-settings dialog. Returns true if the user accepted, with
// `settings` updated in place.
bool showConfigDialog(ProjectSettings& settings);

// Shows the room-attributes dialog. Returns true on accept.
bool showRoomDialog(Room& room);

// Modal single-choice list (used by EL-DEL-ROOM): shows `items`, sets `outIndex`
// to the picked row. Returns false if cancelled or wx is unavailable.
bool showRoomPicker(const std::vector<std::string>& items, int& outIndex);

// EL-CALC-LIGHT run options: which room to process (-1 = all) and the chosen
// luminaire (flux + power). All the other luminotechnical parameters live on the
// Room itself (reflectances, contrast, MF, CU, heights).
struct LightingRunOptions {
    int    roomIndex = -1;      // index into the room list, or -1 for "All rooms"
    double lumens    = 1800.0;  // luminous flux of one fitting
    double watts     = 18.0;    // power of one fitting
};

// Room-selection + luminaire dialog shown by EL-CALC-LIGHT. `roomNames` is the
// list the user chooses from (an "All rooms" entry is added by the dialog).
// Returns true on accept, with `opt` filled in.
bool showLightingRunDialog(const std::vector<std::string>& roomNames,
                           LightingRunOptions& opt);

// ---- Element configuration dialogs (shown on manual insertion) ------------
bool showLightDialog(LightPoint& light);
bool showOutletDialog(Outlet& outlet);
bool showSwitchDialog(Switch& sw);
bool showPanelDialog(Panel& panel);

// ---- Main palette (dockable) ----------------------------------------------
void togglePalette();     // EL command
void refreshPalette();    // repopulate after model changes

} // namespace ui
} // namespace electrical
