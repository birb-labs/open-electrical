#include "models/ProjectData.h"

namespace electrical {

Room* ProjectData::findRoom(int id) {
    for (auto& r : rooms) if (r.id == id) return &r;
    return nullptr;
}

Circuit* ProjectData::findCircuit(int id) {
    for (auto& c : circuits) if (c.id == id) return &c;
    return nullptr;
}

Panel* ProjectData::findPanel(int id) {
    for (auto& e : elements) {
        if (e->type == ElementType::Panel) {
            auto* p = static_cast<Panel*>(e.get());
            if (p->id == id) return p;   // match on the panel's own id
        }
    }
    return nullptr;
}

ElectricalElement* ProjectData::findElementByHandle(const std::string& handle) {
    for (auto& e : elements)
        if (e->handle == handle) return e.get();
    return nullptr;
}

std::unique_ptr<ElectricalElement> ProjectData::makeElement(const std::string& typeTag) {
    if (typeTag == "LightPoint") return std::make_unique<LightPoint>();
    if (typeTag == "Outlet")     return std::make_unique<Outlet>();
    if (typeTag == "Switch")     return std::make_unique<Switch>();
    if (typeTag == "Panel")      return std::make_unique<Panel>();
    return nullptr;
}

void ProjectData::clear() {
    rooms.clear();
    circuits.clear();
    conduits.clear();
    elements.clear();
    nextRoomId = nextCircuitId = nextConduitId = nextPanelId = 1;
}

} // namespace electrical
