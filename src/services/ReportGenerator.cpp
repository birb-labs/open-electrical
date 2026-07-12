#include "services/ReportGenerator.h"
#include "models/LightPoint.h"
#include "models/Outlet.h"
#include "models/Switch.h"
#include "models/Panel.h"

#include <map>
#include <sstream>
#include <iomanip>

namespace electrical {

namespace {

const char* circuitTypeName(CircuitType t) {
    switch (t) {
        case CircuitType::Lighting:        return "Lighting";
        case CircuitType::GeneralOutlets:  return "General outlets";
        case CircuitType::SpecificOutlets: return "Specific outlet";
        case CircuitType::Motor:           return "Motor";
        case CircuitType::Mixed:           return "Mixed";
    }
    return "?";
}

std::string fmt(double v, int prec = 2) {
    std::ostringstream os; os << std::fixed << std::setprecision(prec) << v; return os.str();
}

} // namespace

std::string ReportGenerator::legend(const ProjectData& p) {
    // Detect which specific NBR 5444 symbol variants are actually used, so the
    // legend only lists what appears in this drawing.
    bool light = false, lightWall = false;
    bool outletLow = false, outletMed = false, outletHigh = false;
    bool outletCombo2 = false, outletComboDup1 = false;
    bool sw1 = false, sw2 = false, sw3 = false, sw3way = false, sw4way = false, swBell = false;
    bool panelSurface = false, panelFlush = false;

    for (const auto& e : p.elements) {
        switch (e->type) {
            case ElementType::LightPoint: {
                auto* l = static_cast<LightPoint*>(e.get());
                (l->wallMounted ? lightWall : light) = true;
                break;
            }
            case ElementType::Outlet: {
                auto* o = static_cast<Outlet*>(e.get());
                if (o->kind == OutletKind::WithSwitch2) { outletCombo2 = true; break; }
                if (o->kind == OutletKind::DuplexWithSwitch1) { outletComboDup1 = true; break; }
                for (const auto& m : o->modules) {
                    if      (m.mountingHeight <= 0.6) outletLow  = true;
                    else if (m.mountingHeight <  1.5) outletMed  = true;
                    else                               outletHigh = true;
                }
                break;
            }
            case ElementType::Switch: {
                auto* s = static_cast<Switch*>(e.get());
                switch (s->kind) {
                    case SwitchKind::OneSection:   sw1 = true; break;
                    case SwitchKind::TwoSection:   sw2 = true; break;
                    case SwitchKind::ThreeSection: sw3 = true; break;
                    case SwitchKind::ThreeWay:     sw3way = true; break;
                    case SwitchKind::FourWay:      sw4way = true; break;
                    case SwitchKind::Bell:         swBell = true; break;
                }
                break;
            }
            case ElementType::Panel: {
                auto* pnl = static_cast<Panel*>(e.get());
                (pnl->embedded ? panelFlush : panelSurface) = true;
                break;
            }
        }
    }

    std::ostringstream os;
    os << "LEGEND (ABNT NBR 5444)\n=======================\n";
    if (light)            os << "  Ceiling lighting point - circle w/ horizontal chord + stem to S pole\n";
    if (lightWall)        os << "  Wall lighting point (arandela) - half-disc on the wall\n";
    if (outletLow)        os << "  Outlet, low - triangle, empty\n";
    if (outletMed)        os << "  Outlet, medium - triangle, right half filled\n";
    if (outletHigh)       os << "  Outlet, high - triangle, fully filled\n";
    if (outletCombo2)     os << "  Outlet + 2-section switch (combined, medium height)\n";
    if (outletComboDup1)  os << "  Duplex outlet + 1-section switch (combined, medium height)\n";
    if (sw1)               os << "  Switch, 1 section - plain circle\n";
    if (sw2)               os << "  Switch, 2 sections - circle w/ 1 divider\n";
    if (sw3)               os << "  Switch, 3 sections - circle w/ 3 equal slices\n";
    if (sw3way)             os << "  Three-way (parallel) switch - fully filled circle\n";
    if (sw4way)             os << "  Four-way (intermediate) switch - half-filled circle\n";
    if (swBell)             os << "  Bell push button - circle w/ filled centre dot\n";
    if (panelSurface)      os << "  Load panel, surface (aparente) - 3:1 rectangle + diagonal\n";
    if (panelFlush)        os << "  Load panel, flush (embutido) - as above, ~75% recessed\n";
    os << "  ----  Conduit run\n";
    return os.str();
}

std::string ReportGenerator::loadSchedule(const ProjectData& p) {
    std::ostringstream os;
    os << "LOAD SCHEDULE\n";
    os << "Circuit;Type;Load(VA);Phase;Section(mm2);Breaker(A);V-drop(%)\n";
    for (const auto& c : p.circuits) {
        os << c.name << ';'
           << circuitTypeName(c.type) << ';'
           << fmt(c.connectedLoadVA, 0) << ';'
           << static_cast<char>('A' + c.phaseAssignment) << ';'
           << fmt(c.phaseConductorMM2, 1) << ';'
           << fmt(c.breakerAmps, 0) << ';'
           << fmt(c.voltageDropPct, 2) << '\n';
    }
    double total = 0.0;
    for (const auto& c : p.circuits) total += c.connectedLoadVA;
    os << "TOTAL;;" << fmt(total, 0) << ";;;;\n";
    return os.str();
}

std::vector<BomLine> ReportGenerator::billOfMaterials(const ProjectData& p) {
    std::map<std::string, double> devices;
    for (const auto& e : p.elements) {
        switch (e->type) {
            case ElementType::LightPoint: devices["Lighting point"] += 1; break;
            case ElementType::Switch:     devices["Switch"] += 1; break;
            case ElementType::Panel:      devices["Load panel"] += 1; break;
            case ElementType::Outlet: {
                auto* o = static_cast<Outlet*>(e.get());
                devices["Outlet module"] += static_cast<double>(o->modules.size());
                break;
            }
        }
    }

    std::map<std::string, double> conduitByMat;
    std::map<double, double> wireByGauge;
    for (const auto& c : p.conduits) {
        conduitByMat["Conduit"] += c.lengthM;
        for (const auto& w : c.wires)
            wireByGauge[w.gaugeMM2] += w.lengthM;
    }

    std::vector<BomLine> lines;
    for (auto& [k, v] : devices)      lines.push_back({ k, "un", v });
    for (auto& [k, v] : conduitByMat) lines.push_back({ k, "m", v });
    for (auto& [g, v] : wireByGauge)
        lines.push_back({ "Wire " + fmt(g, 1) + " mm2", "m", v });
    return lines;
}

std::string ReportGenerator::billOfMaterialsText(const ProjectData& p) {
    std::ostringstream os;
    os << "BILL OF MATERIALS\nItem;Unit;Quantity\n";
    for (const auto& l : billOfMaterials(p))
        os << l.item << ';' << l.unit << ';' << fmt(l.quantity, 2) << '\n';
    return os.str();
}

std::string ReportGenerator::singleLineDiagram(const ProjectData& p) {
    std::ostringstream os;
    os << "SINGLE-LINE DIAGRAM (netlist)\n";
    os << "Utility [" << p.settings.utilityProvider << "] "
       << fmt(p.settings.voltageLineToLine, 0) << "/"
       << fmt(p.settings.voltageLineToNeutral, 0) << " V\n";
    // Does any panel own circuit `c`? Used to attribute orphan circuits (panelId
    // matching no panel) to the main/first panel instead of dropping them.
    auto panelExists = [&](int id) {
        for (const auto& e : p.elements)
            if (e->type == ElementType::Panel &&
                static_cast<const Panel*>(e.get())->id == id)
                return true;
        return false;
    };
    // Panels and their circuits (each circuit under the panel it belongs to).
    bool anyPanel = false;
    const Panel* mainPanel = nullptr;
    for (const auto& e : p.elements) {
        if (e->type != ElementType::Panel) continue;
        const auto* pn = static_cast<const Panel*>(e.get());
        if (!mainPanel || pn->isMain) mainPanel = pn;
    }
    for (const auto& e : p.elements) {
        if (e->type != ElementType::Panel) continue;
        auto* panel = static_cast<Panel*>(e.get());
        anyPanel = true;
        os << "  +-- " << panel->name << (panel->isMain ? " (main)\n" : "\n");
        for (const auto& c : p.circuits) {
            const bool owned = (c.panelId == panel->id);
            const bool orphan = (panel == mainPanel) && !panelExists(c.panelId);
            if (!owned && !orphan) continue;
            os << "  |     +-- " << c.name << "  "
               << fmt(c.phaseConductorMM2, 1) << "mm2  "
               << fmt(c.breakerAmps, 0) << "A  "
               << fmt(c.connectedLoadVA, 0) << "VA  "
               << "dV " << fmt(c.voltageDropPct, 2) << "%\n";
        }
    }
    if (!anyPanel)
        os << "  (no panels defined)\n";
    return os.str();
}

} // namespace electrical
