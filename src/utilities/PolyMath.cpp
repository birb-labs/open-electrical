#include "utilities/PolyMath.h"

#include <algorithm>
#include <cmath>

namespace electrical {
namespace polymath {

double polygonArea(const std::vector<Point3>& poly) {
    const size_t n = poly.size();
    if (n < 3) return 0.0;
    double a = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Point3& p = poly[i];
        const Point3& q = poly[(i + 1) % n];
        a += (p.x * q.y) - (q.x * p.y);
    }
    return std::fabs(a) * 0.5;
}

bool pointInPolygon(const Point3& pt, const std::vector<Point3>& poly) {
    bool inside = false;
    const size_t n = poly.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double yi = poly[i].y, yj = poly[j].y;
        const double xi = poly[i].x, xj = poly[j].x;
        const bool crosses = ((yi > pt.y) != (yj > pt.y)) &&
            (pt.x < (xj - xi) * (pt.y - yi) / (yj - yi + 1e-12) + xi);
        if (crosses) inside = !inside;
    }
    return inside;
}

std::vector<Point3> distributeInPolygon(const std::vector<Point3>& poly,
                                        int n, double z) {
    std::vector<Point3> out;
    if (poly.size() < 3 || n <= 0) return out;

    double minx = poly[0].x, maxx = minx, miny = poly[0].y, maxy = miny;
    for (const auto& p : poly) {
        minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
    }
    const double bboxW = maxx - minx, bboxH = maxy - miny;
    if (bboxW <= 0.0 || bboxH <= 0.0) return out;

    // Cell centres of a rows x cols grid over the bbox that fall inside the poly.
    auto cellCentresInside = [&](int rows, int cols) {
        std::vector<Point3> pts;
        const double dx = bboxW / cols;
        const double dy = bboxH / rows;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                Point3 p(minx + (c + 0.5) * dx, miny + (r + 0.5) * dy, z);
                if (pointInPolygon(p, poly)) pts.push_back(p);
            }
        return pts;
    };

    // Near-square grid matching the bbox aspect, sized so a convex room already
    // holds N cells.
    const double aspect = bboxW / bboxH;
    int cols = std::max(1, static_cast<int>(std::round(std::sqrt(n * aspect))));
    int rows = std::max(1, static_cast<int>(std::ceil(n / static_cast<double>(cols))));
    while (rows * cols < n) ++cols;

    // Concavity can reject enough cells to leave fewer than N inside: densify the
    // grid until it holds N, bounded so a degenerate boundary cannot spin.
    std::vector<Point3> inside = cellCentresInside(rows, cols);
    constexpr int kMaxRefinements = 8;
    for (int i = 0; i < kMaxRefinements && static_cast<int>(inside.size()) < n; ++i) {
        ++rows; ++cols;
        inside = cellCentresInside(rows, cols);
    }

    if (static_cast<int>(inside.size()) <= n) {
        out = std::move(inside);
    } else {
        // Overshoot: take N with a uniform stride over the row-major scan so the
        // selection stays spread across the whole polygon.
        const double stride = inside.size() / static_cast<double>(n);
        out.reserve(n);
        for (int i = 0; i < n; ++i)
            out.push_back(inside[static_cast<size_t>(i * stride)]);
    }

    // Too small/thin for even one cell centre: fall back to the bbox centre if it
    // is inside the polygon.
    if (out.empty()) {
        const Point3 c((minx + maxx) * 0.5, (miny + maxy) * 0.5, z);
        if (pointInPolygon(c, poly)) out.push_back(c);
    }
    return out;
}

namespace {
Point3 lerpPt(const Point3& a, const Point3& b, double t) {
    return Point3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}
double segLen(const Point3& a, const Point3& b) {
    const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
} // namespace

std::vector<Point3> trimPathEnds(const std::vector<Point3>& path,
                                 double rStart, double rEnd) {
    if (path.size() < 2) return path;
    rStart = std::max(0.0, rStart);
    rEnd   = std::max(0.0, rEnd);

    double total = 0.0;
    for (size_t i = 1; i < path.size(); ++i) total += segLen(path[i - 1], path[i]);
    if (total <= rStart + rEnd) return path;   // elements overlap: never drop a run

    const double sStart = rStart;
    const double sEnd   = total - rEnd;

    // Point at arc length `s` measured from the path's start.
    auto pointAt = [&](double s) -> Point3 {
        if (s <= 0.0) return path.front();
        double acc = 0.0;
        for (size_t i = 1; i < path.size(); ++i) {
            const double l = segLen(path[i - 1], path[i]);
            if (acc + l >= s) {
                const double t = l > 1e-12 ? (s - acc) / l : 0.0;
                return lerpPt(path[i - 1], path[i], std::min(std::max(t, 0.0), 1.0));
            }
            acc += l;
        }
        return path.back();
    };

    constexpr double eps = 1e-9;
    std::vector<Point3> out;
    out.push_back(pointAt(sStart));
    // Keep every original interior vertex whose arc position lies strictly between
    // the two trimmed ends, so the corner of an L-shaped run is preserved.
    double cum = 0.0;
    for (size_t i = 1; i < path.size(); ++i) {
        cum += segLen(path[i - 1], path[i]);   // arc length at vertex i
        if (i < path.size() - 1 && cum > sStart + eps && cum < sEnd - eps)
            out.push_back(path[i]);
    }
    out.push_back(pointAt(sEnd));
    return out;
}

} // namespace polymath
} // namespace electrical
