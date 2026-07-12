#include "services/CircuitDistributor.h"
#include "models/Outlet.h"
#include "models/Panel.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace electrical {

double CircuitDistributor::minConductorMM2(CircuitType type) {
    switch (type) {
        case CircuitType::Lighting:        return 1.5;  // NBR 5410 minimum
        case CircuitType::GeneralOutlets:  return 2.5;
        case CircuitType::SpecificOutlets: return 2.5;
        case CircuitType::Motor:           return 2.5;
        case CircuitType::Mixed:           return 2.5;
    }
    return 2.5;
}

double CircuitDistributor::breakerFor(double conductorMM2) {
    // Conventional ampacity ladder for PVC-insulated copper, method B1.
    if (conductorMM2 <= 1.5) return 10.0;
    if (conductorMM2 <= 2.5) return 16.0;
    if (conductorMM2 <= 4.0) return 25.0;
    if (conductorMM2 <= 6.0) return 32.0;
    if (conductorMM2 <= 10.0) return 40.0;
    if (conductorMM2 <= 16.0) return 50.0;
    return 63.0;
}

namespace {

Circuit makeCircuit(ProjectData& p, CircuitType type, int panelId, int& phaseCursor,
                    int liveConductors) {
    Circuit c;
    c.id   = p.allocCircuitId();
    c.type = type;
    c.panelId = panelId;
    c.phaseConductorMM2   = CircuitDistributor::minConductorMM2(type);
    c.neutralConductorMM2 = c.phaseConductorMM2;
    c.groundConductorMM2  = c.phaseConductorMM2;
    c.breakerAmps         = CircuitDistributor::breakerFor(c.phaseConductorMM2);
    c.phaseAssignment     = (liveConductors > 0) ? (phaseCursor % liveConductors) : 0;
    ++phaseCursor;
    const char* prefix = (type == CircuitType::Lighting) ? "L" : "O";
    c.name = std::string(prefix) + std::to_string(c.id);
    return c;
}

} // namespace

