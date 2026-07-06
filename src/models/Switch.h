// =============================================================================
//  Switch.h - A wall switch (single-pole / double-pole / three-way / four-way).
// =============================================================================
#pragma once

#include "models/ElectricalElement.h"
#include <vector>

namespace electrical {

class Switch final : public ElectricalElement {
public:
    SwitchKind       kind = SwitchKind::SinglePole;
    int              gangs = 1;              // number of keys on the plate
    std::vector<std::string> controlledLightHandles;  // luminaires it drives

    Switch() : ElectricalElement(ElementType::Switch) {
        mountingHeight = 1.30;   // typical switch height (m AFF)
    }

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Switch"; }
};

} // namespace electrical
