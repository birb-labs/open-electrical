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

    // Pulls wires (one phase/neutral/ground set per circuit) through the conduits
    // that carry it, filling Conduit::wires for the BOM and fill checks.
    static void pullWires(ProjectData& project);
};

} // namespace electrical
