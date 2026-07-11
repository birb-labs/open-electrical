// =============================================================================
//  Switch.h - A wall switch (single-pole / double-pole / three-way / four-way).
// =============================================================================
#pragma once

#include "models/ElectricalElement.h"
#include <string>
#include <vector>

namespace electrical {

class Switch final : public ElectricalElement {
public:
    SwitchKind       kind = SwitchKind::OneSection;
    int              gangs = 1;              // number of keys on the plate
    // One "comando" (lowercase control letter, e.g. "a") per section. Size is
    // kept equal to sectionCount(); the special variants are single-section.
    std::vector<std::string> commands{ "" };
    std::vector<std::string> controlledLightHandles;  // luminaires it drives

    // Number of drawn sections implied by `kind` (1..3).
    int sectionCount() const {
        switch (kind) {
            case SwitchKind::TwoSection:   return 2;
            case SwitchKind::ThreeSection: return 3;
            default:                       return 1;
        }
    }
    // Resizes `commands` to match sectionCount(), preserving existing entries.
    void syncCommands() { commands.resize(static_cast<size_t>(sectionCount())); }

    Switch() : ElectricalElement(ElementType::Switch) {
        mountingHeight = 1.30;   // typical switch height (m AFF)
    }

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Switch"; }
};

} // namespace electrical
