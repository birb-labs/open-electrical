#include "models/Room.h"
#include <cmath>

namespace electrical {

void Room::recomputeArea(Unit unit) {
    // Shoelace formula in drawing units, then scale to square meters.
    const size_t n = boundary.size();
    if (n < 3) { areaM2 = 0.0; return; }
    double a = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Point3& p = boundary[i];
        const Point3& q = boundary[(i + 1) % n];
        a += (p.x * q.y) - (q.x * p.y);
    }
    const double m = toMeters(unit);
    areaM2 = std::fabs(a) * 0.5 * (m * m);
}

double Room::perimeterM(Unit unit) const {
    const size_t n = boundary.size();
    if (n < 2) return 0.0;
    double per = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Point3& p = boundary[i];
        const Point3& q = boundary[(i + 1) % n];
        per += std::hypot(q.x - p.x, q.y - p.y);
    }
    return per * toMeters(unit);
}

void Room::serialize(PropertyBag& bag) const {
    bag.putText("type", typeTag());
    bag.putInt("id", id);
    bag.putText("name", name);
    bag.putEnum("usage", usage);
    bag.putEnum("function", function);
    bag.putReal("ceilingHeight", ceilingHeight);
    bag.putReal("areaM2", areaM2);
    bag.putText("boundaryHandle", boundaryHandle);

    bag.putInt("vertexCount", static_cast<int64_t>(boundary.size()));
    for (size_t i = 0; i < boundary.size(); ++i)
        bag.putPoint("v." + std::to_string(i), boundary[i]);

    bag.putInt("openingCount", static_cast<int64_t>(openings.size()));
    for (size_t i = 0; i < openings.size(); ++i) {
        const auto& o = openings[i];
        const std::string k = "op." + std::to_string(i) + ".";
        bag.putInt(k + "kind", static_cast<int64_t>(o.kind));
        bag.putPoint(k + "start", o.start);
        bag.putPoint(k + "end", o.end);
        bag.putReal(k + "sill", o.sillHeight);
    }
}

void Room::deserialize(const PropertyBag& bag) {
    id            = static_cast<int>(bag.getInt("id", -1));
    name          = bag.getText("name", "Room");
    usage         = bag.getEnum("usage", RoomUsage::LivingRoom);
    function      = bag.getEnum("function", RoomFunction::Residential);
    ceilingHeight = bag.getReal("ceilingHeight", 2.80);
    areaM2        = bag.getReal("areaM2", 0.0);
    boundaryHandle = bag.getText("boundaryHandle");

    boundary.clear();
    const int64_t vc = bag.getInt("vertexCount", 0);
    for (int64_t i = 0; i < vc; ++i)
        boundary.push_back(bag.getPoint("v." + std::to_string(i)));

    openings.clear();
    const int64_t oc = bag.getInt("openingCount", 0);
    for (int64_t i = 0; i < oc; ++i) {
        const std::string k = "op." + std::to_string(i) + ".";
        Opening o;
        o.kind       = static_cast<Opening::Kind>(bag.getInt(k + "kind", 0));
        o.start      = bag.getPoint(k + "start");
        o.end        = bag.getPoint(k + "end");
        o.sillHeight = bag.getReal(k + "sill", 0.0);
        openings.push_back(o);
    }
}

} // namespace electrical
