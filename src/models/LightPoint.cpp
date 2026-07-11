#include "models/LightPoint.h"

namespace electrical {

void LightPoint::serialize(PropertyBag& bag) const {
    serializeBase(bag);
    bag.putText("type", typeTag());
    bag.putInt("lampCount", lampCount);
    bag.putReal("lumensPerLamp", lumensPerLamp);
    bag.putReal("wattsPerLamp", wattsPerLamp);
    bag.putInt("wallMounted", wallMounted ? 1 : 0);
    bag.putInt("switchLink", controllingSwitchRoomIndex);
    bag.putReal("offsetFromCeiling", offsetFromCeiling);
    bag.putText("powerText", powerText);
}

void LightPoint::deserialize(const PropertyBag& bag) {
    deserializeBase(bag);
    lampCount     = static_cast<int>(bag.getInt("lampCount", 1));
    lumensPerLamp = bag.getReal("lumensPerLamp", 800.0);
    wattsPerLamp  = bag.getReal("wattsPerLamp", 9.0);
    wallMounted   = bag.getInt("wallMounted", 0) != 0;
    controllingSwitchRoomIndex = static_cast<int>(bag.getInt("switchLink", -1));
    offsetFromCeiling = bag.getReal("offsetFromCeiling", 0.0);   // absent in older drawings
    powerText     = bag.getText("powerText");   // absent in older drawings
    // Keep the load figure consistent with the lamp data.
    if (powerVA <= 0.0)
        powerVA = lampCount * wattsPerLamp;
}

} // namespace electrical
