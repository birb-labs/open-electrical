// =============================================================================
//  LightPoint.h - A lighting point (ceiling/wall luminaire).
// =============================================================================
#pragma once

#include "models/ElectricalElement.h"

namespace electrical {

class LightPoint final : public ElectricalElement {
public:
    int    lampCount   = 1;
    double lumensPerLamp = 800.0;   // luminous flux of one lamp
    double wattsPerLamp  = 9.0;     // for the load schedule
    bool   wallMounted   = false;   // false = ceiling
    // Vertical offset of the fixture BELOW the ceiling, in metres (NBR 5444 /
    // usual practice references the luminaire to the ceiling, not the floor):
    // 0 = flush with the ceiling, 0.5 = half a metre of pendant drop. The base
    // class mountingHeight (height above finished floor) is derived from this at
    // insertion time as ceilingHeight - offsetFromCeiling.
    double offsetFromCeiling = 0.0;
    int    controllingSwitchRoomIndex = -1;  // link to the switch that drives it
    // Free-text power notation drawn in the symbol's upper sector, e.g. "2x32W".
    // Empty means "derive from lampCount x wattsPerLamp" at insertion time.
    std::string powerText;

    LightPoint() : ElectricalElement(ElementType::LightPoint) {}

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "LightPoint"; }
};

} // namespace electrical
