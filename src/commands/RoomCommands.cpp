// =============================================================================
//  RoomCommands.cpp - EL-ROOM: demarcate a room from a selected polyline.
// =============================================================================
#include "commands/Commands.h"
#include "services/ProjectContext.h"
#include "ui/Localization.h"
#include "ui/UiBridge.h"
#include "utilities/GeometryHelper.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

namespace electrical {
namespace commands {

void insertRoom() {
    ProjectData& project = ProjectContext::instance().project();

    // Select the boundary polyline.
    ads_name ent;
    ads_point pt;
    const std::wstring prompt = L"\n" + EL_TRW("msg.selectPolyline") + L" ";
    if (acedEntSel(prompt.c_str(), ent, pt) != RTNORM) {
        acutPrintf(_T("\n%s\n"), EL_TRW("btn.cancel").c_str());
        return;
    }

    AcDbObjectId polyId;
    if (acdbGetObjectId(polyId, ent) != Acad::eOk) return;

    // Verify it is a polyline and read its vertices.
    std::vector<Point3> loop;
    if (!geom::polylineVertices(polyId, loop)) {
        acutPrintf(_T("\nSelected entity is not a valid closed polyline.\n"));
        return;
    }

    UndoGroup undo(_T("EL-ROOM"));

    Room room;
    room.id = project.allocRoomId();
    room.boundary = loop;
    room.recomputeArea(project.settings.unit);

    // Store the polyline handle so the link survives save/reload.
    AcDbHandle h;
    { AcDbObject* obj = nullptr;
      if (acdbOpenObject(obj, polyId, AcDb::kForRead) == Acad::eOk) {
          obj->getAcDbHandle(h); obj->close();
      } }
    ACHAR hbuf[64]; h.getIntoAsciiBuffer(hbuf);
    room.boundaryHandle = fromAcString(hbuf);

    // Let the user fill name / usage / function / ceiling height.
    if (!ui::showRoomDialog(room)) {
        // Cancelled: roll back the id allocation.
        project.nextRoomId--;
        return;
    }
    room.recomputeArea(project.settings.unit);

    project.rooms.push_back(room);
    ProjectContext::instance().save();

    acutPrintf(_T("\nRoom \"%s\": area = %.2f m2.\n"),
               toAcString(room.name).kwszPtr(), room.areaM2);
}

} // namespace commands
} // namespace electrical
