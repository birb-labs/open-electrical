// =============================================================================
//  SymbolFactory.h - Programmatic NBR 5444 electrical symbol blocks.
//
//  Rather than shipping a separate symbol .dwg, the plugin defines its block
//  records directly in the drawing on demand. `ensure()` creates the block for
//  a given name if it is not already present, so insertSymbol() always finds a
//  real symbol instead of a placeholder.
//
//  Symbols are drawn around the block origin (0,0) in drawing units (~0.1-0.5
//  units) on layer "0" so the inserted reference inherits the target layer's
//  colour. The insertion scale from the project unit is applied at insert time.
// =============================================================================
#pragma once

#include "Platform.h"

namespace electrical {

class SymbolFactory {
public:
    // Standard block names (must match the names used by the element commands).
    static const ACHAR* kLight;    // "EL_LIGHT"   ceiling lighting point
    static const ACHAR* kOutlet;   // "EL_OUTLET"  socket outlet
    static const ACHAR* kSwitch;   // "EL_SWITCH"  wall switch
    static const ACHAR* kPanel;    // "EL_PANEL"   load panel

    // Ensures the block named `blockName` exists in `db`; creates it if it is
    // one of the known plugin symbols. Returns true if the block is present
    // afterwards (existing or freshly created).
    static bool ensure(AcDbDatabase* db, const ACHAR* blockName);

    // Creates all four plugin symbols up front (used by the palette / config).
    static void ensureAll(AcDbDatabase* db);

private:
    static bool blockExists(AcDbDatabase* db, const ACHAR* name);
    static AcDbObjectId createBlock(AcDbDatabase* db, const ACHAR* name);

    // Individual symbol geometry builders (append entities to the record).
    static void buildLight(AcDbBlockTableRecord* btr);
    static void buildOutlet(AcDbBlockTableRecord* btr);
    static void buildSwitch(AcDbBlockTableRecord* btr);
    static void buildPanel(AcDbBlockTableRecord* btr);
};

} // namespace electrical
