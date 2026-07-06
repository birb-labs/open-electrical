#include "models/Circuit.h"

namespace electrical {

void Circuit::serialize(PropertyBag& bag) const {
    bag.putText("type", typeTag());
    bag.putInt("id", id);
    bag.putText("name", name);
    bag.putEnum("circuitType", type);
    bag.putInt("panelId", panelId);
    bag.putReal("phaseMM2", phaseConductorMM2);
    bag.putReal("neutralMM2", neutralConductorMM2);
    bag.putReal("groundMM2", groundConductorMM2);
    bag.putReal("breakerAmps", breakerAmps);
    bag.putReal("connectedLoadVA", connectedLoadVA);
    bag.putReal("demandFactor", demandFactor);
    bag.putInt("phaseAssignment", phaseAssignment);
    bag.putReal("voltageDropPct", voltageDropPct);

    bag.putInt("elementCount", static_cast<int64_t>(elementHandles.size()));
    for (size_t i = 0; i < elementHandles.size(); ++i)
        bag.putText("elem." + std::to_string(i), elementHandles[i]);
}

void Circuit::deserialize(const PropertyBag& bag) {
    id                  = static_cast<int>(bag.getInt("id", -1));
    name                = bag.getText("name", "C1");
    type                = bag.getEnum("circuitType", CircuitType::Lighting);
    panelId             = static_cast<int>(bag.getInt("panelId", -1));
    phaseConductorMM2   = bag.getReal("phaseMM2", 1.5);
    neutralConductorMM2 = bag.getReal("neutralMM2", 1.5);
    groundConductorMM2  = bag.getReal("groundMM2", 1.5);
    breakerAmps         = bag.getReal("breakerAmps", 10.0);
    connectedLoadVA     = bag.getReal("connectedLoadVA", 0.0);
    demandFactor        = bag.getReal("demandFactor", 1.0);
    phaseAssignment     = static_cast<int>(bag.getInt("phaseAssignment", 0));
    voltageDropPct      = bag.getReal("voltageDropPct", 0.0);

    elementHandles.clear();
    const int64_t count = bag.getInt("elementCount", 0);
    for (int64_t i = 0; i < count; ++i)
        elementHandles.push_back(bag.getText("elem." + std::to_string(i)));
}

} // namespace electrical
