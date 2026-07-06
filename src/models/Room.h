// =============================================================================
//  Room.h - A demarcated room / environment.
// =============================================================================
#pragma once

#include "models/Types.h"
#include "models/Serialization.h"
#include <string>
#include <vector>

namespace electrical {

// An opening in the room boundary (door or window). Used by the automatic
// router to avoid crossing openings and to place switches beside doors.
struct Opening {
    enum class Kind : uint8_t { Door, Window } kind = Kind::Door;
    Point3 start;    // opening endpoints on the boundary
    Point3 end;
    double sillHeight = 0.0;   // window sill height (m); 0 for doors
};

class Room final : public ISerializable {
public:
    int          id = -1;
    std::string  name = "Room";
    RoomUsage    usage = RoomUsage::LivingRoom;
    RoomFunction function = RoomFunction::Residential;
    double       ceilingHeight = 2.80;   // meters

    std::string  boundaryHandle;         // handle of the demarcating polyline
    std::vector<Point3>  boundary;       // closed loop (drawing units)
    std::vector<Opening> openings;

    // Area in square meters, recomputed from the boundary (shoelace formula).
    double areaM2 = 0.0;

    // Recomputes areaM2 from `boundary` given the project unit.
    void recomputeArea(Unit unit);

    // Perimeter in meters - used by the automatic outlet spacing (NBR 5410).
    double perimeterM(Unit unit) const;

    void serialize(PropertyBag& bag) const override;
    void deserialize(const PropertyBag& bag) override;
    std::string typeTag() const override { return "Room"; }
};

} // namespace electrical
