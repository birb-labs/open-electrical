#include "models/Switch.h"

namespace electrical {

void Switch::serialize(PropertyBag& bag) const {
    serializeBase(bag);
    bag.putText("type", typeTag());
    bag.putEnum("kind", kind);
    bag.putInt("gangs", gangs);
    bag.putInt("controlCount", static_cast<int64_t>(controlledLightHandles.size()));
    for (size_t i = 0; i < controlledLightHandles.size(); ++i)
        bag.putText("control." + std::to_string(i), controlledLightHandles[i]);
}

void Switch::deserialize(const PropertyBag& bag) {
    deserializeBase(bag);
    kind  = bag.getEnum("kind", SwitchKind::SinglePole);
    gangs = static_cast<int>(bag.getInt("gangs", 1));
    controlledLightHandles.clear();
    const int64_t count = bag.getInt("controlCount", 0);
    for (int64_t i = 0; i < count; ++i)
        controlledLightHandles.push_back(bag.getText("control." + std::to_string(i)));
}

} // namespace electrical
