#include "services/LightingCalculator.h"
#include "utilities/PolyMath.h"

#include <algorithm>
#include <cmath>

namespace electrical {

namespace {
// NBR 5413 / ISO 8995 maintained-illuminance ranges (lux) per room usage:
// {lower, mid, upper}. The task contrast selects which bound applies.
struct LuxRange { double lo, mid, hi; };

LuxRange luxRangeFor(RoomUsage usage) {
    switch (usage) {
        case RoomUsage::Office:      return { 300.0, 500.0, 750.0 };  // work / study
        case RoomUsage::Kitchen:     return { 200.0, 300.0, 500.0 };
        case RoomUsage::ServiceArea: return { 150.0, 200.0, 300.0 };  // laundry, utility
        case RoomUsage::Bathroom:    return { 100.0, 150.0, 200.0 };
        case RoomUsage::Hallway:     return { 100.0, 150.0, 200.0 };  // circulation
        case RoomUsage::DiningRoom:  return { 100.0, 150.0, 200.0 };
        case RoomUsage::LivingRoom:  return { 100.0, 150.0, 300.0 };
        case RoomUsage::Bedroom:     return { 100.0, 150.0, 200.0 };
        case RoomUsage::Closet:      return { 100.0, 150.0, 200.0 };
        case RoomUsage::Garage:      return {  50.0, 100.0, 150.0 };
        case RoomUsage::Balcony:     return {  50.0, 100.0, 150.0 };
        case RoomUsage::Other:       return { 100.0, 150.0, 200.0 };
    }
    return { 100.0, 150.0, 200.0 };
}
} // namespace

double LightingCalculator::targetLux(RoomUsage usage, TaskContrast contrast) {
    const LuxRange r = luxRangeFor(usage);
    switch (contrast) {
        case TaskContrast::Low:  return r.lo;
        case TaskContrast::High: return r.hi;
        case TaskContrast::Medium: break;
    }
    return r.mid;
}

double LightingCalculator::targetLux(RoomUsage usage) {
    return targetLux(usage, TaskContrast::Medium);
}

double LightingCalculator::utilizationFactor(double K, double ceilingRefl) {
    // Saturating curve: CU rises with the room index and asymptotes near the
    // luminaire's max efficiency. Higher ceiling reflectance shifts it up.
    // CU(K) = cuMax * K / (K + k0), clamped to a sane range.
    //
    // Tuned to real direct-luminaire CU tables (rho 70/50/20): a small room
    // (K ~= 0.65) lands near 0.55-0.60 and a large one (K >= 3) near 0.75. The
    // previous k0 = 0.9 was far too pessimistic (~0.29 for a small bedroom),
    // which roughly doubled the luminaire count versus the NBR 5413 hand calc.
    const double cuMax = 0.70 + 0.15 * ceilingRefl;   // ~0.805 for rho_c=0.7
    const double k0    = 0.25;
    double cu = cuMax * (K / (K + k0));
    return std::clamp(cu, 0.30, 0.85);
}

LightingResult LightingCalculator::calculate(const Room& room, Unit unit,
                                             const LightingInput& in) {
    LightingResult res;
    res.requiredLux = (in.targetLuxOverride > 0.0)
                        ? in.targetLuxOverride
                        : targetLux(room.usage, in.contrast);

    // Room area in m^2 (recompute defensively from the boundary).
    Room tmp = room;
    tmp.recomputeArea(unit);
    const double areaM2 = tmp.areaM2;
    res.areaM2 = areaM2;
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
    res.utilizationFactor = (in.cuOverride > 0.0)
        ? in.cuOverride
        : utilizationFactor(res.roomIndex, in.ceilingReflectance);

    // Total flux needed and luminaire count.
    res.totalLumensNeeded =
        (res.requiredLux * areaM2) / (res.utilizationFactor * in.maintenanceFactor);
    res.luminaireCount = std::max(1,
        static_cast<int>(std::ceil(res.totalLumensNeeded / std::max(1.0, in.lumensPerLuminaire))));

    // ---- Layout: adaptive grid clipped to the room polygon ------------------
    // Delegated to the BRX-free polymath module (so it is unit-testable off-CAD);
    // this is what makes L-, U- and trapezoidal rooms work - cells outside the
    // polygon are rejected and the grid densifies until N fit.
    const double z = room.ceilingHeight / m;
    res.positions = polymath::distributeInPolygon(room.boundary, res.luminaireCount, z);

    // Whatever actually fit is the real count (drives achievedLux below).
    if (!res.positions.empty())
        res.luminaireCount = static_cast<int>(res.positions.size());

    // Back-calculate the illuminance actually delivered.
    res.achievedLux = (res.luminaireCount * in.lumensPerLuminaire *
                       res.utilizationFactor * in.maintenanceFactor) / areaM2;
    return res;
}

} // namespace electrical
