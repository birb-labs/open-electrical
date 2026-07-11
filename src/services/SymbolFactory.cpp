#include "services/SymbolFactory.h"

#include <cmath>
#include <cwchar>
#include <vector>

namespace electrical {

const ACHAR* SymbolFactory::kLight              = _T("EL_LIGHT");
const ACHAR* SymbolFactory::kLightWall          = _T("EL_LIGHT_WALL");
const ACHAR* SymbolFactory::kOutletLow          = _T("EL_OUTLET_L");
const ACHAR* SymbolFactory::kOutletMed          = _T("EL_OUTLET_M");
const ACHAR* SymbolFactory::kOutletHigh         = _T("EL_OUTLET_H");
const ACHAR* SymbolFactory::kOutletSwitch2      = _T("EL_OUTLET_SW2");
const ACHAR* SymbolFactory::kOutletDuplexSwitch1 = _T("EL_OUTLET_DUP_SW1");
const ACHAR* SymbolFactory::kSwitch1            = _T("EL_SWITCH_1");
const ACHAR* SymbolFactory::kSwitch2            = _T("EL_SWITCH_2");
const ACHAR* SymbolFactory::kSwitch3            = _T("EL_SWITCH_3");
const ACHAR* SymbolFactory::kSwitch3Way         = _T("EL_SWITCH_3WAY");
const ACHAR* SymbolFactory::kSwitch4Way         = _T("EL_SWITCH_4WAY");
const ACHAR* SymbolFactory::kSwitchBell         = _T("EL_SWITCH_BELL");
const ACHAR* SymbolFactory::kPanelSurface       = _T("EL_PANEL_SURFACE");
const ACHAR* SymbolFactory::kPanelFlush         = _T("EL_PANEL_FLUSH");

// Bump on any geometry change. v11 = chained outlet modules keep their apex
// (the junction with the next module / switch tie) SHARP so modules stay
// connected; only a free top tip is rounded.
// v10 = outlet medium/high fill now follows the
// rounded outline (fan-filled boundary loop) instead of sharp solid triangles.
// v9 = outlet triangle tips slightly rounded (tangent corner fillets,
// kOutletFilletR) instead of sharp vertices.
// v8 = panel widened to a 3:1 rectangle (was 2:1)
// and the light/panel notation attributes are now centre-aligned in their sector
// (setHorizontalMode(kTextCenter)+setAlignmentPoint) so labels stay centred
// regardless of text length; v7 = symbol sizes normalised to a single base
// unit (kSymUnit): a single outlet's triangle side now equals the switch circle
// diameter, and the panel is 2*diam wide x 1*diam tall (was oversized); v6 =
// switch COMANDO<n> moved OUTSIDE the circle (beside each sector, per NBR 5444) +
// combo outlet+switch attribute tags renamed POT/CIRC -> POTENCIA/CIRCUITO;
// v5 = POTENCIA/COMANDO/CIRCUITO tags, power moved inside the light circle,
// per-section COMANDO<n> on switches; v4 = first block attributes; v3 = filled
// panel triangle; v2 = NBR 5444 combo outlet+switch fix.
const int SymbolFactory::kSymbolVersion = 11;

namespace {

constexpr double kPi = 3.14159265358979323846;

// ---- shared dimensions (drawing units) -------------------------------------
// One base unit drives every symbol's size. The switch circle radius IS the base
// unit, so the switch diameter is 2*kSymUnit; everything else is derived from it
// to keep the symbols in proportion (NBR 5444 line symbology, not to scale).
constexpr double kSymUnit  = 0.09;        // base unit = switch circle radius
constexpr double kSwR      = kSymUnit;    // switch circle radius (diam = 2*unit)
constexpr double kLightR   = 1.6 * kSymUnit;   // ceiling light circle (larger, ~0.144)
constexpr double kWallGap  = 0.5 * kSymUnit;   // gap between the wall (y=0) and the
                                               // nearest pole of a wall-mounted symbol
// Outlet triangle: half base-width chosen so the (equilateral) side == the switch
// diameter (2*kSwR), i.e. an outlet is the same size as a switch. Half base =
// side/2 = kSwR.
constexpr double kOutletBW = kSwR;
constexpr double kBellDotR = 0.28 * kSymUnit;  // bell push-button centre dot

// ---- primitive appenders (layer 0, BYBLOCK colour) ------------------------
void addLine(AcDbBlockTableRecord* btr, double x1, double y1, double x2, double y2) {
    AcDbLine* l = new AcDbLine(AcGePoint3d(x1, y1, 0.0), AcGePoint3d(x2, y2, 0.0));
    l->setColorIndex(0);
    btr->appendAcDbEntity(l);
    l->close();
}

void addCircle(AcDbBlockTableRecord* btr, double cx, double cy, double r) {
    AcDbCircle* c = new AcDbCircle(AcGePoint3d(cx, cy, 0.0), AcGeVector3d::kZAxis, r);
    c->setColorIndex(0);
    btr->appendAcDbEntity(c);
    c->close();
}

void addArc(AcDbBlockTableRecord* btr, double cx, double cy, double r,
            double a0, double a1) {
    AcDbArc* a = new AcDbArc(AcGePoint3d(cx, cy, 0.0), r, a0, a1);
    a->setColorIndex(0);
    btr->appendAcDbEntity(a);
    a->close();
}

// A single filled triangle via AcDbSolid (p3 == p2 collapses the quad).
void addSolidTri(AcDbBlockTableRecord* btr,
                 const AcGePoint3d& p0, const AcGePoint3d& p1, const AcGePoint3d& p2) {
    AcDbSolid* s = new AcDbSolid(p0, p1, p2, p2);
    s->setColorIndex(0);
    btr->appendAcDbEntity(s);
    s->close();
}

// Solid-fills a (sector of a) disc by fanning triangles from the centre. Avoids
// AcDbHatch (heavier / more error-prone) while giving a filled appearance.
void addSolidFan(AcDbBlockTableRecord* btr, double cx, double cy, double r,
                 double a0, double a1, int segs = 24) {
    const AcGePoint3d c(cx, cy, 0.0);
    for (int i = 0; i < segs; ++i) {
        const double t0 = a0 + (a1 - a0) * i / segs;
        const double t1 = a0 + (a1 - a0) * (i + 1) / segs;
        addSolidTri(btr, c,
                    AcGePoint3d(cx + r * std::cos(t0), cy + r * std::sin(t0), 0.0),
                    AcGePoint3d(cx + r * std::cos(t1), cy + r * std::sin(t1), 0.0));
    }
}

// Appends a left-justified, editable (non-constant) block attribute definition.
// Text height is a fraction of the symbol size; colour 0 = BYBLOCK.
void addAttDef(AcDbBlockTableRecord* btr, const ACHAR* tag, const ACHAR* prompt,
               double x, double y, double h) {
    AcDbAttributeDefinition* ad = new AcDbAttributeDefinition();
    ad->setPosition(AcGePoint3d(x, y, 0.0));
    ad->setHeight(h);
    ad->setTag(tag);
    ad->setPrompt(prompt);
    ad->setTextString(_T(""));   // empty default value
    ad->setInvisible(false);
    ad->setConstant(false);
    ad->setColorIndex(0);
    btr->appendAcDbEntity(ad);
    ad->close();
}

// Like addAttDef but centres the text on (cx, cy) both horizontally and
// vertically (middle). NBR 5444 notation sits centred inside its reserved sector;
// centre-alignment keeps a short label ("a") and a long one ("2x32W") both centred
// on the same anchor instead of drifting with their length. When the alignment is
// non-default the alignment point (not the position) drives placement, so we set
// both to the same point.
void addAttDefCentered(AcDbBlockTableRecord* btr, const ACHAR* tag, const ACHAR* prompt,
                       double cx, double cy, double h) {
    AcDbAttributeDefinition* ad = new AcDbAttributeDefinition();
    ad->setHeight(h);
    ad->setTag(tag);
    ad->setPrompt(prompt);
    ad->setTextString(_T(""));   // empty default value
    ad->setInvisible(false);
    ad->setConstant(false);
    ad->setColorIndex(0);
    ad->setHorizontalMode(AcDb::kTextCenter);
    ad->setVerticalMode(AcDb::kTextVertMid);
    ad->setPosition(AcGePoint3d(cx, cy, 0.0));
    ad->setAlignmentPoint(AcGePoint3d(cx, cy, 0.0));
    btr->appendAcDbEntity(ad);
    ad->close();
}

// Draws a switch's circle plus its section dividers (no wall tie - callers
// add that separately, since the combined outlet+switch symbols omit it).
// sections: 1 = plain circle, 2 = split by the vertical diameter through the
// bottom pole, 3 = split into three equal 120 degree slices with one boundary
// at the bottom pole (so the wall tie, when present, lines up with a divider).
void addSwitchCircle(AcDbBlockTableRecord* btr, double cx, double cy, double r, int sections) {
    addCircle(btr, cx, cy, r);
    if (sections == 2) {
        addLine(btr, cx, cy - r, cx, cy + r);
    } else if (sections >= 3) {
        for (double a : { 1.5 * kPi, 1.5 * kPi + 2.0 * kPi / 3.0, 1.5 * kPi + 4.0 * kPi / 3.0 })
            addLine(btr, cx, cy, cx + r * std::cos(a), cy + r * std::sin(a));
    }
}

// Outlet triangle corner fillet radius (drawing units). Small relative to the
// side (2*kOutletBW = 0.18) so the tips read as "levemente arredondadas" (NBR
// 5444 still recognises the triangle) rather than visibly clipped.
constexpr double kOutletFilletR = 0.2 * kOutletBW;   // ~0.018

// Per-vertex fillet geometry for a rounded corner: the arc centre + radius, its
// start/end angles, and the two tangent points where the arc meets the adjoining
// edges. r == 0 marks a SHARP corner (no fillet) - used where a module joins the
// next one in a chain, so the junction still meets at a single point.
struct RoundCorner {
    double cx, cy, r, aStart, aEnd;
    AcGePoint3d tIn, tOut;   // tangent toward previous / next vertex (== V when sharp)
};

// Computes the three corner fillets of triangle v0->v1->v2 (traversed CCW) using
// the per-corner radii `r[i]` (0 = leave that corner sharp). Each radius must be
// small enough that its tangent points don't overrun an edge. Shared by the
// outline stroke and the solid fill so both round identically.
void computeRoundCorners(const AcGePoint3d v[3], const double r[3], RoundCorner out[3]) {
    for (int i = 0; i < 3; ++i) {
        const AcGePoint3d& V = v[i];
        const AcGePoint3d& P = v[(i + 2) % 3];   // previous
        const AcGePoint3d& N = v[(i + 1) % 3];   // next
        double dpx = P.x - V.x, dpy = P.y - V.y;
        double dnx = N.x - V.x, dny = N.y - V.y;
        const double lp = std::hypot(dpx, dpy), ln = std::hypot(dnx, dny);
        dpx /= lp; dpy /= lp; dnx /= ln; dny /= ln;
        double half = std::acos(std::max(-1.0, std::min(1.0, dpx * dnx + dpy * dny))) * 0.5;
        const double t = r[i] / std::tan(half);         // tangent distance from vertex
        out[i].r    = r[i];
        out[i].tIn  = AcGePoint3d(V.x + dpx * t, V.y + dpy * t, 0.0);
        out[i].tOut = AcGePoint3d(V.x + dnx * t, V.y + dny * t, 0.0);
        double bx = dpx + dnx, by = dpy + dny;          // angle bisector
        const double bl = std::hypot(bx, by);
        bx /= bl; by /= bl;
        const double d = r[i] / std::sin(half);         // vertex -> arc centre (0 when sharp)
        out[i].cx = V.x + bx * d; out[i].cy = V.y + by * d;
        out[i].aStart = std::atan2(out[i].tIn.y - out[i].cy, out[i].tIn.x - out[i].cx);
        out[i].aEnd   = std::atan2(out[i].tOut.y - out[i].cy, out[i].tOut.x - out[i].cx);
    }
}

// Samples a corner's fillet arc CCW (tIn -> tOut) into `segs` segments, appending
// the points to `pts`. A sharp corner (r == 0) contributes its single vertex.
void appendArcSamples(std::vector<AcGePoint3d>& pts, const RoundCorner& c, int segs) {
    if (c.r <= 1e-12) { pts.emplace_back(c.cx, c.cy, 0.0); return; }
    double delta = c.aEnd - c.aStart;
    while (delta <= 0.0)         delta += 2.0 * kPi;
    while (delta >  2.0 * kPi)   delta -= 2.0 * kPi;
    for (int k = 0; k <= segs; ++k) {
        const double a = c.aStart + delta * k / segs;
        pts.emplace_back(c.cx + c.r * std::cos(a), c.cy + c.r * std::sin(a), 0.0);
    }
}

// Solid-fills a convex boundary polygon by fanning triangles from its centroid.
void fillConvex(AcDbBlockTableRecord* btr, const std::vector<AcGePoint3d>& pts) {
    if (pts.size() < 3) return;
    double sx = 0.0, sy = 0.0;
    for (const auto& p : pts) { sx += p.x; sy += p.y; }
    const AcGePoint3d c(sx / pts.size(), sy / pts.size(), 0.0);
    for (size_t i = 0; i < pts.size(); ++i)
        addSolidTri(btr, c, pts[i], pts[(i + 1) % pts.size()]);
}

// Draws a rounded-corner outline of triangle p0->p1->p2 (traversed CCW) using the
// precomputed corners: each rounded vertex becomes a tangent arc, joined by the
// shortened edges; a sharp corner (r == 0) is left as a plain vertex.
void addRoundedTriangleOutline(AcDbBlockTableRecord* btr, const RoundCorner c[3]) {
    for (int i = 0; i < 3; ++i) {
        // Edge from this vertex's outgoing tangent to the next vertex's incoming one.
        addLine(btr, c[i].tOut.x, c[i].tOut.y, c[(i + 1) % 3].tIn.x, c[(i + 1) % 3].tIn.y);
        // Corner arc (CCW from incoming to outgoing tangent point); none if sharp.
        if (c[i].r > 1e-12)
            addArc(btr, c[i].cx, c[i].cy, c[i].r, c[i].aStart, c[i].aEnd);
    }
}

// Draws one outlet triangle (equilateral, base parallel to the wall) with its
// base centred at (0, yBase), filled per NBR 5444 mounting-height convention:
// 0 = none (low), 1 = right half only (medium), 2 = full (high). The base corners
// are always slightly rounded; the apex (top tip) is rounded only when `roundApex`
// is true - in a stacked chain the apex is the junction with the next module and
// MUST stay sharp so the two modules still meet at a point. Returns the apex y so
// callers can chain the next module's base there.
double addOutletTriangle(AcDbBlockTableRecord* btr, double yBase, int fill, bool roundApex) {
    const double h = kOutletBW * std::sqrt(3.0);
    const AcGePoint3d left(-kOutletBW, yBase, 0.0);
    const AcGePoint3d right(kOutletBW, yBase, 0.0);
    const AcGePoint3d apex(0.0, yBase + h, 0.0);
    // Corner radii (CCW: [0]=left, [1]=right, [2]=apex). Apex sharp at junctions.
    const AcGePoint3d v[3] = { left, right, apex };
    const double rr[3] = { kOutletFilletR, kOutletFilletR, roundApex ? kOutletFilletR : 0.0 };
    RoundCorner c[3];
    computeRoundCorners(v, rr, c);
    addRoundedTriangleOutline(btr, c);

    if (fill == 0) return apex.y;

    // Fill must follow the SAME rounded boundary as the outline, otherwise the
    // solid pokes past the rounded tips. Build the boundary as a point loop
    // (sampling each fillet arc; a sharp apex contributes its single vertex) and
    // fan-fill it.
    constexpr int kArcSegs = 6;
    std::vector<AcGePoint3d> loop;

    if (fill == 2) {
        // High: the whole (rounded) triangle.
        for (int i = 0; i < 3; ++i) appendArcSamples(loop, c[i], kArcSegs);
    } else {
        // Medium: NBR 5444 fills the right half only (bounded by the vertical
        // median x=0). Trace baseMid -> right corner -> right edge -> apex corner
        // (right portion, up to the median) -> back down the median to baseMid.
        const AcGePoint3d baseMid(0.0, yBase, 0.0);
        loop.push_back(baseMid);
        appendArcSamples(loop, c[1], kArcSegs);                    // right corner
        std::vector<AcGePoint3d> apexArc;
        appendArcSamples(apexArc, c[2], kArcSegs);                 // apex corner (or vertex)
        for (const auto& p : apexArc)
            if (p.x >= -1e-9) loop.push_back(p);                   // keep right half
        loop.emplace_back(0.0, c[2].cy + c[2].r, 0.0);             // median-top (apex when sharp)
    }
    fillConvex(btr, loop);
    return apex.y;
}

// Parses a dynamically-named multi-gang outlet block, e.g. "EL_OUTLET_L_M_H"
// -> {0,1,2}. Returns false if `name` doesn't match the pattern.
bool parseOutletChainName(const ACHAR* name, std::vector<int>& fills) {
    static const ACHAR* kPrefix = _T("EL_OUTLET_");
    const size_t plen = std::wcslen(kPrefix);
    if (std::wcsncmp(name, kPrefix, plen) != 0) return false;

    const std::wstring suffix(name + plen);
    if (suffix.empty() || suffix == _T("SW2") || suffix == _T("DUP_SW1")) return false;

    fills.clear();
    size_t start = 0;
    while (start <= suffix.size()) {
        const size_t us = suffix.find(_T('_'), start);
        const std::wstring tok = suffix.substr(start, us == std::wstring::npos ? std::wstring::npos
                                                                                : us - start);
        if      (tok == _T("L")) fills.push_back(0);
        else if (tok == _T("M")) fills.push_back(1);
        else if (tok == _T("H")) fills.push_back(2);
        else return false;
        if (us == std::wstring::npos) break;
        start = us + 1;
    }
    return fills.size() >= 1 && fills.size() <= 3;
}

// Ensures our XData application id is registered in `db` so setXData succeeds.
void ensureRegApp(AcDbDatabase* db) {
    AcDbRegAppTable* rat = nullptr;
    if (!db || db->getRegAppTable(rat, AcDb::kForRead) != Acad::eOk) return;
    if (!rat->has(kXDataAppName)) {
        rat->upgradeOpen();
        AcDbRegAppTableRecord* rec = new AcDbRegAppTableRecord();
        rec->setName(kXDataAppName);
        if (rat->add(rec) != Acad::eOk) delete rec;
        else rec->close();
    }
    rat->close();
}

} // namespace

bool SymbolFactory::blockExists(AcDbDatabase* db, const ACHAR* name) {
    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForRead) != Acad::eOk) return false;
    const bool has = bt->has(name);
    bt->close();
    return has;
}

