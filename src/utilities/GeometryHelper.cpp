#include "utilities/GeometryHelper.h"
#include <cmath>
#include <limits>

namespace electrical {
namespace geom {

bool polylineVertices(AcDbObjectId polyId, std::vector<Point3>& out) {
    out.clear();
    AcDbPolyline* pl = nullptr;
    if (acdbOpenObject(pl, polyId, AcDb::kForRead) != Acad::eOk || !pl)
        return false;

    const unsigned int n = pl->numVerts();
    for (unsigned int i = 0; i < n; ++i) {
        AcGePoint3d p;
        pl->getPointAt(i, p);
        out.push_back(toPoint3(p));
    }
    pl->close();
    return out.size() >= 3;
}

bool pointInPolygon(const Point3& pt, const std::vector<Point3>& poly) {
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double yi = poly[i].y, yj = poly[j].y;
        const double xi = poly[i].x, xj = poly[j].x;
        const bool crosses = ((yi > pt.y) != (yj > pt.y)) &&
            (pt.x < (xj - xi) * (pt.y - yi) / (yj - yi + 1e-12) + xi);
        if (crosses) inside = !inside;
    }
    return inside;
}

Point3 centroid(const std::vector<Point3>& poly) {
    // Area-weighted centroid; falls back to vertex average for degenerate loops.
    double a = 0.0, cx = 0.0, cy = 0.0;
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Point3& p = poly[i];
        const Point3& q = poly[(i + 1) % n];
        const double cross = p.x * q.y - q.x * p.y;
        a  += cross;
        cx += (p.x + q.x) * cross;
        cy += (p.y + q.y) * cross;
    }
    if (std::fabs(a) < 1e-9) {
        Point3 avg;
        for (const auto& p : poly) { avg.x += p.x; avg.y += p.y; }
        if (n) { avg.x /= n; avg.y /= n; }
        return avg;
    }
    a *= 0.5;
    return Point3(cx / (6.0 * a), cy / (6.0 * a), 0.0);
}

Point3 closestOnLoop(const Point3& pt, const std::vector<Point3>& poly) {
    Point3 best;
    double bestD = std::numeric_limits<double>::max();
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Point3& a = poly[i];
        const Point3& b = poly[(i + 1) % n];
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double len2 = dx * dx + dy * dy;
        double t = len2 > 0 ? ((pt.x - a.x) * dx + (pt.y - a.y) * dy) / len2 : 0.0;
        t = std::max(0.0, std::min(1.0, t));
        Point3 proj(a.x + t * dx, a.y + t * dy, 0.0);
        const double d = std::hypot(pt.x - proj.x, pt.y - proj.y);
        if (d < bestD) { bestD = d; best = proj; }
    }
    return best;
}

std::vector<Point3> orthogonalPath(const Point3& a, const Point3& b) {
    // L-shaped path: travel along X to b.x, then along Y to b.y.
    std::vector<Point3> path;
    path.push_back(a);
    if (std::fabs(a.x - b.x) > 1e-9)
        path.emplace_back(b.x, a.y, a.z);
    path.push_back(b);
    return path;
}

} // namespace geom
} // namespace electrical
