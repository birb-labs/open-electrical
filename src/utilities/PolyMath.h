// =============================================================================
//  PolyMath.h - Pure 2D polygon math, free of any BRX / AcGe dependency.
//
//  Split out from GeometryHelper (which bridges to AcDb) so the lighting
//  calculation and its geometry can be unit-tested off-CAD. Everything here
//  works on the model's plain `Point3` and links without the SDK.
// =============================================================================
#pragma once

#include "models/Serialization.h"   // Point3 (BRX-free)
#include <vector>

namespace electrical {
namespace polymath {

// Signed-area magnitude via the shoelace formula, in the polygon's own units.
double polygonArea(const std::vector<Point3>& poly);

// Ray-casting point-in-polygon test (2D, ignores Z). Points exactly on an edge
// are not guaranteed either way (numerically ambiguous); interiors are stable.
bool pointInPolygon(const Point3& pt, const std::vector<Point3>& poly);

// Distributes up to `n` points inside `poly` on an adaptive grid clipped to the
// polygon: an axis-aligned grid over the bounding box, keeping only the cell
// centres that fall inside. The grid is densified until at least `n` fit; if it
// overshoots, the result is uniformly strided down to `n` so the selection stays
// spread. Works for any simple polygon (convex, L, U, trapezoidal). `z` is copied
// into every returned point. Returns fewer than `n` only when the polygon is too
// small/thin to hold that many cell centres.
std::vector<Point3> distributeInPolygon(const std::vector<Point3>& poly,
                                        int n, double z = 0.0);

// Trims a poly-path so it starts `rStart` and ends `rEnd` of arc length inside
// its original endpoints, walking along the path (not straight-line). Used by the
// conduit router so a run leaves the boundary of its source element and stops at
// the boundary of its destination element instead of piercing either symbol's
// footprint. Endpoints are moved along the first/last segment directions, so a
// trimmed end sits exactly `rStart`/`rEnd` from the original along that segment.
// If the path is too short to absorb both trims (overlapping elements), the
// original path is returned unchanged so a run is never lost. Z is interpolated
// along with X/Y.
std::vector<Point3> trimPathEnds(const std::vector<Point3>& path,
                                 double rStart, double rEnd);

} // namespace polymath
} // namespace electrical