AcDbObjectId SymbolFactory::createBlock(AcDbDatabase* db, const ACHAR* name) {
    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForWrite) != Acad::eOk)
        return AcDbObjectId::kNull;

    AcDbObjectId id = AcDbObjectId::kNull;
    AcDbBlockTableRecord* btr = new AcDbBlockTableRecord();
    btr->setName(name);
    btr->setOrigin(AcGePoint3d::kOrigin);
    if (bt->add(id, btr) != Acad::eOk) {
        delete btr;
        bt->close();
        return AcDbObjectId::kNull;
    }

    buildGeometry(btr, name);
    writeVersionStamp(btr);

    btr->close();
    bt->close();
    return id;
}

void SymbolFactory::buildGeometry(AcDbBlockTableRecord* btr, const ACHAR* name) {
    std::vector<int> fills;
    if      (wcscmp(name, kLight)              == 0) buildLight(btr);
    else if (wcscmp(name, kLightWall)          == 0) buildLightWall(btr);
    else if (wcscmp(name, kOutletLow)          == 0) buildOutletChain(btr, {0});
    else if (wcscmp(name, kOutletMed)          == 0) buildOutletChain(btr, {1});
    else if (wcscmp(name, kOutletHigh)         == 0) buildOutletChain(btr, {2});
    else if (wcscmp(name, kOutletSwitch2)      == 0) buildOutletWithSwitch(btr, 1, 2);
    else if (wcscmp(name, kOutletDuplexSwitch1) == 0) buildOutletWithSwitch(btr, 2, 1);
    else if (wcscmp(name, kSwitch1)            == 0) buildSwitch(btr, 1, SwitchFill::None, 1);
    else if (wcscmp(name, kSwitch2)            == 0) buildSwitch(btr, 2, SwitchFill::None, 2);
    else if (wcscmp(name, kSwitch3)            == 0) buildSwitch(btr, 3, SwitchFill::None, 3);
    else if (wcscmp(name, kSwitch3Way)         == 0) buildSwitch(btr, 1, SwitchFill::Full, 1);
    else if (wcscmp(name, kSwitch4Way)         == 0) buildSwitch(btr, 2, SwitchFill::Half, 1);
    else if (wcscmp(name, kSwitchBell)         == 0) buildSwitchBell(btr);
    else if (wcscmp(name, kPanelSurface)       == 0) buildPanel(btr, false);
    else if (wcscmp(name, kPanelFlush)         == 0) buildPanel(btr, true);
    else if (parseOutletChainName(name, fills)) buildOutletChain(btr, fills);
}

