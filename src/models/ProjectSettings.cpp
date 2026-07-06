#include "models/ProjectSettings.h"

namespace electrical {

ProjectSettings::ProjectSettings() {
    providers = predefinedProviders();
}

std::vector<PowerUtility> ProjectSettings::predefinedProviders() {
    // Predefined Brazilian distributors. Voltages reflect common secondary
    // supply arrangements; the user can override per project.
    return {
        { "NeoEnergia", 380.0, 220.0, 60.0, false },
        { "Enel",       380.0, 220.0, 60.0, false },
        { "CEMIG",      380.0, 220.0, 60.0, false },
        { "COPEL",      380.0, 220.0, 60.0, false },
    };
}

void ProjectSettings::serialize(PropertyBag& bag) const {
    bag.putText("type", typeTag());
    bag.putEnum("unit", unit);

    bag.putReal("voltageLL", voltageLineToLine);
    bag.putReal("voltageLN", voltageLineToNeutral);
    bag.putReal("frequency", frequency);
    bag.putEnum("phases", phases);
    bag.putEnum("grounding", grounding);

    bag.putEnum("installation", installation);
    bag.putEnum("conduitMaterial", conduitMaterial);
    bag.putReal("defaultWireGauge", defaultWireGaugeMM2);

    bag.putText("utilityProvider", utilityProvider);
    bag.putText("uiLanguage", uiLanguage);

    // Providers are flattened into indexed keys.
    bag.putInt("providerCount", static_cast<int64_t>(providers.size()));
    for (size_t i = 0; i < providers.size(); ++i) {
        const auto& p = providers[i];
        const std::string k = "provider." + std::to_string(i) + ".";
        bag.putText(k + "name", p.name);
        bag.putReal(k + "vll", p.nominalVoltageLL);
        bag.putReal(k + "vln", p.nominalVoltageLN);
        bag.putReal(k + "freq", p.frequency);
        bag.putInt (k + "user", p.userDefined ? 1 : 0);
    }
}

void ProjectSettings::deserialize(const PropertyBag& bag) {
    unit = bag.getEnum("unit", Unit::Meters);

    voltageLineToLine    = bag.getReal("voltageLL", 380.0);
    voltageLineToNeutral = bag.getReal("voltageLN", 220.0);
    frequency            = bag.getReal("frequency", 60.0);
    phases               = bag.getEnum("phases", Phases::SinglePhase);
    grounding            = bag.getEnum("grounding", GroundingSystem::TN_S);

    installation         = bag.getEnum("installation", InstallationType::Concealed);
    conduitMaterial      = bag.getEnum("conduitMaterial", ConduitMaterial::PVC_Corrugated);
    defaultWireGaugeMM2  = bag.getReal("defaultWireGauge", 2.5);

    utilityProvider      = bag.getText("utilityProvider", "NeoEnergia");
    uiLanguage           = bag.getText("uiLanguage", "en");

    providers.clear();
    const int64_t count = bag.getInt("providerCount", 0);
    for (int64_t i = 0; i < count; ++i) {
        const std::string k = "provider." + std::to_string(i) + ".";
        PowerUtility p;
        p.name             = bag.getText(k + "name");
        p.nominalVoltageLL = bag.getReal(k + "vll", 380.0);
        p.nominalVoltageLN = bag.getReal(k + "vln", 220.0);
        p.frequency        = bag.getReal(k + "freq", 60.0);
        p.userDefined      = bag.getInt (k + "user", 0) != 0;
        providers.push_back(std::move(p));
    }
    if (providers.empty())
        providers = predefinedProviders();
}

} // namespace electrical
