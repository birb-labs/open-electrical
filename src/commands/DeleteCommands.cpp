// =============================================================================
//  DeleteCommands.cpp - Removal commands: EL-DEL (components/conduits) and
//  EL-DEL-ROOM (a room boundary, chosen from a list).
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "models/ProjectData.h"
#include "models/ElectricalElement.h"
#include "services/ProjectContext.h"
#include "ui/UiBridge.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <string>
#include <vector>

namespace electrical {
namespace commands {

using namespace cmdutil;

namespace {

// Hex handle of a drawing entity (matches ElectricalElement::handle /
// Conduit::handle). Empty on failure.
std::string handleOfId(AcDbObjectId id) {
    AcDbEntity* e = nullptr;
    std::string s;
    if (acdbOpenObject(e, id, AcDb::kForRead) == Acad::eOk) {
        AcDbHandle h; e->getAcDbHandle(h);
        ACHAR buf[64]; h.getIntoAsciiBuffer(buf);
        s = fromAcString(buf);
        e->close();
    }
    return s;
}

// Drops any project element/conduit backed by handle `h`. Returns true if the
// model changed (so we know the pick was one of ours vs. a stray drawing entity).
bool dropFromModel(ProjectData& project, const std::string& h) {
    bool changed = false;
    for (auto it = project.elements.begin(); it != project.elements.end(); ) {
        if ((*it)->handle == h) { it = project.elements.erase(it); changed = true; }
        else ++it;
    }
    for (auto it = project.conduits.begin(); it != project.conduits.end(); ) {
        if (it->handle == h) {
            // Erase this run's wiring balloon too, so no orphan tag is left behind.
            for (const std::string& ah : it->annotationHandles) {
                AcDbObjectId aid;
                if (entityIdByHandle(ah, aid)) eraseEntity(aid);
            }
            it = project.conduits.erase(it); changed = true;
        }
        else ++it;
    }
    return changed;
}

} // namespace

// EL-DEL: select any number of placed components and/or conduits; each selected
// entity is erased from the drawing and dropped from the project model (so the
// load schedule / material list stay in sync).
void deleteSelected() {
    ProjectData& project = ProjectContext::instance().project();

    ads_name ss;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, ss) != RTNORM) {
        acutPrintf(_T("\nNothing selected.\n"));
        return;
    }
    Adesk::Int32 n = 0;
    acedSSLength(ss, &n);

    UndoGroup undo(_T("EL-DEL"));
    int erased = 0, modelHits = 0;
    for (Adesk::Int32 i = 0; i < n; ++i) {
        ads_name en;
        if (acedSSName(ss, i, en) != RTNORM) continue;
        AcDbObjectId id;
        if (acdbGetObjectId(id, en) != Acad::eOk) continue;
        if (dropFromModel(project, handleOfId(id))) ++modelHits;
        if (eraseEntity(id)) ++erased;
    }
    acedSSFree(ss);

    ProjectContext::instance().save();
    ui::refreshPalette();
    acutPrintf(_T("\n%d entity(ies) removed (%d tracked in the project).\n"), erased, modelHits);
}

// EL-DEL-ROOM: pick a room from a list and remove ONLY its boundary. Placed
// components stay put; their room link is cleared so nothing points at a room
// that no longer exists.
void deleteRoom() {
    ProjectData& project = ProjectContext::instance().project();
    if (project.rooms.empty()) {
        acutPrintf(_T("\nNo rooms to remove.\n"));
        return;
    }

    std::vector<std::string> names;
    names.reserve(project.rooms.size());
    for (const auto& r : project.rooms)
        names.push_back(r.name + " (#" + std::to_string(r.id) + ")");

    int idx = -1;
    if (!ui::showRoomPicker(names, idx)) return;
    if (idx < 0 || idx >= static_cast<int>(project.rooms.size())) return;

    UndoGroup undo(_T("EL-DEL-ROOM"));
    const Room& room = project.rooms[idx];
    const int roomId = room.id;

    AcDbObjectId bid;
    if (!room.boundaryHandle.empty() && entityIdByHandle(room.boundaryHandle, bid))
        eraseEntity(bid);   // erase only the boundary polyline

    for (auto& e : project.elements)
        if (e->roomId == roomId) e->roomId = -1;   // keep components, detach room

    project.rooms.erase(project.rooms.begin() + idx);
    ProjectContext::instance().save();
    ui::refreshPalette();
    acutPrintf(_T("\nRoom removed (boundary only; components kept).\n"));
}

} // namespace commands
} // namespace electrical