bool SymbolFactory::isOurBlock(const ACHAR* name) {
    const ACHAR* known[] = { kLight, kLightWall, kOutletLow, kOutletMed, kOutletHigh,
                             kOutletSwitch2, kOutletDuplexSwitch1,
                             kSwitch1, kSwitch2, kSwitch3, kSwitch3Way, kSwitch4Way, kSwitchBell,
                             kPanelSurface, kPanelFlush };
    for (const ACHAR* n : known) if (wcscmp(name, n) == 0) return true;
    std::vector<int> fills;
    return parseOutletChainName(name, fills);
}

void SymbolFactory::writeVersionStamp(AcDbBlockTableRecord* btr) {
    ensureRegApp(btr->database());
    resbuf* xd = acutBuildList(AcDb::kDxfRegAppName, kXDataAppName,
                               AcDb::kDxfXdInteger32, static_cast<long>(kSymbolVersion),
                               RTNONE);
    btr->setXData(xd);
    acutRelRb(xd);
}

int SymbolFactory::readVersionStamp(AcDbBlockTableRecord* btr) {
    resbuf* xd = btr->xData(kXDataAppName);
    int ver = -1;
    for (resbuf* rb = xd; rb; rb = rb->rbnext) {
        if (rb->restype == AcDb::kDxfXdInteger32) { ver = static_cast<int>(rb->resval.rlong); break; }
    }
    if (xd) acutRelRb(xd);
    return ver;
}

