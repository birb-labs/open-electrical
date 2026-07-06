// =============================================================================
//  ProjectData.h - In-memory aggregate of an entire electrical project.
//
//  This is the single object the services operate on. DatabaseService loads it
//  from / saves it to the drawing's named dictionary.
// =============================================================================
#pragma once

#include "models/ProjectSettings.h"
#include "models/Room.h"
#include "models/LightPoint.h"
#include "models/Outlet.h"
#include "models/Switch.h"
#include "models/Panel.h"
#include "models/Circuit.h"
#include "models/Conduit.h"

#include <memory>
#include <vector>

namespace electrical {

class ProjectData {
public:
    ProjectSettings settings;

    std::vector<Room>    rooms;
    std::vector<Circuit> circuits;
    std::vector<Conduit> conduits;

    // Elements are polymorphic; stored as owning pointers to the base class.
    std::vector<std::unique_ptr<ElectricalElement>> elements;

    // ---- id allocation ---------------------------------------------------
    int nextRoomId    = 1;
    int nextCircuitId = 1;
    int nextConduitId = 1;

    int allocRoomId()    { return nextRoomId++; }
    int allocCircuitId() { return nextCircuitId++; }
    int allocConduitId() { return nextConduitId++; }

    // ---- lookups ---------------------------------------------------------
    Room*    findRoom(int id);
    Circuit* findCircuit(int id);
    Panel*   findPanel(int id);
    ElectricalElement* findElementByHandle(const std::string& handle);

    // Factory used by the persistence layer to rebuild the right subclass.
    static std::unique_ptr<ElectricalElement> makeElement(const std::string& typeTag);

    void clear();
};

} // namespace electrical
