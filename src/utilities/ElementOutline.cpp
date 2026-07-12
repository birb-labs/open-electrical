#include "utilities/ElementOutline.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace electrical {
namespace outline {

namespace {

// Symbol dimensions, mirroring SymbolFactory's anonymous-namespace constants
// (all derived from the base unit kSymUnit = 0.09). Keep these in sync with
// SymbolFactory.cpp if the symbol sizes ever change.
constexpr double kPi       = 3.14159265358979323846;
constexpr double kSymUnit  = 0.09;
constexpr double kSwR      = kSymUnit;          // switch circle radius
constexpr double kLightR   = 1.6 * kSymUnit;    // ceiling-light circle radius
constexpr double kWallGap  = 0.5 * kSymUnit;    // wall -> nearest symbol pole
constexpr double kOutletBW = kSwR;              // outlet triangle half-base
constexpr double kOutletFilletR = 0.2 * kOutletBW;  // rounded tip radius (matches SymbolFactory)

// Appends a circle (centre cx,cy radius r) sampled into `n` chords.
void appendCircle(std::vector<Segment>& out, double cx, double cy, double r, int n = 48) {
    const double twoPi = 6.283185307179586;
    Point3 prev(cx + r, cy, 0.0);
    for (int i = 1; i <= n; ++i) {
        const double a = twoPi * i / n;
        Point3 cur(cx + r * std::cos(a), cy + r * std::sin(a), 0.0);
        out.emplace_back(prev, cur);
        prev = cur;
    }
}

void appendSeg(std::vector<Segment>& out, double x0, double y0, double x1, double y1) {
    out.emplace_back(Point3(x0, y0, 0.0), Point3(x1, y1, 0.0));
}

// Appends the rounded-corner boundary of ONE outlet triangle (base centred at
// (0,yBase), apex up) as chord segments, mirroring SymbolFactory::addOutletTriangle
// so the attach point can land on the actual curved tip rather than a sharp
// vertex. Base corners are always filleted (kOutletFilletR); the apex is filleted
// only when `roundApex` (the free top tip of the last module in a chain) - a
// junction apex stays sharp so stacked modules still meet at a single point.
void appendRoundedTriangle(std::vector<Segment>& out, double yBase, bool roundApex) {
    const double h = kOutletBW * std::sqrt(3.0);
    // CCW: [0]=left base, [1]=right base, [2]=apex.
    const double vx[3] = { -kOutletBW, kOutletBW, 0.0 };
    const double vy[3] = { yBase, yBase, yBase + h };
    const double rr[3] = { kOutletFilletR, kOutletFilletR, roundApex ? kOutletFilletR : 0.0 };

    std::vector<Point3> loop;
    for (int i = 0; i < 3; ++i) {
        const double Vx = vx[i], Vy = vy[i];
        const int ip = (i + 2) % 3, in = (i + 1) % 3;
        double dpx = vx[ip] - Vx, dpy = vy[ip] - Vy;   // toward previous vertex
        double dnx = vx[in] - Vx, dny = vy[in] - Vy;   // toward next vertex
        const double lp = std::hypot(dpx, dpy), ln = std::hypot(dnx, dny);
        dpx /= lp; dpy /= lp; dnx /= ln; dny /= ln;
        if (rr[i] <= 1e-12) { loop.emplace_back(Vx, Vy, 0.0); continue; }  // sharp
        const double half = std::acos(std::max(-1.0, std::min(1.0, dpx * dnx + dpy * dny))) * 0.5;
        const double t = rr[i] / std::tan(half);        // tangent distance from vertex
        const double tInx = Vx + dpx * t, tIny = Vy + dpy * t;
        const double tOutx = Vx + dnx * t, tOuty = Vy + dny * t;
        double bx = dpx + dnx, by = dpy + dny;          // angle bisector -> arc centre
        const double bl = std::hypot(bx, by); bx /= bl; by /= bl;
        const double d = rr[i] / std::sin(half);
        const double cx = Vx + bx * d, cy = Vy + by * d;
        const double a0 = std::atan2(tIny - cy, tInx - cx);
        const double a1 = std::atan2(tOuty - cy, tOutx - cx);
        double delta = a1 - a0;
        while (delta <= 0.0)       delta += 2.0 * kPi;   // CCW sweep tIn -> tOut
        while (delta > 2.0 * kPi)  delta -= 2.0 * kPi;
        constexpr int segs = 6;
        for (int k = 0; k <= segs; ++k) {
            const double a = a0 + delta * k / segs;
            loop.emplace_back(cx + rr[i] * std::cos(a), cy + rr[i] * std::sin(a), 0.0);
        }
    }
    for (size_t i = 0; i < loop.size(); ++i)
        out.emplace_back(loop[i], loop[(i + 1) % loop.size()]);
}

} // namespace

std::vector<Segment> segments(Kind kind, int modules, bool flushPanel) {
    std::vector<Segment> segs;
    switch (kind) {
        case Kind::Light:
            appendCircle(segs, 0.0, 0.0, kLightR);
            break;
        case Kind::Switch: {
            const double yc = kWallGap + kSwR;
            appendSeg(segs, 0.0, 0.0, 0.0, kWallGap);   // wall tie
            appendCircle(segs, 0.0, yc, kSwR);          // single-section circle
            break;
        }
        case Kind::Panel: {
            const double w = 6.0 * kSwR, h = 2.0 * kSwR;   // 3:1 rectangle
            const double x = w / 2.0;
            // Flush (embutido) panels are recessed ~75% of their height past the
            // wall line, exactly like SymbolFactory::buildPanel's yOff.
            const double y0 = flushPanel ? -0.75 * h : 0.0;
            const double y1 = y0 + h;
            appendSeg(segs, -x, y0,  x, y0);
            appendSeg(segs,  x, y0,  x, y1);
            appendSeg(segs,  x, y1, -x, y1);
            appendSeg(segs, -x, y1, -x, y0);
            break;
        }
        case Kind::Outlet: {
            const int n = std::max(1, std::min(3, modules));
            const double th = kOutletBW * std::sqrt(3.0);   // triangle height
            appendSeg(segs, 0.0, 0.0, 0.0, kWallGap);       // wall stem
            for (int i = 0; i < n; ++i) {
                const double yb = kWallGap + i * th;        // this module's base y
                // Only the last module's apex is free (rounded); inner apexes are
                // junctions with the next module and stay sharp (matches the block).
                appendRoundedTriangle(segs, yb, /*roundApex=*/i + 1 == n);
            }
            break;
        }
    }
    return segs;
}

Point3 closestOnSegments(const std::vector<Segment>& segs, const Point3& target) {
    Point3 best = target;
    double bestD = std::numeric_limits<double>::max();
    for (const auto& s : segs) {
        const Point3& a = s.first;
        const Point3& b = s.second;
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double len2 = dx * dx + dy * dy;
        double t = len2 > 0.0 ? ((target.x - a.x) * dx + (target.y - a.y) * dy) / len2 : 0.0;
        t = std::max(0.0, std::min(1.0, t));
        const Point3 p(a.x + t * dx, a.y + t * dy, 0.0);
        const double d = std::hypot(target.x - p.x, target.y - p.y);
        if (d < bestD) { bestD = d; best = p; }
    }
    return best;
}

Point3 toWorld(const Point3& local, const Point3& origin, double rot) {
    const double c = std::cos(rot), s = std::sin(rot);
    return Point3(origin.x + local.x * c - local.y * s,
                  origin.y + local.x * s + local.y * c,
                  origin.z + local.z);
}

Point3 toLocal(const Point3& world, const Point3& origin, double rot) {
    const double c = std::cos(rot), s = std::sin(rot);
    const double dx = world.x - origin.x, dy = world.y - origin.y;
    return Point3(dx * c + dy * s,       // inverse rotation (transpose)
                  -dx * s + dy * c,
                  world.z - origin.z);
}

Point3 nearestAttachPoint(Kind kind, int modules, const Point3& origin, double rot,
                          const Point3& target, bool flushPanel) {
    const std::vector<Segment> segs = segments(kind, modules, flushPanel);
    const Point3 tLocal = toLocal(target, origin, rot);
    const Point3 pLocal = closestOnSegments(segs, tLocal);
    return toWorld(pLocal, origin, rot);
}

} // namespace outline
} // namespace electrical