int SymbolFactory::blockStampVersion(AcDbDatabase* db, const ACHAR* name) {
    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForRead) != Acad::eOk) return -1;
    AcDbObjectId id;
    if (bt->getAt(name, id) != Acad::eOk) { bt->close(); return -1; }
    bt->close();

    AcDbBlockTableRecord* btr = nullptr;
    if (acdbOpenObject(btr, id, AcDb::kForRead) != Acad::eOk) return -1;
    int ver = readVersionStamp(btr);
    btr->close();
    return ver;
}

// Clears the existing block record's entities and rebuilds them from the
// current geometry. Existing block references update on the next regeneration.
void SymbolFactory::redefineBlock(AcDbDatabase* db, const ACHAR* name) {
    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForRead) != Acad::eOk) return;
    AcDbObjectId id;
    if (bt->getAt(name, id) != Acad::eOk) { bt->close(); return; }
    bt->close();

    AcDbBlockTableRecord* btr = nullptr;
    if (acdbOpenObject(btr, id, AcDb::kForWrite) != Acad::eOk) return;

    AcDbBlockTableRecordIterator* it = nullptr;
    if (btr->newIterator(it) == Acad::eOk && it) {
        for (; !it->done(); it->step()) {
            AcDbEntity* ent = nullptr;
            if (it->getEntity(ent, AcDb::kForWrite) == Acad::eOk) {
                ent->erase();
                ent->close();
            }
        }
        delete it;
    }

    buildGeometry(btr, name);
    writeVersionStamp(btr);
    btr->close();
}

