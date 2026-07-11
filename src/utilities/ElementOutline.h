// =============================================================================
//  ElementOutline.h - Per-component connection outline, free of any BRX / AcGe
//  dependency (works on the model's plain Point3 so it can be unit-tested).
//
//  Each electrical component exposes a simple outline that a conduit is allowed
//  to attach to (NBR 5444 body of the symbol), mirroring SymbolFactory geometry:
//    * Light  -> a circle of the ceiling-light radius.
//    * Outlet -> the triangle chain of a LOW outlet with the same module count.
//    * Switch -> a single-section switch circle (+ its wall tie).
//    * Panel  -> a 3:1 rectangle.
//  The outline lives in the symbol's LOCAL frame (+Y points away from the wall,
//  the insertion point is the origin); callers transform to/from world with the
//  block reference's position + rotation.
// =============================================================================
#pragma once

#include "models/Serialization.h"   // Point3 (BRX-free)
#include <utility>
#include <vector>

namespace electrical {
namespace outline {

enum class Kind { Light, Outlet, Switch, Panel };

// A boundary segment (a, b) in the component's local frame.
using Segment = std::pair<Point3, Point3>;

// The outline segments for `kind`. `modules` is the outlet module count (1..3),
// ignored for the other kinds.
std::vector<Segment> segments(Kind kind, int modules);

// Closest point on a set of segments to `target` (all in the same frame). Returns
// `target` itself when `segs` is empty.
Point3 closestOnSegments(const std::vector<Segment>& segs, const Point3& target);

// Local<->world transform for a symbol placed at `origin` with rotation `rot`
// (radians, CCW). Local +X/+Y are rotated by `rot` then translated by `origin`.
Point3 toWorld(const Point3& local, const Point3& origin, double rot);
Point3 toLocal(const Point3& world, const Point3& origin, double rot);

// Convenience: the world-space point on `kind`'s outline (placed at origin/rot)
// nearest to world-space `target`.
Point3 nearestAttachPoint(Kind kind, int modules, const Point3& origin, double rot,
                          const Point3& target);

} // namespace outline
} // namespace electrical
