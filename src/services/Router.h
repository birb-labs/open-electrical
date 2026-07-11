// =============================================================================
//  Router.h - Automatic conduit routing and wire pulling.
//
//  The router connects each circuit's elements back toward their panel. It can
//  operate in two modes controlled by `omitHorizontal`:
//    - Full routing: orthogonal ceiling runs plus vertical drops to each device.
//    - Vertical-only: no horizontal conduits are drawn; only the vertical drop
//      from ceiling to each device is created (a common documentation style).
// =============================================================================
#pragma once

#include "models/ProjectData.h"
#include "services/WireCounts.h"

namespace electrical {

struct RoutingOptions {
    bool   omitHorizontal = false;  // only vertical drops when true
    double ceilingZ       = 0.0;    // ceiling height in drawing units (0 = use room)
    ConduitMaterial material = ConduitMaterial::PVC_Corrugated;
    double diameterMM     = 20.0;
};

struct RoutingResult {
    int conduitsCreated = 0;
    double totalLengthM = 0.0;
};

class Router {
public:
    // Routes conduits for every circuit that has none yet.
    static RoutingResult route(ProjectData& project, const RoutingOptions& opt = {});

    // Pulls wires through the conduits, filling Conduit::wires for the BOM and
    // fill checks. Each conduit gets one conductor set per DISTINCT circuit that
    // travels through it (see conduitCircuits), so a shared trunk accumulates.
    static void pullWires(ProjectData& project);

    // Distinct circuit ids travelling through `conduit`: its own circuitIds, or,
    // when empty (older/hand-drawn runs), those of the devices at its endpoints.
    static std::vector<int> conduitCircuits(const ProjectData& project,
                                            const Conduit& conduit);

    // Total F/N/PE/R carried by `conduit` = sum of conductorsFor(type) over every
    // distinct circuit it carries. Drives the wiring-distribution balloon.
    static ConductorSet conduitConductors(const ProjectData& project,
                                          const Conduit& conduit);
};

} // namespace electrical
