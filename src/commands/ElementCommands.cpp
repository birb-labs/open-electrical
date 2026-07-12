// =============================================================================
//  ElementCommands.cpp - Insert lighting points, outlets, switches, panels.
//  Each command has a manual (pick) and, where applicable, an automatic variant.
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "services/ProjectContext.h"
#include "services/LightingCalculator.h"
#include "services/DatabaseService.h"
#include "services/SymbolFactory.h"
#include "ui/Localization.h"
#include "ui/UiBridge.h"
#include "utilities/Diag.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

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

// Chooses the outlet block: the combined outlet+switch symbols use their own
// fixed block, everything else is a chain of 1-3 triangles named from each
// module's mounting height (NBR 5444 low/medium/high fill convention).
std::wstring outletBlockFor(const Outlet& o) {
    if (o.kind == OutletKind::WithSwitch2)      return SymbolFactory::kOutletSwitch2;
    if (o.kind == OutletKind::DuplexWithSwitch1) return SymbolFactory::kOutletDuplexSwitch1;
    std::vector<double> heights;
    for (const auto& m : o.modules) heights.push_back(m.mountingHeight);
    if (heights.empty()) heights.push_back(0.30);
    return SymbolFactory::outletChainBlockName(heights);
}

// Chooses the switch block by kind (section count / special variant).
const ACHAR* switchBlock(const Switch& s) {
    switch (s.kind) {
        case SwitchKind::OneSection:   return SymbolFactory::kSwitch1;
        case SwitchKind::TwoSection:   return SymbolFactory::kSwitch2;
        case SwitchKind::ThreeSection: return SymbolFactory::kSwitch3;
        case SwitchKind::ThreeWay:     return SymbolFactory::kSwitch3Way;
        case SwitchKind::FourWay:      return SymbolFactory::kSwitch4Way;
        case SwitchKind::Bell:         return SymbolFactory::kSwitchBell;
    }
    return SymbolFactory::kSwitch1;
}

std::wstring wide(const std::string& s) { return std::wstring(toAcString(s).kwszPtr()); }

// The block that represents `e` in its current configuration. Used both on
// insertion and by EL-EDIT to decide whether the geometry must be recreated.
std::wstring blockNameFor(const ElectricalElement& e) {
    switch (e.type) {
        case ElementType::LightPoint:
            return static_cast<const LightPoint&>(e).wallMounted
                 ? SymbolFactory::kLightWall : SymbolFactory::kLight;
        case ElementType::Outlet: return outletBlockFor(static_cast<const Outlet&>(e));
        case ElementType::Switch: return switchBlock(static_cast<const Switch&>(e));
        case ElementType::Panel:
            return static_cast<const Panel&>(e).embedded
                 ? SymbolFactory::kPanelFlush : SymbolFactory::kPanelSurface;
    }
    return {};
}

const ACHAR* layerFor(const ElectricalElement& e) {
    switch (e.type) {
        case ElementType::LightPoint: return kLayerLighting;
        case ElementType::Outlet:     return kLayerOutlets;
        case ElementType::Switch:     return kLayerSwitches;
        case ElementType::Panel:      return kLayerPanels;
    }
    return kLayerText;
}

double scaleFor(const ElectricalElement&) {
    // All symbols are inserted at 1:1 - each block already carries its final,
    // proportional size (SymbolFactory derives every dimension from kSymUnit).
    // The panel used to be scaled 1.5x, which made the QGBT look oversized.
    return 1.0;
}

// Attribute values in the order each block defines its attribute definitions
// (see SymbolFactory). insertSymbol()/updateBlockAttributes() match by order.
std::vector<SymbolAttr> attrsFor(const ElectricalElement& e) {
    switch (e.type) {
        case ElementType::LightPoint: {
            const auto& l = static_cast<const LightPoint&>(e);
            const std::wstring pot = l.powerText.empty()
                ? std::to_wstring(l.lampCount) + L"x" +
                  std::to_wstring(static_cast<int>(l.wattsPerLamp)) + L"W"
                : wide(l.powerText);
            if (l.wallMounted)   // arandela: command + circuit, no hyphens
                return { { L"COMANDO", wide(l.noteLetter) },
                         { L"CIRCUITO", wide(l.circuitLabel) } };
            // Ceiling: circuit number is drawn between hyphens, e.g. "-3-".
            const std::wstring circ = l.circuitLabel.empty()
                ? std::wstring() : L"-" + wide(l.circuitLabel) + L"-";
            return { { L"POTENCIA", pot },
                     { L"COMANDO",  wide(l.noteLetter) },
                     { L"CIRCUITO", circ } };
        }
        case ElementType::Outlet: {
            const auto& o = static_cast<const Outlet&>(e);
            return { { L"POTENCIA", std::to_wstring(static_cast<int>(o.totalVA())) },
                     { L"CIRCUITO", wide(o.circuitLabel) } };
        }
        case ElementType::Switch: {
            const auto& s = static_cast<const Switch&>(e);
            std::vector<SymbolAttr> a;
            for (size_t i = 0; i < s.commands.size(); ++i)
                a.push_back({ L"COMANDO" + std::to_wstring(i + 1), wide(s.commands[i]) });
            if (a.empty()) a.push_back({ L"COMANDO1", L"" });
            return a;
        }
        case ElementType::Panel: {
            const auto& p = static_cast<const Panel&>(e);
            return { { L"NOME", wide(p.name) },
                     { L"CIRCUITOS", p.circuitIds.empty()
                         ? std::wstring()
                         : std::to_wstring(p.circuitIds.size()) } };
        }
    }
    return {};
}

