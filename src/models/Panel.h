// =============================================================================
//  Panel.h - A load panel / distribution board (quadro de distribuicao).
// =============================================================================
#pragma once

#include "models/ElectricalElement.h"
#include <vector>

namespace electrical {

class Panel final : public ElectricalElement {
public:
    int         id = -1;             // unique panel id (Circuit::panelId references this)
    std::string name = "QD-1";       // panel designation
    bool        isMain = false;      // main board (QGBT) vs sub-board
    bool        embedded = false;    // embutido (flush in the wall) vs aparente (surface)
    int         parentPanelId = -1;  // upstream panel (-1 = fed by the utility)
    std::vector<int> circuitIds;     // circuits originating at this panel
    double      demandFactor = 1.0;  // applied to the summed connected load

    Panel() : ElectricalElement(ElementType::Panel) {
        mountingHeight = 1.60;
    }

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Panel"; }
};

} // namespace electrical
