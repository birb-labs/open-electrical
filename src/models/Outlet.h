// =============================================================================
//  Outlet.h - A socket outlet (single/duplex/triplex).
//
//  Each module carries its own mounting height, matching the requirement that
//  the modules of one outlet may sit at different heights (e.g. a low general
//  outlet plus a high one for an air-conditioner over the same box).
// =============================================================================
#pragma once

#include "models/ElectricalElement.h"
#include <array>
#include <vector>

namespace electrical {

struct OutletModule {
    OutletPurpose purpose      = OutletPurpose::General;  // TUG vs TUE
    double        mountingHeight = 0.30;  // meters AFF, per-module
    double        powerVA        = 100.0; // NBR 5410 min. 100 VA for TUG
    std::string   description;            // e.g. "Air conditioner"
};

class Outlet final : public ElectricalElement {
public:
    OutletKind                kind = OutletKind::Single;
    std::vector<OutletModule> modules;  // 1, 2 or 3 entries

    Outlet() : ElectricalElement(ElementType::Outlet) {
        setKind(OutletKind::Single);
    }

    // Resizes the module list to match the kind, preserving existing modules.
    void setKind(OutletKind k);

    // Sum of module VA - kept in the base powerVA for the load schedule.
    double totalVA() const;

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Outlet"; }
};

} // namespace electrical