// Defaults stamped on automatically-placed luminaires so their notation is never
// blank. Both are meant to be refined later - the command letter with EL-EDIT, the
// circuit by EL-CIRCUIT-AUTO (which rewrites CIRCUITO in place).
constexpr const char* kAutoCommand = "A";
constexpr const char* kAutoCircuit = "?";

// Assembles the lumen-method input from a room's own luminotechnical parameters
// plus the chosen luminaire. The target lux is left to derive from the room's
// usage + contrast (no override); CU derives from the room index unless the room
// stored a manual value.
LightingInput lightingInputForRoom(const Room& r, double lumens, double watts) {
    LightingInput in;
    in.lumensPerLuminaire = lumens;
    in.wattsPerLuminaire  = watts;
    in.maintenanceFactor  = (r.maintenanceFactor > 0.0) ? r.maintenanceFactor : 0.80;
    in.cuOverride         = r.utilizationFactor;   // 0 = derive from the room index
    in.ceilingReflectance = r.ceilingReflectance;
    in.wallReflectance    = r.wallReflectance;
    in.floorReflectance   = r.floorReflectance;
    in.workPlaneHeight    = r.workPlaneHeight;
    in.contrast           = r.contrast;
    return in;
}

// Runs the lumen method for one room and inserts its luminaires. Returns the
// number placed; a room with no boundary polygon places nothing.
int placeLightsInRoom(ProjectData& project, const Room& room, double lumens, double watts) {
    if (room.boundary.size() < 3) {
        acutPrintf(_T("\nRoom \"%s\": no boundary polygon, skipped.\n"),
                   toAcString(room.name).kwszPtr());
        return 0;
    }
    const LightingInput in = lightingInputForRoom(room, lumens, watts);
    const LightingResult r = LightingCalculator::calculate(room, project.settings.unit, in);
    EL_DIAG_LOG("placeLightsInRoom: calculator returned positions");
    if (r.positions.empty()) {
        acutPrintf(_T("\nRoom \"%s\": the lumen method produced no positions ")
                   _T("(check the boundary and ceiling height).\n"),
                   toAcString(room.name).kwszPtr());
        return 0;
    }
    int inserted = 0;
    for (const auto& pos : r.positions) {
        auto lp = std::make_unique<LightPoint>();
        lp->position      = pos;
        lp->roomId        = room.id;
        lp->lampCount     = 1;
        lp->lumensPerLamp = lumens;
        lp->wattsPerLamp  = watts;
        lp->powerVA       = watts;
        // Auto-placed luminaires sit flush with the ceiling (offset 0); pos.z is
        // already the ceiling height in drawing units (LightingCalculator).
        lp->offsetFromCeiling = 0.0;
        lp->mountingHeight    = room.ceilingHeight;
        lp->powerText     = "1x" + std::to_string(static_cast<int>(watts)) + "W";
        lp->noteLetter    = kAutoCommand;
        lp->circuitLabel  = kAutoCircuit;
        lp->handle = insertSymbol(blockNameFor(*lp).c_str(), layerFor(*lp), pos,
                                  scaleFor(*lp), 0.0, attrsFor(*lp));
        if (lp->handle.empty())
            acutPrintf(_T("\nWarning: a luminaire could not be inserted ")
                       _T("(block \"%s\").\n"), blockNameFor(*lp).c_str());
        else
            ++inserted;
        project.elements.push_back(std::move(lp));
    }
    EL_DIAG_LOG("placeLightsInRoom: inserts done");
    acutPrintf(_T("\nRoom \"%s\": %d luminaire(s), E=%.0f lux (achieved %.0f), ")
               _T("A=%.2f m2, K=%.2f, CU=%.2f.\n"),
               toAcString(room.name).kwszPtr(), inserted,
               r.requiredLux, r.achievedLux, r.areaM2, r.roomIndex, r.utilizationFactor);
    return inserted;
}