DistributionResult CircuitDistributor::distribute(ProjectData& project,
                                                  const DistributionOptions& opt) {
    DistributionResult result;

    // Resolve owning panel: the option override, else the main board (isMain),
    // else the first panel, else the implicit main (0). We reference the panel by
    // its OWN id (Panel::id), which is what Circuit::panelId points at - the old
    // code read the panel element's circuitId (always -1 for a panel), so every
    // circuit ended up on a phantom panel 0 and no panel's circuit list filled.
    int panelId = opt.targetPanelId;
    if (panelId < 0) {
        const Panel* first = nullptr;
        for (const auto& e : project.elements) {
            if (e->type != ElementType::Panel) continue;
            const auto* p = static_cast<const Panel*>(e.get());
            if (!first) first = p;
            if (p->isMain) { first = p; break; }
        }
        panelId = (first && first->id >= 0) ? first->id : 0;
    }

    const int liveConductors =
        (project.settings.phases == Phases::ThreePhase) ? 3 :
        (project.settings.phases == Phases::TwoPhase)   ? 2 : 1;
    int phaseCursor = 0;

    // --- Lighting -------------------------------------------------------
    Circuit* light = nullptr;
    for (auto& e : project.elements) {
        if (e->type != ElementType::LightPoint || e->circuitId >= 0) continue;
        const double va = e->powerVA > 0 ? e->powerVA : 100.0;
        if (!light || light->connectedLoadVA + va > opt.maxLightingVA) {
            project.circuits.push_back(
                makeCircuit(project, CircuitType::Lighting, panelId, phaseCursor, liveConductors));
            light = &project.circuits.back();
            ++result.circuitsCreated;
        }
        light->elementHandles.push_back(e->handle);
        light->connectedLoadVA += va;
        e->circuitId = light->id;
        ++result.elementsAssigned;
    }

    // --- Outlets --------------------------------------------------------
    Circuit* tug = nullptr;
    int tugCount = 0;
    for (auto& e : project.elements) {
        if (e->type != ElementType::Outlet || e->circuitId >= 0) continue;
        auto* o = static_cast<Outlet*>(e.get());
        const double va = o->totalVA();

        // Specific-purpose outlets (any module marked Specific) get a dedicated
        // circuit (TUE).
        bool specific = false;
        for (const auto& m : o->modules)
            if (m.purpose == OutletPurpose::Specific) { specific = true; break; }

        if (specific) {
            project.circuits.push_back(
                makeCircuit(project, CircuitType::SpecificOutlets, panelId, phaseCursor, liveConductors));
            Circuit& c = project.circuits.back();
            c.elementHandles.push_back(e->handle);
            c.connectedLoadVA = va;
            e->circuitId = c.id;
            ++result.circuitsCreated;
            ++result.elementsAssigned;
            continue;
        }

        if (!tug || tug->connectedLoadVA + va > opt.maxOutletVA ||
            tugCount >= opt.maxOutletsPerCircuit) {
            project.circuits.push_back(
                makeCircuit(project, CircuitType::GeneralOutlets, panelId, phaseCursor, liveConductors));
            tug = &project.circuits.back();
            tugCount = 0;
            ++result.circuitsCreated;
        }
        tug->elementHandles.push_back(e->handle);
        tug->connectedLoadVA += va;
        e->circuitId = tug->id;
        ++tugCount;
        ++result.elementsAssigned;
    }

    // Rebuild every panel's circuit list from the circuits' panelId, so the
    // panel's CIRCUITOS block attribute and the single-line diagram stay in sync.
    // Circuits whose panelId matches no panel are attributed to the main/first
    // panel (covers legacy data and the implicit-main id 0). Idempotent.
    Panel* mainPanel = nullptr;
    for (auto& e : project.elements) {
        if (e->type != ElementType::Panel) continue;
        auto* p = static_cast<Panel*>(e.get());
        p->circuitIds.clear();
        if (!mainPanel || p->isMain) mainPanel = p;
    }
    for (auto& e : project.elements) {
        if (e->type != ElementType::Panel) continue;
        auto* p = static_cast<Panel*>(e.get());
        for (const auto& c : project.circuits) {
            const bool owned = (c.panelId == p->id);
            const bool orphan = (p == mainPanel) &&
                std::none_of(project.elements.begin(), project.elements.end(),
                    [&](const std::unique_ptr<ElectricalElement>& x) {
                        return x->type == ElementType::Panel &&
                               static_cast<const Panel*>(x.get())->id == c.panelId;
                    });
            if (owned || orphan) p->circuitIds.push_back(c.id);
        }
    }

    return result;
}

void CircuitDistributor::computeVoltageDrops(ProjectData& project) {
    // Simplified NBR 5410 voltage-drop estimate per circuit:
    //   dV = k * rho * L * I / S      dV% = 100 * dV / Vref
    // rho = copper resistivity (~0.0172 ohm.mm2/m at 20 C); L = one-way conductor
    // length (m) = the summed length of the conduits carrying this circuit; I =
    // connected load current; S = phase conductor section (mm2). k accounts for the
    // return path: 2 for single/two-phase (phase+return), sqrt(3) for three-phase.
    // Deliberately ignores power factor, temperature and bunching derating - it is
    // a first-pass indicator, refine against the full NBR 5410 tables if required.
    constexpr double rho = 0.0172;   // ohm.mm2/m, copper
    const auto& s = project.settings;
    const bool threePhase = (s.phases == Phases::ThreePhase);
    const double vRef = threePhase
        ? (s.voltageLineToLine > 0 ? s.voltageLineToLine : 380.0)
        : (s.voltageLineToNeutral > 0 ? s.voltageLineToNeutral : 220.0);
    const double k = threePhase ? std::sqrt(3.0) : 2.0;

    for (auto& c : project.circuits) {
        double lengthM = 0.0;
        for (const auto& cond : project.conduits)
            if (std::find(cond.circuitIds.begin(), cond.circuitIds.end(), c.id)
                    != cond.circuitIds.end())
                lengthM += cond.lengthM;

        const double section = c.phaseConductorMM2 > 0 ? c.phaseConductorMM2 : 1.5;
        const double current = (vRef > 0) ? (c.connectedLoadVA / vRef) : 0.0;
        const double dV = k * rho * lengthM * current / section;
        c.voltageDropPct = (vRef > 0) ? (100.0 * dV / vRef) : 0.0;
    }
}

} // namespace electrical
