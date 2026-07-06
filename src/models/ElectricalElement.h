// =============================================================================
//  ElectricalElement.h - Common base for every placed electrical device.
// =============================================================================
#pragma once

#include "models/Types.h"
#include "models/Serialization.h"
#include <string>

namespace electrical {

// Base class for lighting points, outlets, switches and panels. Each element is
// backed by a block reference in the drawing; `handle` is the AcDbHandle of that
// block reference stored as a hex string so the link survives save/reload.
class ElectricalElement : public ISerializable {
public:
    ElementType type;
    std::string handle;          // AcDbHandle (hex) of the owning block reference
    std::string tag;             // human tag, e.g. "L1", "TUG-3"
    Point3      position;        // insertion point (drawing units)
    double      mountingHeight = 0.0;   // meters above finished floor
    int         roomId = -1;     // owning Room id (-1 = unassigned)
    int         circuitId = -1;  // assigned Circuit id (-1 = unassigned)
    double      powerVA = 0.0;    // apparent power for the load schedule

    explicit ElectricalElement(ElementType t) : type(t) {}
    ~ElectricalElement() override = default;

    // Serializes the common fields; subclasses call this then add their own.
    void serializeBase(PropertyBag& bag) const;
    void deserializeBase(const PropertyBag& bag);
};

} // namespace electrical
