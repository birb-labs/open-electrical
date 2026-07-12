#include "services/Router.h"
#include "models/Circuit.h"
#include "models/Switch.h"
#include "services/CircuitDistributor.h"
#include "services/WireCounts.h"
#include "utilities/GeometryHelper.h"
#include "utilities/PolyMath.h"

#include <algorithm>
#include <vector>

namespace electrical {

namespace {

// Finds an element's insertion point by handle.
const ElectricalElement* elementByHandle(const ProjectData& p, const std::string& h) {
    for (const auto& e : p.elements)
        if (e->handle == h) return e.get();
    return nullptr;
}

// Radius of the bounding circle (drawing units) that fully contains a symbol's
// footprint, centred on the element's insertion point. Values track the symbol
// geometry in SymbolFactory (all derived from kSymUnit = 0.09): a run is trimmed
// to leave this circle so a conduit never crosses the element it connects to. The
// circle is deliberately conservative (covers the whole symbol body) so trimming
// works without needing the block's rotation. 0 for the panel-less/unknown case
// leaves a run un-trimmed at that end.
double elementRadius(const ElectricalElement* e) {
    if (!e) return 0.0;
    switch (e->type) {
        case ElementType::Panel:      return 0.33;   // 3:1 box, ~hypot(0.27,0.18)
        case ElementType::Switch:     return 0.25;   // wall tie + circle to y~0.225
        case ElementType::Outlet:     return 0.22;   // stem + triangle apex ~0.20
        case ElementType::LightPoint: return 0.16;   // ceiling circle r=0.144
        default:                      return 0.16;
    }
}

} // namespace

RoutingResult Router::route(ProjectData& project, const RoutingOptions& opt) {
    RoutingResult result;
    // Metres-per-drawing-unit, to convert device mounting heights (metres) into
    // the drawing's own units for the vertical-drop bottom Z.
    const double unitM = toMeters(project.settings.unit);
    const double invUnit = unitM > 0.0 ? 1.0 / unitM : 1.0;

    // Ceiling reference (drawing units): option override, else the tallest room,
    // else a 2.8 m default. Without a positive value the vertical drops would be
    // zero-length and nothing visible would be drawn.
    double ceilingZ = opt.ceilingZ;
    if (ceilingZ <= 0.0) {
        for (const auto& r : project.rooms)
            ceilingZ = std::max(ceilingZ, r.ceilingHeight * invUnit);
        if (ceilingZ <= 0.0) ceilingZ = 2.8 * invUnit;
    }

    // The panel is the origin of every circuit run ("do quadro ao primeiro ponto,
    // depois ao segundo..."). Use the first panel element in the project, if any.
    const ElectricalElement* panel = nullptr;
    for (const auto& e : project.elements)
        if (e->type == ElementType::Panel) { panel = e.get(); break; }

    // Circuit currently being routed; stamped onto every conduit created below so
    // the wiring balloon knows which circuit(s) travel through each run.
    int curCircuitId = -1;

    // Circuits that actually got routed - the single shared panel drop (added once,
    // after the loop) is tagged with all of them, so the trunk at the panel carries
    // the sum of their conductors without stacking duplicate coincident drops.
    std::vector<int> routedCircuits;

    // Emits one vertical drop conduit (ceiling -> device at its mounting height).
    auto addDrop = [&](const ElectricalElement* d) {
        Conduit c;
        c.id = project.allocConduitId();
        c.material = opt.material;
        c.diameterMM = opt.diameterMM;
        if (curCircuitId >= 0) c.circuitIds.push_back(curCircuitId);
        c.isVerticalDrop = true;
        c.path = { Point3(d->position.x, d->position.y, ceilingZ),
                   Point3(d->position.x, d->position.y, d->mountingHeight * invUnit) };
        c.recomputeLength(project.settings.unit);
        result.totalLengthM += c.lengthM;
        project.conduits.push_back(std::move(c));
        ++result.conduitsCreated;
    };

    // Orders `items` by a nearest-neighbour walk starting from the item closest
    // to `start`, then chaining to the closest unused item each step.
    auto orderByProximity = [](std::vector<const ElectricalElement*> items,
                               const Point3& start) {
        auto dist2 = [](const Point3& a, const Point3& b) {
            const double dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy;
        };
        std::vector<const ElectricalElement*> order;
        std::vector<bool> used(items.size(), false);
        Point3 ref = start;
        for (size_t k = 0; k < items.size(); ++k) {
            double best = 1e30; size_t bi = 0; bool found = false;
            for (size_t i = 0; i < items.size(); ++i) {
                if (used[i]) continue;
                const double d = dist2(items[i]->position, ref);
                if (d < best) { best = d; bi = i; found = true; }
            }
            if (!found) break;
            used[bi] = true; order.push_back(items[bi]); ref = items[bi]->position;
        }
        return order;
    };

    // Emits one orthogonal ceiling run (at ceilingZ) between two device XY points,
    // trimmed so it starts at the source element's footprint boundary and stops at
    // the destination element's footprint boundary (rA/rB = their bounding radii),
    // never piercing either symbol (NBR routing convention).
    auto addCeilingRun = [&](const Point3& aXY, double rA, const Point3& bXY, double rB) {
        Point3 a(aXY.x, aXY.y, ceilingZ), b(bXY.x, bXY.y, ceilingZ);
        Conduit c;
        c.id = project.allocConduitId();
        c.material = opt.material;
        c.diameterMM = opt.diameterMM;
        if (curCircuitId >= 0) c.circuitIds.push_back(curCircuitId);
        c.path = polymath::trimPathEnds(geom::orthogonalPath(a, b), rA, rB);
        c.recomputeLength(project.settings.unit);
        result.totalLengthM += c.lengthM;
        project.conduits.push_back(std::move(c));
        ++result.conduitsCreated;
    };

    for (const auto& circuit : project.circuits) {
        curCircuitId = circuit.id;   // tag every run created for this circuit
        // Classify this circuit's devices by type (NBR 5410 topology: lighting
        // runs thread the luminaires, switches branch off the light they control,
        // and outlet circuits chain the sockets in series).
        std::vector<const ElectricalElement*> lights, switches, outlets, others;
        for (const auto& h : circuit.elementHandles) {
            auto* e = elementByHandle(project, h);
            if (!e) continue;
            switch (e->type) {
                case ElementType::LightPoint: lights.push_back(e);   break;
                case ElementType::Switch:     switches.push_back(e); break;
                case ElementType::Outlet:     outlets.push_back(e);  break;
                default:                      others.push_back(e);   break;
            }
        }
        if (lights.empty() && switches.empty() && outlets.empty() && others.empty())
            continue;
        routedCircuits.push_back(circuit.id);

        // The run leaves the panel (or, lacking one, the first available device).
        const ElectricalElement* originElem = panel;
        if (!panel) {
            if      (!lights.empty())   originElem = lights.front();
            else if (!outlets.empty())  originElem = outlets.front();
            else if (!switches.empty()) originElem = switches.front();
            else if (!others.empty())   originElem = others.front();
        }
        const Point3 origin = originElem ? originElem->position : Point3();
        const double originR = elementRadius(originElem);

        if (opt.omitHorizontal) {
            // Vertical drops only: ceiling -> device, one conduit each.
            for (auto* d : lights)   addDrop(d);
            for (auto* d : switches) addDrop(d);
            for (auto* d : outlets)  addDrop(d);
            for (auto* d : others)   addDrop(d);
            continue;   // the shared panel drop is added once, after the loop
        }

        // ---- Lighting: panel -> light1 -> light2 -> ... (nearest walk) --------
        const std::vector<const ElectricalElement*> orderedLights =
            orderByProximity(lights, origin);
        Point3 prev = origin;
        double prevR = originR;
        for (auto* lp : orderedLights) {
            addCeilingRun(prev, prevR, lp->position, elementRadius(lp));
            prev = lp->position; prevR = elementRadius(lp);
        }

        // Each switch is a branch off the luminaire it commands (its
        // controlledLightHandles), falling back to the nearest luminaire, then to
        // the origin when the circuit carries no luminaire at all.
        for (auto* sw : switches) {
            const ElectricalElement* host = nullptr;
            const auto* s = static_cast<const Switch*>(sw);
            for (const auto& lh : s->controlledLightHandles)
                if (auto* le = elementByHandle(project, lh))
                    if (le->type == ElementType::LightPoint) { host = le; break; }
            if (!host && !orderedLights.empty()) {
                double best = 1e30;
                for (auto* lp : orderedLights) {
                    const double dx = lp->position.x - sw->position.x;
                    const double dy = lp->position.y - sw->position.y;
                    const double d = dx * dx + dy * dy;
                    if (d < best) { best = d; host = lp; }
                }
            }
            addCeilingRun(host ? host->position : origin,
                          host ? elementRadius(host) : originR,
                          sw->position, elementRadius(sw));
        }

        // ---- Outlets: panel -> outlet1 -> outlet2 -> ... (series) -------------
        const std::vector<const ElectricalElement*> orderedOutlets =
            orderByProximity(outlets, origin);
        Point3 op = origin;
        double opR = originR;
        for (auto* o : orderedOutlets) {
            addCeilingRun(op, opR, o->position, elementRadius(o));
            op = o->position; opR = elementRadius(o);
        }

        // Anything else: a direct run from the origin.
        for (auto* d : others) addCeilingRun(origin, originR, d->position, elementRadius(d));

        // ---- Vertical drops (ceiling -> each device at its mounting height) ----
        // The panel drop is NOT added here (it would stack one coincident drop per
        // circuit, inflating the BOM); it is emitted once below.
        for (auto* d : orderedLights)  addDrop(d);
        for (auto* d : switches)       addDrop(d);
        for (auto* d : orderedOutlets) addDrop(d);
        for (auto* d : others)         addDrop(d);
    }

    // One shared vertical drop at the panel (ceiling -> panel), tagged with every
    // circuit routed, instead of a duplicate per circuit.
    if (panel && !routedCircuits.empty()) {
        Conduit c;
        c.id = project.allocConduitId();
        c.material = opt.material;
        c.diameterMM = opt.diameterMM;
        c.circuitIds = routedCircuits;
        c.isVerticalDrop = true;
        c.path = { Point3(panel->position.x, panel->position.y, ceilingZ),
                   Point3(panel->position.x, panel->position.y,
                          panel->mountingHeight * invUnit) };
        c.recomputeLength(project.settings.unit);
        result.totalLengthM += c.lengthM;
        project.conduits.push_back(std::move(c));
        ++result.conduitsCreated;
    }

    return result;
}

namespace {
const Circuit* circuitById(const ProjectData& project, int id) {
    for (const auto& c : project.circuits)
        if (c.id == id) return &c;
    return nullptr;
}
} // namespace

std::vector<int> Router::conduitCircuits(const ProjectData& project, const Conduit& conduit) {
    std::vector<int> ids = conduit.circuitIds;   // set at routing time (preferred)
    if (ids.empty()) {
        // Hand-drawn / legacy run: infer from the devices it connects.
        for (const std::string& h : { conduit.srcHandle, conduit.dstHandle }) {
            const auto* e = elementByHandle(project, h);
            if (e && e->circuitId >= 0 &&
                std::find(ids.begin(), ids.end(), e->circuitId) == ids.end())
                ids.push_back(e->circuitId);
        }
    }
    return ids;
}

ConductorSet Router::conduitConductors(const ProjectData& project, const Conduit& conduit) {
    ConductorSet total;
    for (int cid : conduitCircuits(project, conduit)) {
        const Circuit* c = circuitById(project, cid);
        // Unknown id (e.g. deleted circuit): assume a general-outlet set so the
        // run still shows conductors instead of a blank balloon.
        total += conductorsFor(c ? c->type : CircuitType::GeneralOutlets);
    }
    return total;
}

void Router::pullWires(ProjectData& project) {
    // Each conduit carries one conductor set per DISTINCT circuit travelling
    // through it (the number of DEVICES on a circuit never multiplies conductors;
    // the number of circuits sharing the run does). Conductor gauges come from the
    // owning circuit's sizing; the balloon reads the aggregate via conduitConductors.
    for (auto& conduit : project.conduits) {
        if (!conduit.wires.empty()) continue;
        std::vector<int> ids = conduitCircuits(project, conduit);
        if (ids.empty() && !project.circuits.empty())
            ids.push_back(project.circuits.front().id);   // last resort: main circuit

        for (int cid : ids) {
            const Circuit* c = circuitById(project, cid);
            const double phaseG = c ? c->phaseConductorMM2   : 1.5;
            const double neuG   = c ? c->neutralConductorMM2 : phaseG;
            const double gndG   = c ? c->groundConductorMM2  : phaseG;
            const ConductorSet cs = conductorsFor(c ? c->type : CircuitType::GeneralOutlets);
            for (int i = 0; i < cs.phase;   ++i) conduit.wires.push_back({ cid, phaseG, "phase",   conduit.lengthM });
            for (int i = 0; i < cs.neutral; ++i) conduit.wires.push_back({ cid, neuG,   "neutral", conduit.lengthM });
            for (int i = 0; i < cs.ground;  ++i) conduit.wires.push_back({ cid, gndG,   "ground",  conduit.lengthM });
            for (int i = 0; i < cs.retorno; ++i) conduit.wires.push_back({ cid, phaseG, "return",  conduit.lengthM });
        }
    }

    // Conduit lengths are known now, so fill in each circuit's voltage drop
    // (simplified NBR 5410) for the load schedule / single-line diagram.
    CircuitDistributor::computeVoltageDrops(project);
}

} // namespace electrical
