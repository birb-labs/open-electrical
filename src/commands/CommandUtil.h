// =============================================================================
//  CommandUtil.h - Shared helpers for the interactive commands.
// =============================================================================
#pragma once

#include "Platform.h"
#include "models/Serialization.h"
#include <map>
#include <string>
#include <vector>

namespace electrical {

class ProjectData;
class Room;

namespace cmdutil {

// One block-attribute value to stamp onto an inserted symbol (tag -> text).
// Values are matched to the block's attribute definitions BY TAG (case-sensitive,
// tags are uppercase by convention), never by the order they were defined in.
struct SymbolAttr { std::wstring tag; std::wstring value; };

// Collapses a SymbolAttr list into the tag -> value map the matching uses. A
// duplicated tag keeps its last occurrence.
std::map<std::wstring, std::wstring> attrMap(const std::vector<SymbolAttr>& attribs);

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

// Prompts for a point defining a direction from `from` (rubber-band line); sets
// `angleOut` (radians) so that a symbol's local +Y axis (which points away from
// the wall, into the room) aims toward the picked point. Returns false on cancel.
bool pickDirection(const std::wstring& prompt, const Point3& from, double& angleOut);

// Inserts a block reference of `blockName` at `pos` on `layer`. If the block is
// not defined in the drawing, a simple placeholder (circle marker) is drawn
// instead so the workflow never blocks on missing symbol libraries. Returns the
// created entity's handle as a hex string (empty on failure).
std::string insertSymbol(const ACHAR* blockName, const ACHAR* layer,
                         const Point3& pos, double scale = 1.0,
                         double rotation = 0.0,
                         const std::vector<SymbolAttr>& attribs = {});

// Draws a light-weight (2D) polyline through `pts` on `layer`; returns its handle.
// Only X/Y are used - Z is dropped, so this is for flat geometry (room outlines).
std::string drawPolyline(const ACHAR* layer, const std::vector<Point3>& pts,
                         bool closed = false);

// Draws a true 3D polyline through `pts` on `layer`, preserving Z; returns its
// handle. Used for conduits, whose vertical drops (ceiling -> device) differ only
// in Z and would collapse to a point on a 2D polyline.
std::string drawPolyline3D(const ACHAR* layer, const std::vector<Point3>& pts);

// Draws a single 2D segment a->b on `layer` with a DXF `bulge` (0 = straight line,
// else a circular arc; bulge = tan(theta/4)); returns its handle. Used for a
// curved conduit run whose midpoint the user has dragged.
std::string drawArc2D(const ACHAR* layer, const Point3& a, const Point3& b, double bulge);

// Draws a 2D polyline through `pts` on `layer` with an explicit true colour
// (r,g,b in 0..255) overriding the layer colour. Used for the wiring-distribution
// balloon's white ring/arm/rail, whose colour is fixed by NBR convention. Returns
// the created entity's handle.
// `width` > 0 gives the polyline a constant width, so a 2-point run renders as a
// FILLED rectangle (a volumetric bar) rather than a thin line - used for the
// conductor glyphs (F/N/PE/R) drawn as solid bars.
std::string drawColoredPolyline(const ACHAR* layer, const std::vector<Point3>& pts,
                                int r, int g, int b, bool closed = false,
                                double width = 0.0);

// Draws a FILLED rectangle (AcDbSolid) centred at (cx,cy) with half-extents
// (halfW,halfH) on `layer`, in true colour (r,g,b). Returns its handle. Used for
// the volumetric conductor bars - AcDbSolid is the same fill primitive the outlet
// symbols use, so it renders reliably (unlike a wide polyline).
std::string drawSolidRect(const ACHAR* layer, double cx, double cy,
                          double halfW, double halfH, int r, int g, int b);

// Draws single-line text `text` centred at `pos` with cap height `h` on `layer`,
// in true colour (r,g,b). When `mask` is true a filled dark quad is drawn behind
// the glyphs first (so a white label stays legible over the white rail / light
// background) - its handle, plus the text's, are appended to `outHandles`.
void drawLabel(const ACHAR* layer, const Point3& pos, double h,
               const std::wstring& text, int r, int g, int b, bool mask,
               std::vector<std::string>& outHandles);

// Reads the block name of a block reference into `out`. Returns false if `id` is
// not a block reference or can't be opened. Used to classify a picked element by
// its symbol (EL_OUTLET_*, EL_SWITCH_*, EL_LIGHT, EL_PANEL_*).
bool blockRefName(AcDbObjectId id, std::wstring& out);

// Prompts the user to select a single entity. Returns false on cancel/none.
bool pickEntity(const std::wstring& prompt, AcDbObjectId& out);

// Rotation for a wall-mounted symbol. Lets the user select a linear entity
// (line/polyline/arc — any AcDbCurve) that represents the wall: the symbol is
// then constrained to the two perpendiculars (+90/-90 to the wall direction),
// and a second pick chooses which side (the room interior) it faces. Pressing
// Enter instead of selecting a wall falls back to a free angle (pickDirection).
bool pickRotationOnWall(const Point3& pos, double& angleOut);

// Resolves a hex AcDbHandle string (as stored in ElectricalElement::handle) to
// the entity in the working database.
bool entityIdByHandle(const std::string& hexHandle, AcDbObjectId& out);

// Reads the insertion point and rotation of a block reference.
bool blockRefTransform(AcDbObjectId brId, Point3& pos, double& rotation);

// Rewrites the text of a block reference's attributes, matched BY TAG (same
// convention as insertSymbol). Attributes whose tag is absent from `attribs` are
// left untouched. Returns false if the entity is not a block reference.
bool updateBlockAttributes(AcDbObjectId brId, const std::vector<SymbolAttr>& attribs);

// Tag -> value overloads (UTF-8 keys and values), used by the circuit sync pass.
bool updateBlockAttributes(AcDbBlockReference* blockRef,
                           const std::map<std::string, std::string>& tagValues);
bool updateBlockAttributes(AcDbObjectId brId,
                           const std::map<std::string, std::string>& tagValues);

// Erases an entity from the drawing.
bool eraseEntity(AcDbObjectId id);

// Finds the room whose boundary contains `pt` (nullptr if none).
Room* roomAt(ProjectData& project, const Point3& pt);

} // namespace cmdutil
} // namespace electrical
