// =============================================================================
//  ReportCommands.cpp - EL-REPORT (documents) and EL-UNDO (roll back).
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "services/ProjectContext.h"
#include "services/ReportGenerator.h"
#include "ui/Localization.h"
#include "utilities/GeometryHelper.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <cwchar>

namespace electrical {
namespace commands {

using namespace cmdutil;

namespace {

// Places a multi-line report as MText at a picked point.
void placeReport(const std::string& utf8, const Point3& at) {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* bt = nullptr;
    db->getBlockTable(bt, AcDb::kForRead);
    AcDbObjectId msId; bt->getAt(ACDB_MODEL_SPACE, msId); bt->close();

    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, msId, AcDb::kForWrite) != Acad::eOk) return;

    ensureLayer(kLayerText, 7);
    AcDbMText* mt = new AcDbMText();
    mt->setLayer(kLayerText);
    mt->setLocation(toAcGe(at));
    mt->setContents(toAcString(utf8).kwszPtr());
    mt->setTextHeight(0.2);
    mt->setDatabaseDefaults();
    ms->appendAcDbEntity(mt);
    mt->close();
    ms->close();
}

} // namespace

void generateReport() {
    ProjectData& project = ProjectContext::instance().project();

    ACHAR res[32] = {0};
    acedInitGet(0, _T("Legend Loads Bom Sld All"));
    if (acedGetKword(
            _T("\nReport [Legend/Loads/Bom/Sld/All] <All>: "), res) != RTNORM)
        wcscpy(res, _T("All"));

    Point3 at;
    if (!pickPoint(L"Report insertion point", at)) return;

    UndoGroup undo(_T("EL-REPORT"));
    const std::wstring which(res);
    double dy = 0.0;
    auto emit = [&](const std::string& text) {
        Point3 p = at; p.y -= dy;
        placeReport(text, p);
        dy += 4.0;
    };

    if (which == L"Legend" || which == L"All") emit(ReportGenerator::legend(project));
    if (which == L"Loads"  || which == L"All") emit(ReportGenerator::loadSchedule(project));
    if (which == L"Bom"    || which == L"All") emit(ReportGenerator::billOfMaterialsText(project));
    if (which == L"Sld"    || which == L"All") emit(ReportGenerator::singleLineDiagram(project));

    acutPrintf(_T("\n%s\n"), EL_TRW("msg.done").c_str());
}

void generateLoadSchedule() {
    ProjectData& project = ProjectContext::instance().project();
    Point3 at;
    if (!pickPoint(L"Load schedule insertion point", at)) return;
    UndoGroup undo(_T("EL-LOAD-SCHEDULE"));
    placeReport(ReportGenerator::loadSchedule(project), at);
    acutPrintf(_T("\n%s\n"), EL_TRW("msg.done").c_str());
}

void generateSingleLine() {
    ProjectData& project = ProjectContext::instance().project();
    Point3 at;
    if (!pickPoint(L"Single-line diagram insertion point", at)) return;
    UndoGroup undo(_T("EL-SINGLE-LINE"));
    placeReport(ReportGenerator::singleLineDiagram(project), at);
    acutPrintf(_T("\n%s\n"), EL_TRW("msg.done").c_str());
}

void undoLast() {
    if (undoLastPluginAction()) {
        // The drawing rolled back; reload the model from the (restored) dwg.
        ProjectContext::instance().reload();
        acutPrintf(_T("\n%s\n"), EL_TRW("msg.undone").c_str());
    } else {
        acutPrintf(_T("\nNothing to undo.\n"));
    }
}

} // namespace commands
} // namespace electrical
