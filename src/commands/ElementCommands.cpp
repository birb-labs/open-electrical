// =============================================================================
//  ElementCommands.cpp - Insert lighting points, outlets, switches, panels.
//  Each command has a manual (pick) and, where applicable, an automatic variant.
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "services/ProjectContext.h"
#include "services/LightingCalculator.h"
#include "services/DatabaseService.h"
#include "ui/Localization.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <memory>

namespace electrical {
namespace commands {

using namespace cmdutil;

namespace {

void finalize(std::unique_ptr<ElectricalElement> e, const char* what) {
    ProjectData& project = ProjectContext::instance().project();
    if (auto* room = roomAt(project, e->position)) e->roomId = room->id;
    project.elements.push_back(std::move(e));
    ProjectContext::instance().save();
    acutPrintf(_T("\n%s inserted.\n"), toAcString(std::string(what)).kwszPtr());
}

} // namespace

// ---- Lighting --------------------------------------------------------------
void insertLight() {
    Point3 p;
    if (!pickPoint(EL_TRW("msg.pickPoint"), p)) return;
    UndoGroup undo(_T("EL-LIGHT"));

    auto lp = std::make_unique<LightPoint>();
    lp->position = p;
    lp->powerVA = lp->lampCount * lp->wattsPerLamp;
    lp->handle = insertSymbol(_T("EL_LIGHT"), kLayerLighting, p);
    finalize(std::move(lp), "Lighting point");
}

void insertLightAuto() {
    ProjectData& project = ProjectContext::instance().project();
    if (project.rooms.empty()) {
        acutPrintf(_T("\nNo rooms defined. Use EL-ROOM first.\n"));
        return;
    }
    UndoGroup undo(_T("EL-LIGHT-AUTO"));

    int total = 0;
    for (const auto& room : project.rooms) {
        LightingResult r = LightingCalculator::calculate(room, project.settings.unit);
        for (const auto& pos : r.positions) {
            auto lp = std::make_unique<LightPoint>();
            lp->position = pos;
            lp->roomId = room.id;
            lp->lumensPerLamp = 2000.0;
            lp->wattsPerLamp = 18.0;
            lp->powerVA = lp->wattsPerLamp;
            lp->handle = insertSymbol(_T("EL_LIGHT"), kLayerLighting, pos);
            project.elements.push_back(std::move(lp));
            ++total;
        }
        acutPrintf(_T("\nRoom \"%s\": %d luminaire(s), target %.0f lux, achieved %.0f lux.\n"),
                   toAcString(room.name).kwszPtr(), (int)r.positions.size(),
                   r.requiredLux, r.achievedLux);
    }
    ProjectContext::instance().save();
    acutPrintf(_T("\n%d lighting point(s) placed automatically.\n"), total);
}

// ---- Outlets ---------------------------------------------------------------
void insertOutlet() {
    Point3 p;
    if (!pickPoint(EL_TRW("msg.pickPoint"), p)) return;

    // Ask kind via keyword.
    int kw = 0;
    acedInitGet(0, _T("Single Duplex Triplex"));
    ACHAR res[64] = {0};
    acutPrintf(_T("\nOutlet type "));
    if (acedGetKword(_T("[Single/Duplex/Triplex] <Single>: "), res) != RTNORM)
        wcscpy(res, _T("Single"));
    (void)kw;

    UndoGroup undo(_T("EL-OUTLET"));
    auto o = std::make_unique<Outlet>();
    if (wcscmp(res, _T("Duplex")) == 0)  o->setKind(OutletKind::Duplex);
    else if (wcscmp(res, _T("Triplex")) == 0) o->setKind(OutletKind::Triplex);
    else o->setKind(OutletKind::Single);
    o->position = p;
    o->powerVA = o->totalVA();
    o->handle = insertSymbol(_T("EL_OUTLET"), kLayerOutlets, p);
    finalize(std::move(o), "Outlet");
}

void insertOutletAuto() {
    ProjectData& project = ProjectContext::instance().project();
    if (project.rooms.empty()) {
        acutPrintf(_T("\nNo rooms defined. Use EL-ROOM first.\n"));
        return;
    }
    UndoGroup undo(_T("EL-OUTLET-AUTO"));

    // NBR 5410 general-outlet criteria (residential, simplified):
    //  - rooms > 6 m2: at least 1 outlet every 5 m of perimeter.
    //  - wet areas (kitchen/service): 1 every 3.5 m.
    int total = 0;
    for (const auto& room : project.rooms) {
        const double per = room.perimeterM(project.settings.unit);
        double spacing = 5.0;
        if (room.usage == RoomUsage::Kitchen || room.usage == RoomUsage::ServiceArea)
            spacing = 3.5;
        int count = std::max(1, static_cast<int>(per / spacing));

        // Distribute along the boundary.
        const double m = toMeters(project.settings.unit);
        double target = 0.0;
        double acc = 0.0;
        const auto& b = room.boundary;
        const double step = per / count;
        for (int k = 0; k < count; ++k) {
            target = (k + 0.5) * step;   // meters along perimeter
            acc = 0.0;
            Point3 pos = b.empty() ? Point3() : b.front();
            for (size_t i = 0; i < b.size(); ++i) {
                const Point3& a = b[i];
                const Point3& c = b[(i + 1) % b.size()];
                const double segLen = std::hypot(c.x - a.x, c.y - a.y) * m;
                if (acc + segLen >= target) {
                    const double t = (target - acc) / (segLen > 0 ? segLen : 1.0);
                    pos = Point3(a.x + t * (c.x - a.x), a.y + t * (c.y - a.y), 0.0);
                    break;
                }
                acc += segLen;
            }
            auto o = std::make_unique<Outlet>();
            o->setKind(OutletKind::Single);
            o->position = pos;
            o->roomId = room.id;
            o->powerVA = o->totalVA();
            o->handle = insertSymbol(_T("EL_OUTLET"), kLayerOutlets, pos);
            project.elements.push_back(std::move(o));
            ++total;
        }
        acutPrintf(_T("\nRoom \"%s\": %d outlet(s) (perimeter %.1f m).\n"),
                   toAcString(room.name).kwszPtr(), count, per);
    }
    ProjectContext::instance().save();
    acutPrintf(_T("\n%d outlet(s) placed automatically.\n"), total);
}

// ---- Switches --------------------------------------------------------------
void insertSwitch() {
    Point3 p;
    if (!pickPoint(EL_TRW("msg.pickPoint"), p)) return;
    UndoGroup undo(_T("EL-SWITCH"));

    auto s = std::make_unique<Switch>();
    s->position = p;
    s->handle = insertSymbol(_T("EL_SWITCH"), kLayerSwitches, p);
    finalize(std::move(s), "Switch");
}

void insertSwitchAuto() {
    ProjectData& project = ProjectContext::instance().project();
    if (project.rooms.empty()) {
        acutPrintf(_T("\nNo rooms defined. Use EL-ROOM first.\n"));
        return;
    }
    UndoGroup undo(_T("EL-SWITCH-AUTO"));

    // Place one switch beside the first door of each room (fallback: near a
    // boundary vertex) to control that room's lighting.
    int total = 0;
    for (const auto& room : project.rooms) {
        Point3 pos;
        if (!room.openings.empty()) {
            const auto& o = room.openings.front();
            pos = Point3((o.start.x + o.end.x) * 0.5, (o.start.y + o.end.y) * 0.5, 0.0);
        } else if (!room.boundary.empty()) {
            pos = room.boundary.front();
        }
        auto s = std::make_unique<Switch>();
        s->position = pos;
        s->roomId = room.id;
        s->handle = insertSymbol(_T("EL_SWITCH"), kLayerSwitches, pos);
        // Link to the room's lighting points.
        for (const auto& e : project.elements)
            if (e->type == ElementType::LightPoint && e->roomId == room.id)
                s->controlledLightHandles.push_back(e->handle);
        project.elements.push_back(std::move(s));
        ++total;
    }
    ProjectContext::instance().save();
    acutPrintf(_T("\n%d switch(es) placed automatically.\n"), total);
}

// ---- Panels ----------------------------------------------------------------
void insertPanel() {
    Point3 p;
    if (!pickPoint(EL_TRW("msg.pickPoint"), p)) return;
    UndoGroup undo(_T("EL-PANEL"));

    auto panel = std::make_unique<Panel>();
    panel->position = p;
    ProjectData& project = ProjectContext::instance().project();
    // First panel is the main board.
    bool anyPanel = false;
    for (const auto& e : project.elements)
        if (e->type == ElementType::Panel) { anyPanel = true; break; }
    panel->isMain = !anyPanel;
    panel->name = panel->isMain ? "QGBT" : "QD";
    panel->handle = insertSymbol(_T("EL_PANEL"), kLayerPanels, p, 1.5);
    finalize(std::move(panel), "Load panel");
}

} // namespace commands
} // namespace electrical
