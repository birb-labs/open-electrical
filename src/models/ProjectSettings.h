// =============================================================================
//  ProjectSettings.h - Global, drawing-wide configuration (EL-CONFIG).
// =============================================================================
#pragma once

#include "models/Types.h"
#include "models/Serialization.h"
#include <string>
#include <vector>

namespace electrical {

// A power utility company. A predefined set ships with the plugin; the user may
// register additional providers (persisted with the project).
struct PowerUtility {
    std::string name;         // e.g. "NeoEnergia", "Enel", "CEMIG", "COPEL"
    double      nominalVoltageLL = 380.0;  // line-to-line V
    double      nominalVoltageLN = 220.0;  // line-to-neutral V
    double      frequency        = 60.0;   // Hz
    bool        userDefined      = false;
};

class ProjectSettings final : public ISerializable {
public:
    // ---- Scale -----------------------------------------------------------
    Unit unit = Unit::Meters;

    // ---- Electrical network ---------------------------------------------
    double          voltageLineToLine   = 380.0;   // V
    double          voltageLineToNeutral = 220.0;  // V
    double          frequency           = 60.0;    // Hz
    Phases          phases              = Phases::SinglePhase;
    GroundingSystem grounding           = GroundingSystem::TN_S;

    // ---- Installation ----------------------------------------------------
    InstallationType installation       = InstallationType::Concealed;
    ConduitMaterial  conduitMaterial    = ConduitMaterial::PVC_Corrugated;
    double           defaultWireGaugeMM2 = 2.5;     // mm^2 (NBR 5410 minimum for TUG)

    // ---- Utility provider ------------------------------------------------
    std::string      utilityProvider    = "NeoEnergia";
    std::vector<PowerUtility> providers;            // includes predefined + user

    // Luminotechnical parameters are NOT global: they live per room (see Room -
    // reflectances, contrast, MF, CU, work-plane and ceiling heights). The lumen
    // method reads them straight off each Room; there is no project-wide default.

    // ---- UI language (commands stay English; UI text is localized) -------
    std::string      uiLanguage         = "en";     // "en" | "pt-BR" | "es"

    ProjectSettings();

    // Returns the built-in provider list (NeoEnergia, Enel, CEMIG, COPEL).
    static std::vector<PowerUtility> predefinedProviders();

    // ISerializable
    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "ProjectSettings"; }
};

} // namespace electrical
