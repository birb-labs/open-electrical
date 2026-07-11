// =============================================================================
//  Commands.h - Declarations of every EL- command handler.
//
//  Command *global* names contain a hyphen (EL-CONFIG, EL-ROOM, ...), which is
//  not a legal C++ identifier, so the ACED_ARXCOMMAND_ENTRY_AUTO macros cannot
//  be used. Instead main.cpp registers these free functions manually with
//  acedRegCmds->addCommand(group, globalName, localName, flags, fn). Global
//  names never carry a leading underscore (BricsCAD honours the underscore only
//  on the local name).
// =============================================================================
#pragma once

#include "Platform.h"

namespace electrical {

class ElectricalElement;

namespace commands {

// A registerable command: the global name typed at the prompt + its handler.
struct CommandDef {
    const ACHAR* globalName;   // e.g. "EL-CONFIG"
    const ACHAR* localName;    // localized alias (kept == global for now)
    void (*fn)();
    int  flags;                // ACRX_CMD_* flags
};

// ---- Palette / main -------------------------------------------------------
void showPalette();            // EL

// ---- Configuration --------------------------------------------------------
void configProject();          // EL-CONFIG

// ---- Rooms ----------------------------------------------------------------
void insertRoom();             // EL-ROOM

// ---- Elements -------------------------------------------------------------
void insertLight();            // EL-LIGHT
void insertOutlet();           // EL-OUTLET
void insertOutletAuto();       // EL-OUTLET-AUTO
void insertSwitch();           // EL-SWITCH
void insertSwitchAuto();       // EL-SWITCH-AUTO
void insertPanel();            // EL-PANEL
void calcLightingAuto();       // EL-CALC-LIGHT (lumen method + auto placement)
void editElement();            // EL-EDIT (edit a placed component)

// Rewrites an element's block attributes (matched by tag) from its model state.
// Defined in ElementCommands.cpp, where the per-type attribute mapping lives.
void syncElementAttributes(ElectricalElement& e);

// ---- Distribution / routing ----------------------------------------------
void distributeCircuits();     // EL-CIRCUIT
void distributeCircuitsAuto(); // EL-CIRCUIT-AUTO
void routeConduit();           // EL-CONDUIT
void routeConduitAuto();       // EL-CONDUIT-AUTO
void editConduit();            // EL-CONDUIT-EDIT (move endpoint / curve a run)
void routeAllAuto();           // EL-ROUTE-AUTO (route conduits + pull wires)
void routeWire();              // EL-WIRE
void routeWireAuto();          // EL-WIRE-AUTO
void flipWiring();             // EL-WIRE-FLIP

// ---- Removal --------------------------------------------------------------
void deleteSelected();         // EL-DEL (components + conduits)
void deleteRoom();             // EL-DEL-ROOM (a room boundary, from a list)

// ---- Documents / control --------------------------------------------------
void generateReport();         // EL-REPORT
void generateLoadSchedule();   // EL-LOAD-SCHEDULE
void generateSingleLine();     // EL-SINGLE-LINE
void undoLast();               // EL-UNDO

// The full command table (defined in Commands.cpp).
const CommandDef* commandTable(size_t& count);

} // namespace commands
} // namespace electrical