// Shared entry for EL-CALC-LIGHT / EL-LIGHT-AUTO: pick room(s) + luminaire, then
// run the lumen method per room using each room's own parameters.
void runLightingCalc(const ACHAR* undoName) {
    ProjectData& project = ProjectContext::instance().project();
    if (project.rooms.empty()) {
        acutPrintf(_T("\nNo rooms defined. Use EL-ROOM first, then set each room's type/usage.\n"));
        return;
    }

    std::vector<std::string> names;
    names.reserve(project.rooms.size());
    for (const auto& r : project.rooms) names.push_back(r.name);

    ui::LightingRunOptions opt;
    if (!ui::showLightingRunDialog(names, opt)) return;

    UndoGroup undo(undoName);
    int total = 0, rooms = 0;
    for (size_t i = 0; i < project.rooms.size(); ++i) {
        if (opt.roomIndex >= 0 && static_cast<size_t>(opt.roomIndex) != i) continue;
        total += placeLightsInRoom(project, project.rooms[i], opt.lumens, opt.watts);
        ++rooms;
    }
    ProjectContext::instance().save();
    acutPrintf(_T("\n%d lighting point(s) placed across %d room(s).\n"), total, rooms);
}

} // namespace

// ---- Lighting --------------------------------------------------------------
void insertLight() {
    Point3 p;
    if (!pickPoint(EL_TRW("msg.pickPoint"), p)) return;

    // Configuration dialog first, then place.
    LightPoint lp;
    if (!ui::showLightDialog(lp)) return;

    // Wall luminaires are constrained to the wall's perpendicular; ceiling
    // lighting points never rotate.
    double rot = 0.0;
    if (lp.wallMounted) pickRotationOnWall(p, rot);

    // Ceiling luminaires are referenced to the ceiling (NBR 5444): drop the block
    // to Z = ceilingHeight - offsetFromCeiling. The ceiling height comes from the
    // room under the pick point (else a 2.8 m default). Wall luminaires keep the
    // picked Z (their height is chosen by the wall pick).
    ProjectData& project = ProjectContext::instance().project();
    const double mPerUnit = toMeters(project.settings.unit);
    const double invUnit  = mPerUnit > 0.0 ? 1.0 / mPerUnit : 1.0;
    double ceilingM = 2.8;
    if (const Room* r = roomAt(project, p)) ceilingM = r->ceilingHeight;

    UndoGroup undo(_T("EL-LIGHT"));
    auto elem = std::make_unique<LightPoint>(lp);
    if (!elem->wallMounted) {
        elem->mountingHeight = std::max(0.0, ceilingM - elem->offsetFromCeiling);
        p.z = elem->mountingHeight * invUnit;
    }
    elem->position = p;
    elem->handle = insertSymbol(blockNameFor(*elem).c_str(), layerFor(*elem), p,
                                scaleFor(*elem), rot, attrsFor(*elem));
    finalize(std::move(elem), "Lighting point");
}

// EL-CALC-LIGHT: lists the rooms for the user to pick one (or "All"), asks for the
// luminaire (flux/power), then runs the lumen method per room. The target
// illuminance is derived from each room's type + usage (NBR 5413) and its
// contrast; reflectances, MF, CU and heights all come from the room itself.
void calcLightingAuto() { runLightingCalc(_T("EL-CALC-LIGHT")); }

// Rewrites `e`'s block attributes from its current model state (matched by tag).
// Used by EL-CIRCUIT-AUTO to push freshly-assigned circuit numbers into the
// drawing; a no-op for elements whose entity is gone.
void syncElementAttributes(ElectricalElement& e) {
    AcDbObjectId id;
    if (!entityIdByHandle(e.handle, id)) return;
    updateBlockAttributes(id, attrsFor(e));
}

