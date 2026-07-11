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

    // Interactive-edit link (EL-CONDUIT / EL-CONDUIT-EDIT): the block references
    // this run connects. Empty for auto-routed runs. `bulge` curves a 2-point run
    // into a circular arc (0 = straight); it is the standard DXF bulge = tan(theta/4).
    std::string  srcHandle;
    std::string  dstHandle;
    double       bulge = 0.0;

    // Distinct circuits that travel through this run. Auto-routing fills it with
    // the circuit it is drawn for; a manual run derives it from the circuitId of
    // the components at each end. The wiring balloon sums conductorsFor() over
    // these ids, so a shared trunk carrying two circuits shows twice the wires.
    std::vector<int> circuitIds;

    // Mirrors the wiring balloon horizontally (ring gap + arm + rail flip to the
    // other side of the run). Toggled by EL-WIRE-FLIP so the tag can dodge nearby
    // geometry.
    bool mirror = false;

    // Handles of the wiring-distribution balloon entities (ring, arm, line, F/N/
    // PE/R labels) drawn by EL-WIRE for this run. Kept so a redraw or EL-DEL can
    // erase the old graphic instead of stacking duplicates.
    std::vector<std::string> annotationHandles;

    std::vector<Wire> wires;     // conductors inside this conduit
    double       lengthM = 0.0;  // total 3D length in meters

    void recomputeLength(Unit unit);

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Conduit"; }
};

} // namespace electrical
