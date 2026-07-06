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
    int    controllingSwitchRoomIndex = -1;  // link to the switch that drives it

    LightPoint() : ElectricalElement(ElementType::LightPoint) {}

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "LightPoint"; }
};

} // namespace electrical
