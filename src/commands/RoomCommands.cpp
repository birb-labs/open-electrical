// =============================================================================
//  RoomCommands.cpp - EL-ROOM: demarcate a room by picking its corners.
//
//  The user clicks the room corners one by one (minimum 3). A rubber-band line
//  follows the cursor from the previous corner; pressing Enter closes the loop,
//  and the "Undo" keyword removes the last corner. The plugin then draws the
//  closed boundary polyline on the rooms layer and stores its handle.
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "services/ProjectContext.h"
#include "ui/Localization.h"
#include "ui/UiBridge.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <vector>

namespace electrical {
namespace commands {

// Collects the room corners interactively. Returns false if the user cancelled.
static bool pickCorners(std::vector<Point3>& corners) {
    corners.clear();
    ads_point prev = { 0, 0, 0 };

    while (true) {
        // Allow the "Undo" keyword; an empty return (Enter) is also accepted so
        // the user can close the loop once enough corners exist.
        acedInitGet(0, _T("Undo"));

        const std::wstring prompt =
            L"\n" + (corners.empty() ? EL_TRW("msg.roomFirstCorner")
                                     : EL_TRW("msg.roomNextCorner")) + L" ";

        ads_point p;
        const int rc = corners.empty()
            ? acedGetPoint(nullptr, prompt.c_str(), p)
            : acedGetPoint(prev,    prompt.c_str(), p);

        if (rc == RTNORM) {
            corners.emplace_back(p[X], p[Y], p[Z]);
            prev[X] = p[X]; prev[Y] = p[Y]; prev[Z] = p[Z];
        } else if (rc == RTNONE) {
            // Enter: finish if the loop is valid, otherwise keep asking.
            if (corners.size() >= 3) return true;
            acutPrintf(_T("\n%s\n"), EL_TRW("msg.roomNeedCorners").c_str());
        } else if (rc == RTKWORD) {
            ACHAR kw[64] = {0};
            acedGetInput(kw);
            if (kw[0] == _T('U') || kw[0] == _T('u')) {
                if (!corners.empty()) {
                    corners.pop_back();
                    if (!corners.empty()) {
                        const Point3& b = corners.back();
                        prev[X] = b.x; prev[Y] = b.y; prev[Z] = b.z;
                    }
                    acutPrintf(_T("\n%s\n"), EL_TRW("msg.roomUndone").c_str());
                }
            }
        } else {
            // RTCAN (Esc) or error.
            return false;
        }
    }
}

void insertRoom() {
    ProjectData& project = ProjectContext::instance().project();

    std::vector<Point3> corners;
    if (!pickCorners(corners)) {
        acutPrintf(_T("\n%s\n"), EL_TRW("btn.cancel").c_str());
        return;
    }

    UndoGroup undo(_T("EL-ROOM"));

    Room room;
    room.id = project.allocRoomId();
    room.boundary = corners;
    room.recomputeArea(project.settings.unit);

    // Draw the closed boundary polyline on the rooms layer and keep its handle
    // so the model <-> drawing link survives save/reload.
    cmdutil::ensureLayer(cmdutil::kLayerRooms, 8 /* gray */);
    room.boundaryHandle = cmdutil::drawPolyline(cmdutil::kLayerRooms, corners, true);

    // Let the user fill name / usage / function / ceiling height.
    if (!ui::showRoomDialog(room)) {
        // Cancelled: roll back the id allocation.
        project.nextRoomId--;
        return;
    }
    room.recomputeArea(project.settings.unit);

    project.rooms.push_back(room);
    ProjectContext::instance().save();

    acutPrintf(_T("\nRoom \"%s\": %d corners, area = %.2f m2.\n"),
               toAcString(room.name).kwszPtr(),
               static_cast<int>(room.boundary.size()), room.areaM2);
}

} // namespace commands
} // namespace electrical