bool SymbolFactory::ensure(AcDbDatabase* db, const ACHAR* blockName) {
    if (!db || !blockName) return false;
    if (blockExists(db, blockName)) {
        // Block already baked into this drawing: refresh it if it was created by
        // an older plugin build (stale or missing geometry-version stamp).
        if (isOurBlock(blockName) && blockStampVersion(db, blockName) != kSymbolVersion)
            redefineBlock(db, blockName);
        return true;
    }
    if (!isOurBlock(blockName)) return false;
    return !createBlock(db, blockName).isNull();
}

void SymbolFactory::ensureAll(AcDbDatabase* db) {
    const ACHAR* all[] = { kLight, kLightWall, kOutletLow, kOutletMed, kOutletHigh,
                           kOutletSwitch2, kOutletDuplexSwitch1,
                           kSwitch1, kSwitch2, kSwitch3, kSwitch3Way, kSwitch4Way, kSwitchBell,
                           kPanelSurface, kPanelFlush };
    for (const ACHAR* n : all) ensure(db, n);
}

std::wstring SymbolFactory::outletChainBlockName(const std::vector<double>& moduleHeightsM) {
    auto bucketChar = [](double heightM) -> const ACHAR* {
        if (heightM <= 0.6) return _T("L");
        if (heightM <  1.5) return _T("M");
        return _T("H");
    };
    if (moduleHeightsM.size() == 1) {
        const ACHAR* c = bucketChar(moduleHeightsM[0]);
        if (wcscmp(c, _T("L")) == 0) return kOutletLow;
        if (wcscmp(c, _T("M")) == 0) return kOutletMed;
        return kOutletHigh;
    }
    std::wstring name = _T("EL_OUTLET_");
    for (size_t i = 0; i < moduleHeightsM.size() && i < 3; ++i) {
        if (i > 0) name += _T("_");
        name += bucketChar(moduleHeightsM[i]);
    }
    return name;
}

