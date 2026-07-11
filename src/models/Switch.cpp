#include "models/Switch.h"

namespace electrical {

void Switch::serialize(PropertyBag& bag) const {
    serializeBase(bag);
    bag.putText("type", typeTag());
    bag.putEnum("kind", kind);
    bag.putInt("gangs", gangs);
    bag.putInt("cmdCount", static_cast<int64_t>(commands.size()));
    for (size_t i = 0; i < commands.size(); ++i)
        bag.putText("cmd." + std::to_string(i), commands[i]);
    bag.putInt("controlCount", static_cast<int64_t>(controlledLightHandles.size()));
    for (size_t i = 0; i < controlledLightHandles.size(); ++i)
        bag.putText("control." + std::to_string(i), controlledLightHandles[i]);
}

void Switch::deserialize(const PropertyBag& bag) {
    deserializeBase(bag);
    kind  = bag.getEnum("kind", SwitchKind::OneSection);
    gangs = static_cast<int>(bag.getInt("gangs", 1));
    // Per-section commands (absent in drawings written by older builds).
    commands.clear();
    const int64_t cmdCount = bag.getInt("cmdCount", 0);
    for (int64_t i = 0; i < cmdCount; ++i)
        commands.push_back(bag.getText("cmd." + std::to_string(i)));
    syncCommands();
    controlledLightHandles.clear();
    const int64_t count = bag.getInt("controlCount", 0);
    for (int64_t i = 0; i < count; ++i)
        controlledLightHandles.push_back(bag.getText("control." + std::to_string(i)));
}

} // namespace electrical
