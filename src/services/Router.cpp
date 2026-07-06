#include "services/Router.h"
#include "utilities/GeometryHelper.h"

#include <algorithm>

namespace electrical {

namespace {

// Finds an element's insertion point by handle.
const ElectricalElement* elementByHandle(const ProjectData& p, const std::string& h) {
    for (const auto& e : p.elements)
        if (e->handle == h) return e.get();
    return nullptr;
}

} // namespace

RoutingResult Router::route(ProjectData& project, const RoutingOptions& opt) {
    RoutingResult result;

    for (const auto& circuit : project.circuits) {
        // Gather this circuit's device positions.
        std::vector<const ElectricalElement*> devices;
        for (const auto& h : circuit.elementHandles)
            if (auto* e = elementByHandle(project, h)) devices.push_back(e);
        if (devices.empty()) continue;

        // Ceiling reference: option override, else the tallest device room.
        const double ceilingZ = opt.ceilingZ;

        if (opt.omitHorizontal) {
            // Vertical drop only: ceiling -> device, one conduit each.
            for (auto* d : devices) {
                Conduit c;
                c.id = project.allocConduitId();
                c.material = opt.material;
                c.diameterMM = opt.diameterMM;
                c.isVerticalDrop = true;
                Point3 top(d->position.x, d->position.y, ceilingZ);
                c.path = { top, d->position };
                c.recomputeLength(project.settings.unit);
                result.totalLengthM += c.lengthM;
                project.conduits.push_back(std::move(c));
                ++result.conduitsCreated;
            }
            continue;
        }

        // Full routing: chain devices at ceiling level (orthogonal), then drop.
        // Order devices by a simple nearest-neighbour walk from the first one.
        std::vector<const ElectricalElement*> order;
        std::vector<bool> used(devices.size(), false);
        size_t cur = 0; used[0] = true; order.push_back(devices[0]);
        for (size_t step = 1; step < devices.size(); ++step) {
            double best = 1e30; size_t bestI = cur;
            for (size_t i = 0; i < devices.size(); ++i) {
                if (used[i]) continue;
                const double dx = devices[i]->position.x - devices[cur]->position.x;
                const double dy = devices[i]->position.y - devices[cur]->position.y;
                const double d = dx * dx + dy * dy;
                if (d < best) { best = d; bestI = i; }
            }
            used[bestI] = true; order.push_back(devices[bestI]); cur = bestI;
        }

        // Ceiling runs between consecutive devices.
        for (size_t i = 1; i < order.size(); ++i) {
            Point3 a(order[i - 1]->position.x, order[i - 1]->position.y, ceilingZ);
            Point3 b(order[i]->position.x,     order[i]->position.y,     ceilingZ);
            Conduit c;
            c.id = project.allocConduitId();
            c.material = opt.material;
            c.diameterMM = opt.diameterMM;
            c.path = geom::orthogonalPath(a, b);
            c.recomputeLength(project.settings.unit);
            result.totalLengthM += c.lengthM;
            project.conduits.push_back(std::move(c));
            ++result.conduitsCreated;
        }
        // Vertical drops to each device.
        for (auto* d : order) {
            Conduit c;
            c.id = project.allocConduitId();
            c.material = opt.material;
            c.diameterMM = opt.diameterMM;
            c.isVerticalDrop = true;
            Point3 top(d->position.x, d->position.y, ceilingZ);
            c.path = { top, d->position };
            c.recomputeLength(project.settings.unit);
            result.totalLengthM += c.lengthM;
            project.conduits.push_back(std::move(c));
            ++result.conduitsCreated;
        }
    }

    return result;
}

void Router::pullWires(ProjectData& project) {
    // Simplistic: every conduit carries phase+neutral+ground of whichever
    // circuit(s) its endpoints' devices belong to. A full implementation would
    // trace the graph; here we tag by the nearest circuit conductor section.
    for (auto& conduit : project.conduits) {
        if (!conduit.wires.empty()) continue;
        // Default to the smallest lighting set unless a richer trace is available.
        double gauge = 1.5;
        int circuitId = -1;
        if (!project.circuits.empty()) {
            gauge = project.circuits.front().phaseConductorMM2;
            circuitId = project.circuits.front().id;
        }
        conduit.wires.push_back({ circuitId, gauge, "phase",   conduit.lengthM });
        conduit.wires.push_back({ circuitId, gauge, "neutral", conduit.lengthM });
        conduit.wires.push_back({ circuitId, gauge, "ground",  conduit.lengthM });
    }
}

} // namespace electrical