// ---------------------------------------------------------------------------
//  Geometry (NBR 5444). All symbols share the convention that +Y points away
//  from the wall (the wall itself is the line y=0).
// ---------------------------------------------------------------------------

// Ceiling lighting point: a circle (bigger than the switch circle) split by a
// horizontal chord through its centre, plus a stem from the centre to the
// south pole - three reserved sectors (top: wattage, bottom-left: controlling
// switch letter, bottom-right: circuit number - left for a future text pass).
void SymbolFactory::buildLight(AcDbBlockTableRecord* btr) {
    addCircle(btr, 0.0, 0.0, kLightR);
    addLine(btr, -kLightR, 0.0, kLightR, 0.0);   // horizontal chord (E-W poles)
    addLine(btr, 0.0, 0.0, 0.0, -kLightR);       // centre -> south pole
    // Notations (order: POTENCIA, COMANDO, CIRCUITO), all INSIDE the circle and
    // centred on their sector: power in the upper sector (above the chord),
    // command in the lower-left sector and circuit number in the lower-right
    // sector (NBR 5444). kLightR ~= 0.144, so the sector centres sit at ~+/-0.07.
    addAttDefCentered(btr, _T("POTENCIA"), _T("Power (e.g. 2x32W)"),  0.000,  0.072, 0.042);
    addAttDefCentered(btr, _T("COMANDO"),  _T("Switch command (a, b, c)"), -0.062, -0.075, 0.050);
    addAttDefCentered(btr, _T("CIRCUITO"), _T("Circuit number (e.g. -3-)"), 0.062, -0.075, 0.050);
}

