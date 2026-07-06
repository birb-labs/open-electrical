#include "services/ReportGenerator.h"
#include "models/Outlet.h"

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
    bool hasLight = false, hasOutlet = false, hasSwitch = false, hasPanel = false;
    for (const auto& e : p.elements) {
        switch (e->type) {
            case ElementType::LightPoint: hasLight = true; break;
            case ElementType::Outlet:     hasOutlet = true; break;
            case ElementType::Switch:     hasSwitch = true; break;
            case ElementType::Panel:      hasPanel = true; break;
        }
    }
    std::ostringstream os;
    os << "LEGEND\n======\n";
    if (hasLight)  os << "  (X)   Lighting point\n";
    if (hasOutlet) os << "  =O=   Socket outlet (height noted per module)\n";
    if (hasSwitch) os << "  S     Wall switch\n";
    if (hasPanel)  os << "  [QD]  Load panel / distribution board\n";
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
    // Panels and their circuits.
    for (const auto& e : p.elements) {
        if (e->type != ElementType::Panel) continue;
        auto* panel = static_cast<Panel*>(e.get());
        os << "  +-- " << panel->name << (panel->isMain ? " (main)\n" : "\n");
        for (const auto& c : p.circuits) {
            os << "  |     +-- " << c.name << "  "
               << fmt(c.phaseConductorMM2, 1) << "mm2  "
               << fmt(c.breakerAmps, 0) << "A  "
               << fmt(c.connectedLoadVA, 0) << "VA\n";
        }
    }
    if (p.elements.empty())
        os << "  (no panels defined)\n";
    return os.str();
}

} // namespace electrical
