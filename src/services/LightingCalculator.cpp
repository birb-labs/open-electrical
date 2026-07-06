#include "services/LightingCalculator.h"
#include "utilities/GeometryHelper.h"

#include <algorithm>
#include <cmath>

namespace electrical {

double LightingCalculator::targetLux(RoomUsage usage) {
    // Representative maintained illuminance targets (lux).
    switch (usage) {
        case RoomUsage::LivingRoom:  return 150.0;
        case RoomUsage::Bedroom:     return 150.0;
        case RoomUsage::Kitchen:     return 300.0;
        case RoomUsage::Bathroom:    return 200.0;
        case RoomUsage::Hallway:     return 100.0;
        case RoomUsage::ServiceArea: return 200.0;
        case RoomUsage::Garage:      return 100.0;
        case RoomUsage::Office:      return 500.0;
        case RoomUsage::DiningRoom:  return 200.0;
        case RoomUsage::Balcony:     return 100.0;
        case RoomUsage::Closet:      return 150.0;
        case RoomUsage::Other:       return 150.0;
    }
    return 150.0;
}

double LightingCalculator::utilizationFactor(double K, double ceilingRefl) {
    // Saturating curve: CU rises with the room index and asymptotes near the
    // luminaire's max efficiency. Higher ceiling reflectance shifts it up.
    // CU(K) = cuMax * K / (K + k0), clamped to a sane range.
    const double cuMax = 0.55 + 0.20 * ceilingRefl;   // ~0.69 for rho_c=0.7
    const double k0    = 0.9;
    double cu = cuMax * (K / (K + k0));
    return std::clamp(cu, 0.20, 0.85);
}

LightingResult LightingCalculator::calculate(const Room& room, Unit unit,
                                             const LightingInput& in) {
    LightingResult res;
    res.requiredLux = targetLux(room.usage);

    // Room area in m^2 (recompute defensively from the boundary).
    Room tmp = room;
    tmp.recomputeArea(unit);
    const double areaM2 = tmp.areaM2;
    if (areaM2 <= 0.0 || room.boundary.size() < 3)
        return res;

    // Bounding box (drawing units) for the room index and the grid layout.
    double minx = room.boundary[0].x, maxx = minx;
    double miny = room.boundary[0].y, maxy = miny;
    for (const auto& p : room.boundary) {
        minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
    }
    const double m  = toMeters(unit);
    const double Lm = (maxx - minx) * m;   // length in meters
    const double Wm = (maxy - miny) * m;   // width in meters

    // Mounting height above the work plane.
    const double Hm = std::max(0.3, room.ceilingHeight - in.workPlaneHeight);

    // Room index K = (L*W) / (H*(L+W)).
    res.roomIndex = (Lm * Wm) / (Hm * (Lm + Wm) + 1e-9);
    res.utilizationFactor = utilizationFactor(res.roomIndex, in.ceilingReflectance);

    // Total flux needed and luminaire count.
    res.totalLumensNeeded =
        (res.requiredLux * areaM2) / (res.utilizationFactor * in.maintenanceFactor);
    res.luminaireCount = std::max(1,
        static_cast<int>(std::ceil(res.totalLumensNeeded / std::max(1.0, in.lumensPerLuminaire))));

    // Choose a near-square grid (rows x cols) matching the room aspect ratio.
    const double aspect = (Wm > 1e-6) ? (Lm / Wm) : 1.0;
    int cols = std::max(1, static_cast<int>(std::round(std::sqrt(res.luminaireCount * aspect))));
    int rows = std::max(1, static_cast<int>(std::ceil(res.luminaireCount / static_cast<double>(cols))));
    // Keep grid capacity >= count.
    while (rows * cols < res.luminaireCount) ++cols;

    // Lay luminaires on cell centres, keeping only those inside the room.
    const double dx = (maxx - minx) / cols;
    const double dy = (maxy - miny) / rows;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Point3 pos(minx + (c + 0.5) * dx, miny + (r + 0.5) * dy,
                       room.ceilingHeight / m);
            if (geom::pointInPolygon(pos, room.boundary))
                res.positions.push_back(pos);
            if (static_cast<int>(res.positions.size()) >= res.luminaireCount)
                break;
        }
        if (static_cast<int>(res.positions.size()) >= res.luminaireCount) break;
    }
    // If concavity rejected some cells, drop the count to what actually fit.
    if (!res.positions.empty())
        res.luminaireCount = static_cast<int>(res.positions.size());

    // Back-calculate the illuminance actually delivered.
    res.achievedLux = (res.luminaireCount * in.lumensPerLuminaire *
                       res.utilizationFactor * in.maintenanceFactor) / areaM2;
    return res;
}

} // namespace electrical
