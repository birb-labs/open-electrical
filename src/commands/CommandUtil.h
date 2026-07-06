// =============================================================================
//  CommandUtil.h - Shared helpers for the interactive commands.
// =============================================================================
#pragma once

#include "Platform.h"
#include "models/Serialization.h"
#include <string>

namespace electrical {

class ProjectData;
class Room;

namespace cmdutil {

// Ensures the plugin's layers exist (lighting, outlets, switches, panels,
// conduits, wiring, text). Returns the layer id for `name`.
AcDbObjectId ensureLayer(const ACHAR* name, short colorIndex);

// Standard layer names used by the plugin.
extern const ACHAR* kLayerLighting;
extern const ACHAR* kLayerOutlets;
extern const ACHAR* kLayerSwitches;
extern const ACHAR* kLayerPanels;
extern const ACHAR* kLayerConduit;
extern const ACHAR* kLayerWiring;
extern const ACHAR* kLayerRooms;
extern const ACHAR* kLayerText;

// Prompts the user to pick a point. Returns false on cancel.
bool pickPoint(const std::wstring& prompt, Point3& out);

// Inserts a block reference of `blockName` at `pos` on `layer`. If the block is
// not defined in the drawing, a simple placeholder (circle marker) is drawn
// instead so the workflow never blocks on missing symbol libraries. Returns the
// created entity's handle as a hex string (empty on failure).
std::string insertSymbol(const ACHAR* blockName, const ACHAR* layer,
                         const Point3& pos, double scale = 1.0);

// Draws a light-weight polyline through `pts` on `layer`; returns its handle.
std::string drawPolyline(const ACHAR* layer, const std::vector<Point3>& pts,
                         bool closed = false);

// Finds the room whose boundary contains `pt` (nullptr if none).
Room* roomAt(ProjectData& project, const Point3& pt);

} // namespace cmdutil
} // namespace electrical
