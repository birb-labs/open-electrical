// =============================================================================
//  CircuitDistributor.h - Automatic branch-circuit distribution (NBR 5410).
//
//  Rules applied (residential defaults; configurable later):
//   - Lighting and outlets are never mixed on the same circuit.
//   - Lighting circuits are grouped up to a conventional VA ceiling.
//   - General-purpose outlets (TUG) are grouped up to a VA ceiling / count.
//   - Each specific outlet (TUE, e.g. shower, A/C) gets its own circuit.
//   - Conductor section and breaker follow the standard minimum ladder.
//   - Phases are balanced round-robin across the available live conductors.
// =============================================================================
#pragma once

#include "models/ProjectData.h"

namespace electrical {

struct DistributionOptions {
    double maxLightingVA = 1200.0;   // per lighting circuit
    double maxOutletVA   = 1270.0;   // per TUG circuit (~ conventional)
    int    maxOutletsPerCircuit = 10;
    int    targetPanelId = -1;       // panel that owns the new circuits (-1 = first/main)
};

struct DistributionResult {
    int circuitsCreated = 0;
    int elementsAssigned = 0;
};

class CircuitDistributor {
public:
    // Distributes all currently unassigned elements into circuits, mutating the
    // project (new Circuit records, element.circuitId set). Idempotent for
    // already-assigned elements.
    static DistributionResult distribute(ProjectData& project,
                                         const DistributionOptions& opt = {});

    // Standard NBR 5410 minimum conductor section (mm2) for a circuit type.
    static double minConductorMM2(CircuitType type);

    // A conventional breaker rating (A) for a conductor section, surface/PVC.
    static double breakerFor(double conductorMM2);

    // Fills Circuit::voltageDropPct for every circuit from the routed conduit
    // lengths and the connected load, per a simplified NBR 5410 model (copper,
    // resistive load). Call AFTER routing (EL-CONDUIT-AUTO), when conduit lengths
    // are known - EL-WIRE-AUTO does this. Circuits with no routed conduit keep 0.
    static void computeVoltageDrops(ProjectData& project);
};

} // namespace electrical
