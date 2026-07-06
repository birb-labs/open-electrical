#include "services/CircuitDistributor.h"
#include "models/Outlet.h"

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

    // Resolve owning panel.
    int panelId = opt.targetPanelId;
    if (panelId < 0) {
        for (const auto& e : project.elements)
            if (e->type == ElementType::Panel) { panelId = e->circuitId >= 0 ? e->circuitId : 0; break; }
        if (panelId < 0) panelId = 0;   // implicit main
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

    return result;
}

} // namespace electrical
