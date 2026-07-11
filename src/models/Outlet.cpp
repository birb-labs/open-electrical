#include "models/Outlet.h"

namespace electrical {

void Outlet::setKind(OutletKind k) {
    kind = k;
    // The combined outlet+switch symbols are a single medium-height outlet
    // point (see SymbolFactory::buildOutletWithSwitch); "duplex" for the
    // 1-section-switch variant is conveyed by the module purpose/tag, not by
    // an extra drawn triangle (the NBR 5444 geometry for both combo rows is
    // described identically: "identica a tomada media").
    const size_t want = (k == OutletKind::Triplex) ? 3
                      : (k == OutletKind::Duplex)  ? 2 : 1;
    if (modules.size() < want) {
        while (modules.size() < want) modules.emplace_back();
    } else if (modules.size() > want) {
        modules.resize(want);
    }
    if (k == OutletKind::WithSwitch2 || k == OutletKind::DuplexWithSwitch1)
        for (auto& m : modules) m.mountingHeight = 1.30;  // mandatory medium height
}

double Outlet::totalVA() const {
    double sum = 0.0;
    for (const auto& m : modules) sum += m.powerVA;
    return sum;
}

void Outlet::serialize(PropertyBag& bag) const {
    serializeBase(bag);
    bag.putText("type", typeTag());
    bag.putEnum("kind", kind);
    bag.putInt("moduleCount", static_cast<int64_t>(modules.size()));
    for (size_t i = 0; i < modules.size(); ++i) {
        const auto& m = modules[i];
        const std::string p = "mod." + std::to_string(i) + ".";
        bag.putEnum(p + "purpose", m.purpose);
        bag.putReal(p + "height", m.mountingHeight);
        bag.putReal(p + "va", m.powerVA);
        bag.putText(p + "desc", m.description);
    }
}

void Outlet::deserialize(const PropertyBag& bag) {
    deserializeBase(bag);
    kind = bag.getEnum("kind", OutletKind::Single);
    const int64_t count = bag.getInt("moduleCount", 1);
    modules.clear();
    for (int64_t i = 0; i < count; ++i) {
        const std::string p = "mod." + std::to_string(i) + ".";
        OutletModule m;
        m.purpose        = bag.getEnum(p + "purpose", OutletPurpose::General);
        m.mountingHeight = bag.getReal(p + "height", 0.30);
        m.powerVA        = bag.getReal(p + "va", 100.0);
        m.description    = bag.getText(p + "desc");
        modules.push_back(std::move(m));
    }
    if (modules.empty()) setKind(kind);
    powerVA = totalVA();
}

} // namespace electrical
