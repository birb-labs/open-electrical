// =============================================================================
//  CircuitCommands.cpp - Circuit distribution, conduit routing, wire pulling.
// =============================================================================
#include "commands/Commands.h"
#include "commands/CommandUtil.h"
#include "models/ElectricalElement.h"
#include "services/ProjectContext.h"
#include "services/CircuitDistributor.h"
#include "services/Router.h"
#include "ui/Localization.h"
#include "ui/UiBridge.h"
#include "utilities/ElementOutline.h"
#include "utilities/StringUtil.h"
#include "utilities/TransactionHelper.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

namespace electrical {
namespace commands {

using namespace cmdutil;

namespace {

// The hex handle string of an entity (as stored in ElectricalElement::handle and
// Conduit::srcHandle/dstHandle). Empty on failure.
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

// Classifies a placed symbol by its block name into the outline kind + outlet
// module count used by the connection rule. Returns false for non-plugin blocks.
bool classifyOutline(const std::wstring& blockName, outline::Kind& kind, int& modules,
                     bool& flushPanel) {
    modules = 1;
    flushPanel = false;
    if (blockName.rfind(L"EL_LIGHT", 0) == 0)  { kind = outline::Kind::Light;  return true; }
    if (blockName.rfind(L"EL_PANEL", 0) == 0)  {
        kind = outline::Kind::Panel;
        flushPanel = (blockName == L"EL_PANEL_FLUSH");
        return true;
    }
    if (blockName.rfind(L"EL_SWITCH", 0) == 0) { kind = outline::Kind::Switch; return true; }
    if (blockName.rfind(L"EL_OUTLET", 0) == 0) {
        kind = outline::Kind::Outlet;
        if      (blockName == L"EL_OUTLET_SW2")     modules = 1;   // outlet+switch combos
        else if (blockName == L"EL_OUTLET_DUP_SW1") modules = 2;
        else {   // EL_OUTLET_L / _M / _H / _L_M / _L_M_H ...: one module per L|M|H token
            const std::wstring suffix = blockName.substr(std::wcslen(L"EL_OUTLET_"));
            modules = 1 + static_cast<int>(std::count(suffix.begin(), suffix.end(), L'_'));
        }
        modules = std::max(1, std::min(3, modules));
        return true;
    }
    return false;
}

// Attach kind + module count + world transform of a picked element, all resolved
// from the drawing. `kind`/`modules` fall back to a point-like Light when the
// block is unknown, so the routine still produces a sensible endpoint.
struct AttachSpec {
    std::string  handle;
    outline::Kind kind = outline::Kind::Light;
    int          modules = 1;
    Point3       pos;
    double       rot = 0.0;
    bool         known = false;
    bool         flushPanel = false;
};

bool resolveAttach(AcDbObjectId id, AttachSpec& out) {
    if (!blockRefTransform(id, out.pos, out.rot)) return false;
    out.handle = handleOfId(id);
    std::wstring bn;
    if (blockRefName(id, bn))
        out.known = classifyOutline(bn, out.kind, out.modules, out.flushPanel);
    return true;
}

// World-space point on `a`'s outline nearest to world point `target` (or just its
// insertion point when the block kind is unknown).
Point3 attachPoint(const AttachSpec& a, const Point3& target) {
    if (!a.known) return Point3(a.pos.x, a.pos.y, 0.0);
    Point3 p = outline::nearestAttachPoint(a.kind, a.modules, a.pos, a.rot, target,
                                           a.flushPanel);
    p.z = 0.0;
    return p;
}

// The mutually-nearest attach points of two elements: each endpoint sits on its
// own outline, at the point closest to the other endpoint (refined so both agree).
void mutualAttachPoints(const AttachSpec& a, const AttachSpec& b, Point3& pA, Point3& pB) {
    pA = attachPoint(a, b.pos);
    pB = attachPoint(b, pA);
    pA = attachPoint(a, pB);
    pB = attachPoint(b, pA);
}

// ---- Wiring-distribution balloon -------------------------------------------

// Midpoint of a poly-path by 2D arc length (Z ignored). Used to anchor the
// balloon near the middle of each conduit.
Point3 pathMidpoint(const std::vector<Point3>& path) {
    if (path.empty())      return Point3();
    if (path.size() == 1)  return Point3(path[0].x, path[0].y, 0.0);
    double total = 0.0;
    for (size_t i = 1; i < path.size(); ++i)
        total += std::hypot(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
    const double half = total * 0.5;
    double acc = 0.0;
    for (size_t i = 1; i < path.size(); ++i) {
        const double seg = std::hypot(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
        if (seg > 1e-9 && acc + seg >= half) {
            const double t = (half - acc) / seg;
            return Point3(path[i - 1].x + t * (path[i].x - path[i - 1].x),
                          path[i - 1].y + t * (path[i].y - path[i - 1].y), 0.0);
        }
        acc += seg;
    }
    return Point3(path.back().x, path.back().y, 0.0);
}

// Draws the wiring-distribution balloon for one conduit: a leaning "C" ring on
// the run, an arm up to a white horizontal rail, and one coloured label per
// conductor carried (F red / N cyan / PE green / R white on a dark mask), per
// NBR 5410. The count is the SUM over the distinct circuits sharing the run
// (Router::conduitConductors). Created entity handles are stored on the conduit
// so a redraw/EL-DEL can remove them. Returns true when a balloon was drawn.
bool drawWiringBalloon(const ProjectData& project, Conduit& c) {
    const ConductorSet cs = Router::conduitConductors(project, c);
    if (cs.total() == 0) return false;
    const Point3 M = pathMidpoint(c.path);
    const double s = c.mirror ? -1.0 : 1.0;   // horizontal mirror (EL-WIRE-FLIP)

    // Metrics (drawing units), scaled to the symbol system (base ~0.09). Small,
    // nearly-closed ring; conductors are drawn as TICK marks crossing the rail
    // (the "Mundo da Eletrica" convention), not letters.
    const double ringR = 0.028;   // small ring on the run
    const double tH    = 0.05;    // conductor bar half-height (= old line half-length)
    const double barW  = 0.02;    // vertical bar width (all equal)
    const double hc    = 0.045;   // horizontal bar half-length (T / L cap)
    const double sp    = 0.13;    // spacing between conductor glyphs
    const double railY = M.y + ringR + 0.085;

    std::vector<std::string>& H = c.annotationHandles;
    const double kPi = 3.14159265358979323846;

    // Ring: a tight "C" (~300 deg) whose ~60 deg opening faces the arm/rail side.
    {
        std::vector<Point3> arc;
        const int n = 16;
        const double gapHalf = 30.0 * kPi / 180.0;
        const double a0 = gapHalf, a1 = 2.0 * kPi - gapHalf;   // CCW, gap toward +x
        for (int i = 0; i <= n; ++i) {
            const double a = a0 + (a1 - a0) * i / n;
            arc.emplace_back(M.x + s * ringR * std::cos(a), M.y + ringR * std::sin(a), 0.0);
        }
        H.push_back(drawColoredPolyline(kLayerWiring, arc, 255, 255, 255));
    }

    const double railX0  = M.x + s * (ringR + 0.03);
    const double railLen = cs.total() * sp + 0.05;
    const double railX1  = railX0 + s * railLen;

    // Arm (ring -> rail) and the white horizontal rail carrying the tick marks.
    H.push_back(drawColoredPolyline(kLayerWiring,
        { Point3(M.x + s * ringR, M.y, 0.0), Point3(railX0, railY, 0.0) }, 255, 255, 255));
    H.push_back(drawColoredPolyline(kLayerWiring,
        { Point3(railX0, railY, 0.0), Point3(railX1, railY, 0.0) }, 255, 255, 255));

    // One volumetric glyph per conductor, built from filled bars (constant-width
    // polylines). Vertical bars share width (barW) and height (2*tH); horizontal
    // bars have thickness barW. Shapes per the user's spec:
    //   Fase    = one vertical bar (full height)
    //   Terra   = vertical bar + centred top bar  -> "T"
    //   Neutro  = vertical bar + top bar to the LEFT -> upside-down "L", leg left
    //   Retorno = only the upper half of the Fase bar
    // Colours per NBR (F red / N cyan / PE green / R white). `s` mirrors x.
    double x = railX0 + s * (sp * 0.5);
    auto vbar = [&](double y0, double y1, int r, int g, int b) {   // vertical filled bar
        H.push_back(drawSolidRect(kLayerWiring, x, 0.5 * (y0 + y1),
                                  0.5 * barW, 0.5 * std::fabs(y1 - y0), r, g, b));
    };
    auto hbar = [&](double x0, double x1, double y, int r, int g, int b) {   // horizontal bar
        H.push_back(drawSolidRect(kLayerWiring, 0.5 * (x0 + x1), y,
                                  0.5 * std::fabs(x1 - x0), 0.5 * barW, r, g, b));
    };
    auto glyph = [&](int r, int g, int b, int kind) {
        switch (kind) {
            case 0:  // Fase: full vertical bar
                vbar(railY - tH, railY + tH, r, g, b);
                break;
            case 1:  // Neutro: full vertical + top bar to the left (inverted L)
                vbar(railY - tH, railY + tH, r, g, b);
                hbar(x, x - s * 2.0 * hc, railY + tH, r, g, b);
                break;
            case 2:  // Retorno: upper half of the Fase bar only
                vbar(railY, railY + tH, r, g, b);
                break;
            case 3:  // Terra: full vertical + centred top bar (T)
                vbar(railY - tH, railY + tH, r, g, b);
                hbar(x - hc, x + hc, railY + tH, r, g, b);
                break;
        }
        x += s * sp;
    };
    for (int i = 0; i < cs.phase;   ++i) glyph(255, 0,   0,   0);
    for (int i = 0; i < cs.neutral; ++i) glyph(0,   255, 255, 1);
    for (int i = 0; i < cs.ground;  ++i) glyph(0,   255, 0,   3);
    for (int i = 0; i < cs.retorno; ++i) glyph(255, 255, 255, 2);

    H.erase(std::remove(H.begin(), H.end(), std::string()), H.end());
    return !H.empty();
}

// Erases any existing balloons and redraws them for every conduit (idempotent).
// Returns the number of conduits that got a balloon.
int refreshWiringBalloons(ProjectData& project) {
    // Erase every previous balloon first (idempotent redraw / no orphans).
    for (auto& c : project.conduits) {
        for (const std::string& h : c.annotationHandles) {
            AcDbObjectId id;
            if (entityIdByHandle(h, id)) eraseEntity(id);
        }
        c.annotationHandles.clear();
    }
    // Runs that overlap at their midpoint (a shared trunk drawn as several
    // coincident conduits) are grouped so ONE balloon there shows the SUM of all
    // circuits travelling through the trunk (e.g. near the panel), per NBR.
    const double eps = 0.02;
    std::vector<char> done(project.conduits.size(), 0);
    int drawn = 0;
    for (size_t i = 0; i < project.conduits.size(); ++i) {
        if (done[i]) continue;
        Conduit& rep = project.conduits[i];
        const Point3 mi = pathMidpoint(rep.path);
        std::vector<int> unionIds = Router::conduitCircuits(project, rep);
        for (size_t j = i + 1; j < project.conduits.size(); ++j) {
            if (done[j]) continue;
            const Point3 mj = pathMidpoint(project.conduits[j].path);
            if (std::hypot(mi.x - mj.x, mi.y - mj.y) > eps) continue;
            done[j] = 1;
            for (int id : Router::conduitCircuits(project, project.conduits[j]))
                if (std::find(unionIds.begin(), unionIds.end(), id) == unionIds.end())
                    unionIds.push_back(id);
        }
        done[i] = 1;
        // Present the union only for this draw so conductorsFor() sums the trunk.
        const std::vector<int> saved = rep.circuitIds;
        rep.circuitIds = unionIds;
        const bool ok = drawWiringBalloon(project, rep);
        rep.circuitIds = saved;
        if (ok) ++drawn;
    }
    return drawn;
}

} // namespace

// ---- Circuits --------------------------------------------------------------
void distributeCircuitsAuto() {
    ProjectData& project = ProjectContext::instance().project();
    UndoGroup undo(_T("EL-CIRCUIT-AUTO"));

    DistributionResult r = CircuitDistributor::distribute(project);

    // The distributor is a pure service: it assigns circuitId but knows nothing
    // about the drawing. Push those numbers into each element's CIRCUITO block
    // attribute here, so placed symbols show the circuit they ended up on
    // (previously they kept whatever was typed at insertion, or "?" for the
    // automatically-placed luminaires).
    int redrawn = 0;
    for (auto& e : project.elements) {
        if (e->circuitId < 0 || e->handle.empty()) continue;
        const std::string label = std::to_string(e->circuitId);
        if (e->circuitLabel == label) continue;   // already showing this circuit
        e->circuitLabel = label;
        syncElementAttributes(*e);
        ++redrawn;
    }

    ProjectContext::instance().save();
    acutPrintf(_T("\n%d circuit(s) created, %d element(s) assigned, ")
               _T("%d CIRCUITO notation(s) updated.\n"),
               r.circuitsCreated, r.elementsAssigned, redrawn);
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

    // Draw the conduit polylines in 3D so vertical drops (ceiling -> device,
    // which differ only in Z) are visible instead of collapsing to a point.
    for (const auto& c : project.conduits)
        if (c.handle.empty())
            const_cast<Conduit&>(c).handle =
                drawPolyline3D(kLayerConduit, c.path);

    ProjectContext::instance().save();
    acutPrintf(_T("\n%d conduit(s) routed, total %.2f m%s.\n"),
               r.conduitsCreated, r.totalLengthM,
               opt.omitHorizontal ? _T(" (vertical drops only)") : _T(""));
}

void routeConduit() {
    // Manual: pick the SOURCE and DESTINATION elements. The conduit runs straight
    // between the two connection points defined by each component's own outline
    // (per-type rule in ElementOutline): it starts on the source's outline at the
    // point nearest the destination, and ends on the destination's outline at the
    // point nearest the source - so it never starts/ends inside a symbol.
    ProjectData& project = ProjectContext::instance().project();

    AcDbObjectId srcId, dstId;
    if (!pickEntity(L"Select the SOURCE element", srcId)) return;
    if (!pickEntity(L"Select the DESTINATION element", dstId)) return;
    if (srcId == dstId) {
        acutPrintf(_T("\nSource and destination are the same element.\n"));
        return;
    }

    AttachSpec src, dst;
    if (!resolveAttach(srcId, src) || !resolveAttach(dstId, dst)) {
        acutPrintf(_T("\nBoth picks must be placed components (block references).\n"));
        return;
    }

    Point3 pA, pB;
    mutualAttachPoints(src, dst, pA, pB);
    const double len = std::hypot(pB.x - pA.x, pB.y - pA.y);
    if (len < 1e-9) {
        acutPrintf(_T("\nThe two elements overlap; no conduit drawn.\n"));
        return;
    }

    UndoGroup undo(_T("EL-CONDUIT"));
    Conduit c;
    c.id = project.allocConduitId();
    c.material = project.settings.conduitMaterial;
    c.srcHandle = src.handle;
    c.dstHandle = dst.handle;
    c.path = { pA, pB };                        // straight segment, outline endpoints
    c.recomputeLength(project.settings.unit);
    // Circuits travelling through a manual run = the circuits of its end devices.
    for (const auto& e : project.elements) {
        if ((e->handle == c.srcHandle || e->handle == c.dstHandle) && e->circuitId >= 0 &&
            std::find(c.circuitIds.begin(), c.circuitIds.end(), e->circuitId) == c.circuitIds.end())
            c.circuitIds.push_back(e->circuitId);
    }
    c.handle = drawPolyline(kLayerConduit, c.path, false);
    project.conduits.push_back(std::move(c));
    ProjectContext::instance().save();
    acutPrintf(_T("\nConduit added between the two elements (%.3f units).\n"), len);
}

// Finds the project conduit backed by drawing entity `id` (matched by handle).
static Conduit* conduitByEntity(ProjectData& project, AcDbObjectId id) {
    const std::string h = handleOfId(id);
    if (h.empty()) return nullptr;
    for (auto& c : project.conduits)
        if (c.handle == h) return &c;
    return nullptr;
}

// EL-CONDUIT-EDIT: reshape an existing manual conduit run. The user picks the run,
// then either moves one endpoint (re-clamped onto that element's outline, so it
// stays within the allowed connection points) or curves the run by giving a point
// its midpoint should pass through. The edit updates the drawing AND the project
// model (path/bulge/length), so it flows straight into the material schedule.
void editConduit() {
    ProjectData& project = ProjectContext::instance().project();

    AcDbObjectId condId;
    if (!pickEntity(L"Select the conduit to edit", condId)) return;
    Conduit* c = conduitByEntity(project, condId);
    if (!c) {
        acutPrintf(_T("\nThat is not an editable conduit (route it with EL-CONDUIT first).\n"));
        return;
    }
    if (c->path.size() != 2) {
        acutPrintf(_T("\nOnly straight element-to-element runs are editable.\n"));
        return;
    }

    ACHAR kw[16] = {0};
    acedInitGet(0, _T("Source Destination Curve"));
    if (acedGetKword(_T("\nEdit [Source endpoint/Destination endpoint/Curve] <Curve>: "), kw)
        != RTNORM)
        wcscpy(kw, _T("Curve"));

    UndoGroup undo(_T("EL-CONDUIT-EDIT"));

    if (wcscmp(kw, _T("Curve")) == 0) {
        Point3 through;
        if (!pickPoint(L"Point the conduit's curve should pass through", through)) return;
        const Point3& a = c->path[0];
        const Point3& b = c->path[1];
        // Bulge from the sagitta of the picked point relative to the chord.
        const double cx = (a.x + b.x) * 0.5, cy = (a.y + b.y) * 0.5;
        const double chord = std::hypot(b.x - a.x, b.y - a.y);
        if (chord < 1e-9) return;
        const double nx = -(b.y - a.y) / chord, ny = (b.x - a.x) / chord;   // unit normal
        const double sag = (through.x - cx) * nx + (through.y - cy) * ny;   // signed offset
        c->bulge = 2.0 * sag / chord;                                       // tan(theta/4)
    } else {
        // Move an endpoint: pick a new spot, snap it to that element's outline.
        const bool source = (wcscmp(kw, _T("Source")) == 0);
        const std::string& elHandle = source ? c->srcHandle : c->dstHandle;
        AcDbObjectId elId;
        AttachSpec spec;
        if (!elHandle.empty() && entityIdByHandle(elHandle, elId) && resolveAttach(elId, spec)) {
            Point3 want;
            if (!pickPoint(L"New endpoint (snaps to the component outline)", want)) return;
            const Point3 snapped = attachPoint(spec, want);
            c->path[source ? 0 : 1] = snapped;
        } else {
            acutPrintf(_T("\nThat endpoint has no linked component; pick a free point.\n"));
            Point3 want;
            if (!pickPoint(L"New endpoint", want)) return;
            want.z = 0.0;
            c->path[source ? 0 : 1] = want;
        }
    }

    // Redraw the entity (erase the old, draw straight or curved) and refresh state.
    eraseEntity(condId);
    c->handle = drawArc2D(kLayerConduit, c->path[0], c->path[1], c->bulge);
    c->recomputeLength(project.settings.unit);
    ProjectContext::instance().save();
    ui::refreshPalette();
    acutPrintf(_T("\nConduit updated (%.3f m).\n"), c->lengthM);
}

// One-shot: route conduits then pull wires, so the BOM is complete in a single
// command (EL-ROUTE-AUTO).
void routeAllAuto() {
    routeConduitAuto();
    ProjectData& project = ProjectContext::instance().project();
    if (project.conduits.empty()) return;
    UndoGroup undo(_T("EL-ROUTE-AUTO"));
    Router::pullWires(project);
    const int drawn = refreshWiringBalloons(project);
    ProjectContext::instance().save();
    double totalWire = 0.0;
    for (const auto& c : project.conduits)
        for (const auto& w : c.wires) totalWire += w.lengthM;
    acutPrintf(_T("\nRouting + wiring complete. %d wiring tag(s), total conductor length %.2f m.\n"),
               drawn, totalWire);
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
    // EL-WIRE now also DRAWS the wiring distribution (the F/N/PE/R balloon inside
    // each conduit), refreshed idempotently on every run.
    const int drawn = refreshWiringBalloons(project);
    ProjectContext::instance().save();

    double totalWire = 0.0;
    for (const auto& c : project.conduits)
        for (const auto& w : c.wires) totalWire += w.lengthM;
    acutPrintf(_T("\nEL-WIRE: %d conduit(s), %d circuit(s), %d wiring tag(s) drawn (%.2f m).\n"),
               static_cast<int>(project.conduits.size()),
               static_cast<int>(project.circuits.size()), drawn, totalWire);
    if (drawn == 0)
        acutPrintf(_T("  No tags drawn: run EL-CIRCUIT-AUTO first so the conduits know "
                      "their circuits (a conduit with no circuit carries no conductors).\n"));
}

void routeWire() {
    acutPrintf(_T("\nManual wiring: pulling conductors into existing conduits.\n"));
    routeWireAuto();
}

// EL-WIRE-FLIP: pick a conduit and mirror its wiring balloon to the other side of
// the run (toggles Conduit::mirror, then redraws just that tag).
void flipWiring() {
    ProjectData& project = ProjectContext::instance().project();
    AcDbObjectId id;
    if (!pickEntity(L"Select the conduit whose wiring tag to mirror", id)) return;
    Conduit* c = conduitByEntity(project, id);
    if (!c) { acutPrintf(_T("\nThat is not a plugin conduit.\n")); return; }

    UndoGroup undo(_T("EL-WIRE-FLIP"));
    c->mirror = !c->mirror;
    for (const std::string& h : c->annotationHandles) {
        AcDbObjectId eid;
        if (entityIdByHandle(h, eid)) eraseEntity(eid);
    }
    c->annotationHandles.clear();
    drawWiringBalloon(project, *c);
    ProjectContext::instance().save();
    acutPrintf(_T("\nWiring tag mirrored.\n"));
}

} // namespace commands
} // namespace electrical
