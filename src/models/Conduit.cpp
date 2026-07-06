#include "models/Conduit.h"
#include <cmath>

namespace electrical {

void Conduit::recomputeLength(Unit unit) {
    double len = 0.0;
    for (size_t i = 1; i < path.size(); ++i) {
        const Point3& a = path[i - 1];
        const Point3& b = path[i];
        len += std::sqrt((b.x - a.x) * (b.x - a.x) +
                         (b.y - a.y) * (b.y - a.y) +
                         (b.z - a.z) * (b.z - a.z));
    }
    lengthM = len * toMeters(unit);
}

void Conduit::serialize(PropertyBag& bag) const {
    bag.putText("type", typeTag());
    bag.putInt("id", id);
    bag.putText("handle", handle);
    bag.putEnum("material", material);
    bag.putReal("diameterMM", diameterMM);
    bag.putInt("isVerticalDrop", isVerticalDrop ? 1 : 0);
    bag.putReal("lengthM", lengthM);

    bag.putInt("pathCount", static_cast<int64_t>(path.size()));
    for (size_t i = 0; i < path.size(); ++i)
        bag.putPoint("p." + std::to_string(i), path[i]);

    bag.putInt("wireCount", static_cast<int64_t>(wires.size()));
    for (size_t i = 0; i < wires.size(); ++i) {
        const auto& w = wires[i];
        const std::string k = "w." + std::to_string(i) + ".";
        bag.putInt(k + "circuit", w.circuitId);
        bag.putReal(k + "gauge", w.gaugeMM2);
        bag.putText(k + "color", w.color);
        bag.putReal(k + "length", w.lengthM);
    }
}

void Conduit::deserialize(const PropertyBag& bag) {
    id             = static_cast<int>(bag.getInt("id", -1));
    handle         = bag.getText("handle");
    material       = bag.getEnum("material", ConduitMaterial::PVC_Corrugated);
    diameterMM     = bag.getReal("diameterMM", 20.0);
    isVerticalDrop = bag.getInt("isVerticalDrop", 0) != 0;
    lengthM        = bag.getReal("lengthM", 0.0);

    path.clear();
    const int64_t pc = bag.getInt("pathCount", 0);
    for (int64_t i = 0; i < pc; ++i)
        path.push_back(bag.getPoint("p." + std::to_string(i)));

    wires.clear();
    const int64_t wc = bag.getInt("wireCount", 0);
    for (int64_t i = 0; i < wc; ++i) {
        const std::string k = "w." + std::to_string(i) + ".";
        Wire w;
        w.circuitId = static_cast<int>(bag.getInt(k + "circuit", -1));
        w.gaugeMM2  = bag.getReal(k + "gauge", 1.5);
        w.color     = bag.getText(k + "color");
        w.lengthM   = bag.getReal(k + "length", 0.0);
        wires.push_back(std::move(w));
    }
}

} // namespace electrical
