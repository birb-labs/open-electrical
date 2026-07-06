#include "models/Panel.h"

namespace electrical {

void Panel::serialize(PropertyBag& bag) const {
    serializeBase(bag);
    bag.putText("type", typeTag());
    bag.putText("name", name);
    bag.putInt("isMain", isMain ? 1 : 0);
    bag.putInt("parentPanelId", parentPanelId);
    bag.putReal("demandFactor", demandFactor);
    bag.putInt("circuitCount", static_cast<int64_t>(circuitIds.size()));
    for (size_t i = 0; i < circuitIds.size(); ++i)
        bag.putInt("circuit." + std::to_string(i), circuitIds[i]);
}

void Panel::deserialize(const PropertyBag& bag) {
    deserializeBase(bag);
    name          = bag.getText("name", "QD-1");
    isMain        = bag.getInt("isMain", 0) != 0;
    parentPanelId = static_cast<int>(bag.getInt("parentPanelId", -1));
    demandFactor  = bag.getReal("demandFactor", 1.0);
    circuitIds.clear();
    const int64_t count = bag.getInt("circuitCount", 0);
    for (int64_t i = 0; i < count; ++i)
        circuitIds.push_back(static_cast<int>(bag.getInt("circuit." + std::to_string(i), -1)));
}

} // namespace electrical