// ---- Outlets ---------------------------------------------------------------
void insertOutlet() {
    Point3 p;
    if (!pickPoint(EL_TRW("msg.pickPoint"), p)) return;

    // Configuration dialog (type, purpose, height, VA).
    Outlet cfg;
    if (!ui::showOutletDialog(cfg)) return;

    double rot = 0.0;
    pickRotationOnWall(p, rot);

    UndoGroup undo(_T("EL-OUTLET"));
    auto o = std::make_unique<Outlet>(cfg);
    o->position = p;
    o->powerVA = o->totalVA();
    o->handle = insertSymbol(blockNameFor(*o).c_str(), layerFor(*o), p,
                             scaleFor(*o), rot, attrsFor(*o));
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
            o->handle = insertSymbol(SymbolFactory::kOutletLow, kLayerOutlets, pos);
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

    Switch cfg;
    if (!ui::showSwitchDialog(cfg)) return;

    double rot = 0.0;
    pickRotationOnWall(p, rot);

    UndoGroup undo(_T("EL-SWITCH"));
    auto s = std::make_unique<Switch>(cfg);
    s->position = p;
    s->handle = insertSymbol(blockNameFor(*s).c_str(), layerFor(*s), p,
                             scaleFor(*s), rot, attrsFor(*s));
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
        s->handle = insertSymbol(SymbolFactory::kSwitch1, kLayerSwitches, pos);
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

    // Default: first panel is the main board (QGBT).
    Panel cfg;
    ProjectData& project = ProjectContext::instance().project();
    bool anyPanel = false;
    for (const auto& e : project.elements)
        if (e->type == ElementType::Panel) { anyPanel = true; break; }
    cfg.isMain = !anyPanel;
    cfg.name   = cfg.isMain ? "QGBT" : "QD";

    if (!ui::showPanelDialog(cfg)) return;

    double rot = 0.0;
    pickRotationOnWall(p, rot);

    UndoGroup undo(_T("EL-PANEL"));
    auto panel = std::make_unique<Panel>(cfg);
    if (panel->id < 0) panel->id = project.allocPanelId();   // stable id for Circuit::panelId
    panel->position = p;
    panel->handle = insertSymbol(blockNameFor(*panel).c_str(), layerFor(*panel), p,
                                 scaleFor(*panel), rot, attrsFor(*panel));
    finalize(std::move(panel), "Load panel");
}

// ---- Editing ---------------------------------------------------------------
// EL-EDIT: pick a placed component, re-open its configuration dialog, then push
// the changes back into the model, the drawing dictionary and the symbol. When
// the new configuration maps to the same block, only the attributes are
// rewritten (references stay intact); when the geometry changes (e.g. a switch
// grows a section, or an outlet gains a module) the block reference is recreated
// at the same position and rotation.
void editElement() {
    AcDbObjectId id;
    if (!pickEntity(L"Select an electrical component to edit", id)) return;

    // Resolve the entity's handle so we can find its model object.
    std::string hex;
    {
        AcDbEntity* ent = nullptr;
        if (acdbOpenObject(ent, id, AcDb::kForRead) != Acad::eOk) return;
        AcDbHandle h;
        ent->getAcDbHandle(h);
        ACHAR buf[64] = { 0 };
        h.getIntoAsciiBuffer(buf);
        hex = fromAcString(buf);
        ent->close();
    }

    ProjectData& project = ProjectContext::instance().project();
    ElectricalElement* elem = nullptr;
    for (auto& e : project.elements)
        if (e->handle == hex) { elem = e.get(); break; }
    if (!elem) {
        acutPrintf(_T("\nThat entity is not an open-electrical component.\n"));
        return;
    }

    // Remember where and how the existing symbol sits.
    Point3 pos = elem->position;
    double rot = 0.0;
    blockRefTransform(id, pos, rot);
    const std::wstring oldBlock = blockNameFor(*elem);

    // Type-specific dialog, applied to a copy so a cancel changes nothing.
    bool accepted = false;
    switch (elem->type) {
        case ElementType::LightPoint: {
            auto* l = static_cast<LightPoint*>(elem);
            LightPoint copy = *l;
            if ((accepted = ui::showLightDialog(copy))) *l = copy;
            break;
        }
        case ElementType::Outlet: {
            auto* o = static_cast<Outlet*>(elem);
            Outlet copy = *o;
            if ((accepted = ui::showOutletDialog(copy))) *o = copy;
            break;
        }
        case ElementType::Switch: {
            auto* s = static_cast<Switch*>(elem);
            Switch copy = *s;
            if ((accepted = ui::showSwitchDialog(copy))) *s = copy;
            break;
        }
        case ElementType::Panel: {
            auto* pn = static_cast<Panel*>(elem);
            Panel copy = *pn;
            if ((accepted = ui::showPanelDialog(copy))) *pn = copy;
            break;
        }
    }
    if (!accepted) return;

    // The dialogs return a fresh model; restore the identity fields they don't own.
    elem->handle   = hex;
    elem->position = pos;

    UndoGroup undo(_T("EL-EDIT"));
    const std::wstring newBlock = blockNameFor(*elem);
    const std::vector<SymbolAttr> attrs = attrsFor(*elem);

    if (newBlock == oldBlock) {
        // Same geometry: just refresh the attribute texts in place.
        updateBlockAttributes(id, attrs);
    } else {
        // Geometry changed: recreate the symbol, preserving position + rotation.
        eraseEntity(id);
        elem->handle = insertSymbol(newBlock.c_str(), layerFor(*elem), pos,
                                    scaleFor(*elem), rot, attrs);
    }
    ProjectContext::instance().save();
    acutPrintf(_T("\nComponent updated.\n"));
}

} // namespace commands
} // namespace electrical