// Wall luminaire (arandela): a half-disc with its flat side on the wall,
// split into two by a segment from the wall-centre to the apex (its one pole).
void SymbolFactory::buildLightWall(AcDbBlockTableRecord* btr) {
    addArc(btr, 0.0, 0.0, kLightR, 0.0, kPi);    // arc bulging into the room
    addLine(btr, -kLightR, 0.0, kLightR, 0.0);   // flat side, on the wall
    addLine(btr, 0.0, 0.0, 0.0, kLightR);        // wall-centre -> apex
    // Notations (order: COMANDO, CIRCUITO), centred on the disc's vertical axis:
    // command in the upper part of the half-disc, circuit number in the lower part
    // (nearer the wall), no hyphens.
    addAttDefCentered(btr, _T("COMANDO"),  _T("Switch command (a, b, c)"), 0.0, 0.095, 0.05);
    addAttDefCentered(btr, _T("CIRCUITO"), _T("Circuit number"),           0.0, 0.045, 0.05);
}

// Switch (1/2/3 sections, three-way, four-way): a circle offset from the wall
// with a tie from its nearest (bottom) pole back to the wall.
void SymbolFactory::buildSwitch(AcDbBlockTableRecord* btr, int sections, SwitchFill fill,
                                int commandCount) {
    const double yc = kWallGap + kSwR;
    addSwitchCircle(btr, 0.0, yc, kSwR, sections);
    addLine(btr, 0.0, 0.0, 0.0, yc - kSwR);      // wall tie
    if (fill == SwitchFill::Full)
        addSolidFan(btr, 0.0, yc, kSwR, 0.0, 2.0 * kPi);
    else if (fill == SwitchFill::Half)
        addSolidFan(btr, 0.0, yc, kSwR, -0.5 * kPi, 0.5 * kPi);  // right half

    // One editable COMANDO<n> attribute per section, placed OUTSIDE the circle
    // just beyond the rim, on the bisector of the sector it labels (NBR 5444: the
    // command letters sit beside the symbol, never inside it - same convention as
    // the outlet's power/circuit notation). Attribute order is COMANDO1..N.
    const double th   = 0.05;               // command text height
    const double gap  = 0.035;              // clearance from the circle rim
    const double rt   = kSwR + gap;         // text anchor radius
    const double glyph = 0.6 * th;          // approximate width of one glyph

    // addAttDef anchors text at its lower-left corner, so shift the anchor to
    // centre the glyph on the bisector ray's endpoint.
    auto putAt = [&](int n, double angleDeg) {
        const double a  = angleDeg * kPi / 180.0;
        const double cx = rt * std::cos(a);
        const double cy = yc + rt * std::sin(a);
        // Text grows rightwards: nudge left when the label hangs off the left side.
        const double ax = (std::cos(a) < -0.1) ? cx - glyph
                        : (std::cos(a) >  0.1) ? cx
                                               : cx - 0.5 * glyph;
        const std::wstring tag = L"COMANDO" + std::to_wstring(n);
        addAttDef(btr, tag.c_str(), _T("Section command (a, b, c)"), ax, cy - 0.5 * th, th);
    };
    if (commandCount >= 3) {
        // Sector bisectors for the 120-degree slices (dividers at 270/30/150).
        putAt(1, 210.0);   // lower-left sector
        putAt(2,  90.0);   // top sector
        putAt(3, 330.0);   // lower-right sector
    } else if (commandCount == 2) {
        putAt(1, 180.0);   // left half
        putAt(2,   0.0);   // right half
    } else {
        putAt(1, 0.0);     // undivided circle: label to the right
    }
}

// Bell push button: a 1-section switch with a small filled dot at its centre.
void SymbolFactory::buildSwitchBell(AcDbBlockTableRecord* btr) {
    buildSwitch(btr, 1, SwitchFill::None, 1);
    const double yc = kWallGap + kSwR;
    addSolidFan(btr, 0.0, yc, kBellDotR, 0.0, 2.0 * kPi);
}

