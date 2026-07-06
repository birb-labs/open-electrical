// =============================================================================
//  UiBridge.h - Thin façade the command layer calls to show UI.
//
//  This isolates every wxWidgets include behind a handful of free functions so
//  the commands/services never pull in wx headers. It also owns the one-time
//  wxWidgets bootstrap (wxEntryStart) needed to run wx windows inside the host,
//  following the brxWxSample integration pattern.
// =============================================================================
#pragma once

namespace electrical {

class ProjectSettings;
class Room;

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

// ---- Main palette (dockable) ----------------------------------------------
void togglePalette();     // EL command
void refreshPalette();    // repopulate after model changes

} // namespace ui
} // namespace electrical
