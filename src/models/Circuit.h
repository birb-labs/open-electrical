// =============================================================================
//  Circuit.h - A branch circuit originating at a panel.
// =============================================================================
#pragma once

#include "models/Types.h"
#include "models/Serialization.h"
#include <string>
#include <vector>

namespace electrical {

class Circuit final : public ISerializable {
public:
    int          id = -1;
    std::string  name = "C1";       // circuit designation
    CircuitType  type = CircuitType::Lighting;
    int          panelId = -1;      // originating panel

    // Elements fed by this circuit, referenced by block-reference handle.
    std::vector<std::string> elementHandles;

    // Sizing results (filled by CircuitDistributor / conductor sizing).
    double phaseConductorMM2   = 1.5;   // NBR 5410: 1.5 lighting, 2.5 outlets
    double neutralConductorMM2 = 1.5;
    double groundConductorMM2  = 1.5;
    double breakerAmps         = 10.0;  // protective device rating
    double connectedLoadVA     = 0.0;
    double demandFactor        = 1.0;
    int    phaseAssignment     = 0;     // 0=A,1=B,2=C for balancing
    double voltageDropPct      = 0.0;

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Circuit"; }
};

} // namespace electrical