// Outlet (any of low/medium/high, or a 1-3 module chain): the first module's
// base connects to the wall by a stem; each following module chains onto the
// previous module's apex (no stem of its own).
void SymbolFactory::buildOutletChain(AcDbBlockTableRecord* btr, const std::vector<int>& fills) {
    double yBase = kWallGap;
    addLine(btr, 0.0, 0.0, 0.0, yBase);          // wall stem, first module only
    for (size_t i = 0; i < fills.size(); ++i)
        // Only the LAST module's apex is a free tip (round it); intermediate apexes
        // are junctions with the next module and stay sharp so the chain connects.
        yBase = addOutletTriangle(btr, yBase, fills[i], /*roundApex=*/i + 1 == fills.size());
    // Notations (order: POTENCIA, CIRCUITO): power (VA) and circuit beside the chain.
    addAttDef(btr, _T("POTENCIA"), _T("Power (VA)"),    kOutletBW + 0.03, kWallGap + 0.02, 0.06);
    addAttDef(btr, _T("CIRCUITO"), _T("Circuit number"), kOutletBW + 0.03, kWallGap + 0.12, 0.06);
}

// Outlet + switch combo (always medium height): the usual wall stem + 1-3
// chained, medium-fill outlet triangle(s) - identical to a plain outlet of
// that module count - with the switch circle attached at the LAST triangle's
// tip (apex, away from the wall). The switch has no wall tie of its own: it
// isn't the element touching the wall, the outlet chain is.
void SymbolFactory::buildOutletWithSwitch(AcDbBlockTableRecord* btr, int outletModules,
                                          int switchSections) {
    double yBase = kWallGap;
    addLine(btr, 0.0, 0.0, 0.0, yBase);          // wall stem (first module only)
    for (int i = 0; i < outletModules; ++i)
        // Every apex here is a junction (next module, or the switch tie on the last
        // one), so keep them all sharp - the tie/next base must meet the point.
        yBase = addOutletTriangle(btr, yBase, /*fill=*/1, /*roundApex=*/false);   // medium fill

    const double stemLen = 0.05;
    const double yc = yBase + stemLen + kSwR;
    addLine(btr, 0.0, yBase, 0.0, yc - kSwR);    // short tie: last apex -> switch
    addSwitchCircle(btr, 0.0, yc, kSwR, switchSections);
    // Notations: same tags as the plain outlet chain (POTENCIA, CIRCUITO), which
    // is what attrsFor() emits for every Outlet regardless of kind.
    addAttDef(btr, _T("POTENCIA"), _T("Power (VA)"),    kOutletBW + 0.03, kWallGap + 0.02, 0.06);
    addAttDef(btr, _T("CIRCUITO"), _T("Circuit number"), kOutletBW + 0.03, kWallGap + 0.12, 0.06);
}

// Load panel: a 3:1 rectangle (straight corners) with a right triangle whose
// hypotenuse is the rectangle's secondary diagonal (right angle at the wall
// corner). Surface-mounted sits flush against the wall; flush-mounted has
// ~75% of its height recessed past the wall line.
void SymbolFactory::buildPanel(AcDbBlockTableRecord* btr, bool flush) {
    // 3:1 rectangle sized to the base unit: height = switch diameter (2*kSwR),
    // width = three times that (the QGBT reads much wider than tall). Panels are
    // inserted at scale 1.0 (see scaleFor).
    const double h = 2.0 * kSwR, w = 3.0 * h;
    const double yOff = flush ? -0.75 * h : 0.0;
    const AcGePoint3d bl(-w / 2, yOff, 0.0), br(w / 2, yOff, 0.0);
    const AcGePoint3d tr(w / 2, yOff + h, 0.0), tl(-w / 2, yOff + h, 0.0);
    addLine(btr, bl.x, bl.y, br.x, br.y);   // bottom
    addLine(btr, br.x, br.y, tr.x, tr.y);   // right
    addLine(btr, tr.x, tr.y, tl.x, tl.y);   // top
    addLine(btr, tl.x, tl.y, bl.x, bl.y);   // left
    addLine(btr, tl.x, tl.y, br.x, br.y);   // secondary diagonal (hypotenuse)

    // Solid-filled right triangle (right angle at the wall corner bl, hypotenuse
    // on the secondary diagonal) - marks the distribution/load panel per the
    // user's convention, for both surface and flush variants.
    addSolidTri(btr, bl, br, tl);
    // Notations (order: NOME, CIRCUITOS), centred on the rectangle's vertical axis
    // (x = 0): board id above the box, circuit count below it.
    addAttDefCentered(btr, _T("NOME"),      _T("Panel name"),   0.0, yOff + h + 0.05, 0.08);
    addAttDefCentered(btr, _T("CIRCUITOS"), _T("Circuit count"), 0.0, yOff - 0.10,     0.07);
}

} // namespace electrical
