// =============================================================================
//  SymbolFactory.h - Programmatic NBR 5444 electrical symbol blocks.
//
//  Rather than shipping a separate symbol .dwg, the plugin defines its block
//  records directly in the drawing on demand. `ensure()` creates the block for
//  a given name if it is not already present, so insertSymbol() always finds a
//  real symbol instead of a placeholder.
//
//  Local convention shared by every symbol: +Y points away from the wall, into
//  the room (the wall itself is the line y=0). Rotation to match the real wall
//  orientation happens at insertion time (block reference rotation), not here.
//
//  Symbols follow the ABNT NBR 5444 conventions:
//    - lighting point (ceiling) : circle (bigger than the switch circle) split
//                                 by a horizontal chord, with a stem from the
//                                 centre to the south pole (3 reserved sectors)
//    - lighting point (wall)    : half-disc, flat side on the wall, split by a
//                                 segment from the wall-centre to the apex
//    - switch (1/2/3 sections)  : circle offset from the wall with a tie back
//                                 to the wall from its nearest pole; sections
//                                 are equal slices, one boundary at that pole
//    - three-way / four-way     : as 1/2-section, fully / half solid-filled
//    - bell push button         : as 1-section, with a small filled centre dot
//    - outlet (low/med/high)    : equilateral triangle offset from the wall
//                                 with a stem to the wall; fill encodes height
//                                 (empty/right-half/full); modules of a multi-
//                                 gang outlet chain apex-to-base
//    - outlet + switch combo    : wall stem + 1-3 medium-fill outlet triangle(s)
//                                 chained as usual, then the switch circle
//                                 attached at the LAST triangle's tip (apex,
//                                 away from the wall) - no wall tie of its own,
//                                 since it is not the element touching the wall
//    - load panel (surface)     : 3:1 rectangle, straight corners, flush
//                                 against the wall, with a right triangle
//                                 whose hypotenuse is the rectangle's
//                                 secondary diagonal
//    - load panel (flush)       : identical, ~75% recessed past the wall line
//
//  Geometry is drawn on layer "0" (BYBLOCK colour) so an inserted reference
//  inherits the target layer's colour; the insertion scale from the project
//  unit is applied at insert time.
// =============================================================================
#pragma once

#include "Platform.h"
#include <vector>

namespace electrical {

class SymbolFactory {
public:
    // ---- Standard block names -------------------------------------------
    static const ACHAR* kLight;        // "EL_LIGHT"            ceiling lighting point
    static const ACHAR* kLightWall;    // "EL_LIGHT_WALL"        wall luminaire (arandela)

    static const ACHAR* kOutletLow;    // "EL_OUTLET_L"          outlet, low  (empty triangle)
    static const ACHAR* kOutletMed;    // "EL_OUTLET_M"          outlet, medium (half fill)
    static const ACHAR* kOutletHigh;   // "EL_OUTLET_H"          outlet, high (full fill)
    // Multi-gang outlets: any other block matching "EL_OUTLET_" + 1-3 groups of
    // L/M/H separated by '_' (e.g. "EL_OUTLET_L_H") is built on demand by
    // chaining a triangle per group, apex-to-base, away from the wall.

    static const ACHAR* kOutletSwitch2;       // "EL_OUTLET_SW2"      outlet (med) + 2-section switch
    static const ACHAR* kOutletDuplexSwitch1; // "EL_OUTLET_DUP_SW1"  outlet (med) + 1-section switch

    static const ACHAR* kSwitch1;      // "EL_SWITCH_1"     1 section
    static const ACHAR* kSwitch2;      // "EL_SWITCH_2"     2 sections
    static const ACHAR* kSwitch3;      // "EL_SWITCH_3"     3 sections
    static const ACHAR* kSwitch3Way;   // "EL_SWITCH_3WAY"  parallel / three-way (filled)
    static const ACHAR* kSwitch4Way;   // "EL_SWITCH_4WAY"  intermediate / four-way (half)
    static const ACHAR* kSwitchBell;   // "EL_SWITCH_BELL"  bell push button

    static const ACHAR* kPanelSurface; // "EL_PANEL_SURFACE" load panel, aparente
    static const ACHAR* kPanelFlush;   // "EL_PANEL_FLUSH"    load panel, embutido

    // Geometry revision stamped into every block definition we create (XData,
    // under kXDataAppName). BUMP THIS whenever any symbol's geometry changes:
    // ensure() compares it against the stamp cached in the drawing and auto-
    // redefines any block left over from an older plugin build, so a rebuilt
    // plugin's new geometry shows up even in drawings that already contain the
    // old blocks (block definitions live in the .dwg, not in the .lrx module).
    static const int kSymbolVersion;

    // Ensures the block named `blockName` exists in `db`; creates it if it is
    // one of the known plugin symbols (including dynamically-named multi-gang
    // outlet chains). Returns true if present afterwards.
    static bool ensure(AcDbDatabase* db, const ACHAR* blockName);

    // Creates every fixed-name plugin symbol up front (used by the palette /
    // config warm-up). Dynamically-named outlet chains are created lazily by
    // ensure() the first time insertSymbol() asks for one.
    static void ensureAll(AcDbDatabase* db);

    // Builds the block name for a multi-gang outlet from its per-module
    // mounting heights in metres (1-3 entries), bucketed the same way as
    // outletBlockForHeight in ElementCommands.cpp (<=0.6 low, <1.5 medium,
    // else high). Returned string is one of the fixed kOutletLow/Med/High
    // constants for a single module, or a dynamic "EL_OUTLET_x_y[_z]" name.
    static std::wstring outletChainBlockName(const std::vector<double>& moduleHeightsM);

private:
    static bool blockExists(AcDbDatabase* db, const ACHAR* name);
    static AcDbObjectId createBlock(AcDbDatabase* db, const ACHAR* name);

    // True if `name` is one of our fixed symbols or a valid dynamic outlet chain.
    static bool isOurBlock(const ACHAR* name);
    // Appends the geometry for `name` into an (empty) block record.
    static void buildGeometry(AcDbBlockTableRecord* btr, const ACHAR* name);
    // Writes / reads the kSymbolVersion XData stamp on a block record.
    static void writeVersionStamp(AcDbBlockTableRecord* btr);
    static int  readVersionStamp(AcDbBlockTableRecord* btr);
    // Version stamped on the existing block `name`, or -1 if absent/unstamped.
    static int  blockStampVersion(AcDbDatabase* db, const ACHAR* name);
    // Clears and rebuilds an existing block record (updates all its INSERTs).
    static void redefineBlock(AcDbDatabase* db, const ACHAR* name);

    enum class SwitchFill { None, Half, Full };

    // Geometry builders (append entities to the record).
    static void buildLight(AcDbBlockTableRecord* btr);
    static void buildLightWall(AcDbBlockTableRecord* btr);
    // `sections` = number of drawn sectors (dividers); `commandCount` = number of
    // COMANDO<n> attributes (the three-way/four-way variants draw 1-2 sectors but
    // carry a single command).
    static void buildSwitch(AcDbBlockTableRecord* btr, int sections, SwitchFill fill,
                            int commandCount);
    static void buildSwitchBell(AcDbBlockTableRecord* btr);
    static void buildOutletChain(AcDbBlockTableRecord* btr, const std::vector<int>& fills);
    static void buildOutletWithSwitch(AcDbBlockTableRecord* btr, int outletModules, int switchSections);
    static void buildPanel(AcDbBlockTableRecord* btr, bool flush);
};

} // namespace electrical
