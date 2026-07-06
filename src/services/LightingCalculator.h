// =============================================================================
//  LightingCalculator.h - Luminotechnical design via the lumen method.
//
//  Follows the classic "metodo dos lumens" (NBR 5413 / ISO 8995 illuminance
//  targets, NBR 5410 for the electrical side):
//
//      phi_total = (E * A) / (CU * MF)
//      N         = ceil(phi_total / phi_luminaire)
//
//  where E is the target illuminance (lux), A the floor area (m2), CU the
//  coefficient of utilization (from the room index) and MF the maintenance
//  factor. Luminaires are then laid out on a uniform grid inside the room.
// =============================================================================
#pragma once

#include "models/Room.h"
#include "models/Types.h"
#include <vector>

namespace electrical {

struct LightingInput {
    double lumensPerLuminaire = 2000.0;  // luminous flux of one fitting
    double wattsPerLuminaire  = 18.0;    // for the load schedule
    double maintenanceFactor  = 0.80;    // MF (clean/normal environment)
    double ceilingReflectance = 0.70;
    double wallReflectance    = 0.50;
    double floorReflectance   = 0.20;
    double workPlaneHeight    = 0.75;    // m above floor
};

struct LightingResult {
    double requiredLux        = 0.0;
    double roomIndex          = 0.0;   // K
    double utilizationFactor  = 0.0;   // CU
    double totalLumensNeeded  = 0.0;
    int    luminaireCount     = 0;
    std::vector<Point3> positions;     // suggested luminaire positions (dwg units)
    double achievedLux        = 0.0;   // with the rounded-up luminaire count
};

class LightingCalculator {
public:
    // Target illuminance (lux) for a room usage, per NBR 5413 / ISO 8995.
    static double targetLux(RoomUsage usage);

    // Full luminotechnical calculation + grid layout for a room.
    static LightingResult calculate(const Room& room, Unit unit,
                                    const LightingInput& in = {});

private:
    // Coefficient of utilization estimated from the room index K and the
    // ceiling reflectance. A compact monotonic approximation of the
    // manufacturer CU tables (good enough for automatic first placement;
    // refine against a specific luminaire's photometric table if required).
    static double utilizationFactor(double K, double ceilingReflectance);
};

} // namespace electrical
