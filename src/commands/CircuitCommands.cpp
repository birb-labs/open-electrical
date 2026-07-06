// =============================================================================
//  CircuitCommands.cpp - Circuit distribution, conduit routing, wire pulling.
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "services/ProjectContext.h"
#include "services/CircuitDistributor.h"
#include "services/Router.h"
#include "ui/Localization.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <algorithm>
#include <cwchar>

namespace electrical {
namespace commands {

using namespace cmdutil;

// ---- Circuits --------------------------------------------------------------
void distributeCircuitsAuto() {
    ProjectData& project = ProjectContext::instance().project();
    UndoGroup undo(_T("EL-CIRCUIT-AUTO"));

    DistributionResult r = CircuitDistributor::distribute(project);
    ProjectContext::instance().save();
    acutPrintf(_T("\n%d circuit(s) created, %d element(s) assigned.\n"),
               r.circuitsCreated, r.elementsAssigned);
}

void distributeCircuits() {
    // Manual mode: for the foundation this drives the same distributor but lets
    // the user confirm the VA ceilings first. A full manual UI (drag elements
    // between circuits) is layered on top of this in the palette.
    acutPrintf(_T("\nManual circuit distribution. Using standard NBR 5410 ceilings.\n"));
    distributeCircuitsAuto();
}

// ---- Conduits --------------------------------------------------------------
void routeConduitAuto() {
    ProjectData& project = ProjectContext::instance().project();
    if (project.circuits.empty()) {
        acutPrintf(_T("\nNo circuits yet. Run EL-CIRCUIT-AUTO first.\n"));
        return;
    }

    // Offer to omit horizontal conduits (vertical drops only).
    ACHAR res[16] = {0};
    acedInitGet(0, _T("Yes No"));
    if (acedGetKword(_T("\nOmit horizontal conduits (vertical drops only)? [Yes/No] <No>: "), res) != RTNORM)
        wcscpy(res, _T("No"));

    RoutingOptions opt;
    opt.omitHorizontal = (wcscmp(res, _T("Yes")) == 0);
    opt.material = project.settings.conduitMaterial;

    // Ceiling reference from the tallest room (drawing units).
    double ceiling = 0.0;
    for (const auto& r : project.rooms)
        ceiling = std::max(ceiling, r.ceilingHeight / toMeters(project.settings.unit));
    opt.ceilingZ = ceiling;

    UndoGroup undo(_T("EL-CONDUIT-AUTO"));
    RoutingResult r = Router::route(project, opt);

    // Draw the conduit polylines.
    for (const auto& c : project.conduits)
        if (c.handle.empty())
            const_cast<Conduit&>(c).handle =
                drawPolyline(kLayerConduit, c.path, false);

    ProjectContext::instance().save();
    acutPrintf(_T("\n%d conduit(s) routed, total %.2f m%s.\n"),
               r.conduitsCreated, r.totalLengthM,
               opt.omitHorizontal ? _T(" (vertical drops only)") : _T(""));
}

void routeConduit() {
    // Manual: pick a polyline path and register it as a conduit run.
    ProjectData& project = ProjectContext::instance().project();
    Point3 a, b;
    if (!pickPoint(L"Conduit start", a)) return;
    if (!pickPoint(L"Conduit end", b)) return;

    UndoGroup undo(_T("EL-CONDUIT"));
    Conduit c;
    c.id = project.allocConduitId();
    c.material = project.settings.conduitMaterial;
    c.path = { a, b };
    c.recomputeLength(project.settings.unit);
    c.handle = drawPolyline(kLayerConduit, c.path, false);
    project.conduits.push_back(std::move(c));
    ProjectContext::instance().save();
    acutPrintf(_T("\nConduit added.\n"));
}

// ---- Wiring ----------------------------------------------------------------
void routeWireAuto() {
    ProjectData& project = ProjectContext::instance().project();
    if (project.conduits.empty()) {
        acutPrintf(_T("\nNo conduits yet. Run EL-CONDUIT-AUTO first.\n"));
        return;
    }
    UndoGroup undo(_T("EL-WIRE-AUTO"));
    Router::pullWires(project);
    ProjectContext::instance().save();

    double totalWire = 0.0;
    for (const auto& c : project.conduits)
        for (const auto& w : c.wires) totalWire += w.lengthM;
    acutPrintf(_T("\nWires pulled. Total conductor length %.2f m.\n"), totalWire);
}

void routeWire() {
    acutPrintf(_T("\nManual wiring: pulling conductors into existing conduits.\n"));
    routeWireAuto();
}

} // namespace commands
} // namespace electrical
