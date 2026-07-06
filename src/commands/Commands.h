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
void insertLightAuto();        // EL-LIGHT-AUTO
void insertOutlet();           // EL-OUTLET
void insertOutletAuto();       // EL-OUTLET-AUTO
void insertSwitch();           // EL-SWITCH
void insertSwitchAuto();       // EL-SWITCH-AUTO
void insertPanel();            // EL-PANEL

// ---- Distribution / routing ----------------------------------------------
void distributeCircuits();     // EL-CIRCUIT
void distributeCircuitsAuto(); // EL-CIRCUIT-AUTO
void routeConduit();           // EL-CONDUIT
void routeConduitAuto();       // EL-CONDUIT-AUTO
void routeWire();              // EL-WIRE
void routeWireAuto();          // EL-WIRE-AUTO

// ---- Documents / control --------------------------------------------------
void generateReport();         // EL-REPORT
void undoLast();               // EL-UNDO

// The full command table (defined in Commands.cpp).
const CommandDef* commandTable(size_t& count);

} // namespace commands
} // namespace electrical
