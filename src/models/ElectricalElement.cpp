#include "models/ElectricalElement.h"

namespace electrical {

void ElectricalElement::serializeBase(PropertyBag& bag) const {
    bag.putEnum("elemType", type);
    bag.putText("handle", handle);
    bag.putText("tag", tag);
    bag.putPoint("position", position);
    bag.putReal("mountingHeight", mountingHeight);
    bag.putInt("roomId", roomId);
    bag.putInt("circuitId", circuitId);
    bag.putReal("powerVA", powerVA);
}

void ElectricalElement::deserializeBase(const PropertyBag& bag) {
    type           = bag.getEnum("elemType", ElementType::LightPoint);
    handle         = bag.getText("handle");
    tag            = bag.getText("tag");
    position       = bag.getPoint("position");
    mountingHeight = bag.getReal("mountingHeight", 0.0);
    roomId         = static_cast<int>(bag.getInt("roomId", -1));
    circuitId      = static_cast<int>(bag.getInt("circuitId", -1));
    powerVA        = bag.getReal("powerVA", 0.0);
}

} // namespace electrical
