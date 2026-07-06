// =============================================================================
//  GeometryHelper.h - Geometry helpers bridging the model's Point3 and BRX
//  AcGe types, plus polygon/polyline utilities used by rooms and routing.
// =============================================================================
#pragma once

#include "Platform.h"
#include "models/Serialization.h"
#include <vector>

namespace electrical {

inline AcGePoint3d toAcGe(const Point3& p) { return AcGePoint3d(p.x, p.y, p.z); }
inline Point3      toPoint3(const AcGePoint3d& p) { return Point3(p.x, p.y, p.z); }

namespace geom {

// Reads the vertices of a closed AcDbPolyline into a Point3 loop.
bool polylineVertices(AcDbObjectId polyId, std::vector<Point3>& out);

// Point-in-polygon test (ray casting), 2D (ignores Z).
bool pointInPolygon(const Point3& pt, const std::vector<Point3>& poly);

// Centroid of a polygon loop (2D).
Point3 centroid(const std::vector<Point3>& poly);

// Closest point on a polyline loop to a given point (for wall snapping).
Point3 closestOnLoop(const Point3& pt, const std::vector<Point3>& poly);

// Manhattan (orthogonal) path between two points, following X then Y - the
// basis for the automatic conduit router's horizontal segments.
std::vector<Point3> orthogonalPath(const Point3& a, const Point3& b);

} // namespace geom
} // namespace electrical
