// =============================================================================
//  Conduit.h - A conduit run segment carrying one or more circuits' wiring.
// =============================================================================
#pragma once

#include "models/Types.h"
#include "models/Serialization.h"
#include <string>
#include <vector>

namespace electrical {

// One wire pulled through the conduit (for fill calculation and BOM).
struct Wire {
    int         circuitId = -1;
    double      gaugeMM2 = 1.5;
    std::string color;          // e.g. phase/neutral/ground colour code
    double      lengthM = 0.0;
};

class Conduit final : public ISerializable {
public:
    int          id = -1;
    std::string  handle;         // handle of the polyline/line representing it
    ConduitMaterial material = ConduitMaterial::PVC_Corrugated;
    double       diameterMM = 20.0;   // nominal diameter

    // Poly-path of the run (drawing units). A "vertical drop" is a run whose
    // endpoints share XY but differ in Z (ceiling -> floor).
    std::vector<Point3> path;
    bool         isVerticalDrop = false;

    std::vector<Wire> wires;     // conductors inside this conduit
    double       lengthM = 0.0;  // total 3D length in meters

    void recomputeLength(Unit unit);

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Conduit"; }
};

} // namespace electrical
