// =============================================================================
//  Types.h - Domain enumerations shared across the model layer.
//  Follows ABNT NBR 5410 (installations) and NBR 5444 (symbology) conventions.
// =============================================================================
#pragma once

#include <cstdint>

namespace electrical {

// Drawing / project unit. Internally all geometry is stored in the drawing's
// native units; this only controls scale interpretation and display.
enum class Unit : uint8_t { Meters, Centimeters, Decimeters, Millimeters };

// Number of live conductors of the supply.
enum class Phases : uint8_t { SinglePhase, TwoPhase, ThreePhase };

// Grounding arrangement per IEC 60364 / NBR 5410.
enum class GroundingSystem : uint8_t { TN_S, TN_C, TN_C_S, TT, IT };

// How conduits/wiring are installed (drives derating factors in NBR 5410).
enum class InstallationType : uint8_t { Surface, Concealed };

enum class ConduitMaterial : uint8_t { PVC_Rigid, PVC_Corrugated, Steel, Aluminum };

// Room intended use - governs the automatic lux target and outlet criteria.
enum class RoomUsage : uint8_t {
    LivingRoom, Bedroom, Kitchen, Bathroom, Hallway, ServiceArea,
    Garage, Office, DiningRoom, Balcony, Closet, Other
};

// Building function - governs default demand factors and load criteria.
enum class RoomFunction : uint8_t { Residential, Commercial, Industrial };

// Concrete electrical element kinds.
enum class ElementType : uint8_t { LightPoint, Outlet, Switch, Panel };

// Outlet module composition. Each module can have its own mounting height.
enum class OutletKind : uint8_t { Single, Duplex, Triplex };

// Outlet electrical purpose (NBR 5410 TUG/TUE distinction).
enum class OutletPurpose : uint8_t { General, Specific };

enum class SwitchKind : uint8_t { SinglePole, DoublePole, ThreeWay, FourWay };

// Circuit category, used by the automatic distributor and load schedule.
enum class CircuitType : uint8_t { Lighting, GeneralOutlets, SpecificOutlets, Motor, Mixed };

inline const char* toKey(Unit u) {
    switch (u) {
        case Unit::Meters:      return "m";
        case Unit::Centimeters: return "cm";
        case Unit::Decimeters:  return "dm";
        case Unit::Millimeters: return "mm";
    }
    return "m";
}

// Multiplier that converts one project unit into meters (for area/length calc).
inline double toMeters(Unit u) {
    switch (u) {
        case Unit::Meters:      return 1.0;
        case Unit::Decimeters:  return 0.1;
        case Unit::Centimeters: return 0.01;
        case Unit::Millimeters: return 0.001;
    }
    return 1.0;
}

} // namespace electrical
