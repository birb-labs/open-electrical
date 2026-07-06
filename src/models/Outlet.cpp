#include "models/Outlet.h"

namespace electrical {

void Outlet::setKind(OutletKind k) {
    kind = k;
    const size_t want = (k == OutletKind::Single) ? 1
                      : (k == OutletKind::Duplex) ? 2 : 3;
    if (modules.size() < want) {
        while (modules.size() < want) modules.emplace_back();
    } else if (modules.size() > want) {
        modules.resize(want);
    }
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
