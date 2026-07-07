#include "services/SymbolFactory.h"

#include <cmath>

namespace electrical {

const ACHAR* SymbolFactory::kLight  = _T("EL_LIGHT");
const ACHAR* SymbolFactory::kOutlet = _T("EL_OUTLET");
const ACHAR* SymbolFactory::kSwitch = _T("EL_SWITCH");
const ACHAR* SymbolFactory::kPanel  = _T("EL_PANEL");

namespace {

// Helpers that append primitives to a block record. Geometry stays on layer 0
// with BYBLOCK colour so the inserted reference takes the target layer colour.
void addLine(AcDbBlockTableRecord* btr, double x1, double y1, double x2, double y2) {
    AcDbLine* l = new AcDbLine(AcGePoint3d(x1, y1, 0.0), AcGePoint3d(x2, y2, 0.0));
    l->setColorIndex(0);   // BYBLOCK
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
            double startAng, double endAng) {
    AcDbArc* a = new AcDbArc(AcGePoint3d(cx, cy, 0.0), r, startAng, endAng);
    a->setColorIndex(0);
    btr->appendAcDbEntity(a);
    a->close();
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

    // Dispatch to the matching geometry builder.
    if (wcscmp(name, kLight) == 0)       buildLight(btr);
    else if (wcscmp(name, kOutlet) == 0) buildOutlet(btr);
    else if (wcscmp(name, kSwitch) == 0) buildSwitch(btr);
    else if (wcscmp(name, kPanel) == 0)  buildPanel(btr);

    btr->close();
    bt->close();
    return id;
}

bool SymbolFactory::ensure(AcDbDatabase* db, const ACHAR* blockName) {
    if (!db || !blockName) return false;
    if (blockExists(db, blockName)) return true;
    // Only create if it is one of ours.
    if (wcscmp(blockName, kLight)  != 0 && wcscmp(blockName, kOutlet) != 0 &&
        wcscmp(blockName, kSwitch) != 0 && wcscmp(blockName, kPanel)  != 0)
        return false;
    return !createBlock(db, blockName).isNull();
}

void SymbolFactory::ensureAll(AcDbDatabase* db) {
    ensure(db, kLight);
    ensure(db, kOutlet);
    ensure(db, kSwitch);
    ensure(db, kPanel);
}

// ---------------------------------------------------------------------------
//  Symbol geometry (NBR 5444 conventions)
// ---------------------------------------------------------------------------

// Ceiling lighting point: a circle with an inscribed cross (X).
void SymbolFactory::buildLight(AcDbBlockTableRecord* btr) {
    const double r = 0.12;
    addCircle(btr, 0.0, 0.0, r);
    const double d = r * std::cos(0.7853981633974483);  // r/sqrt(2)
    addLine(btr, -d, -d,  d,  d);
    addLine(btr, -d,  d,  d, -d);
}

// Socket outlet: a filled-looking half-disc on a base line with two contacts.
// Drawn as a semicircle (arc) closed by its diameter, plus two prongs and a
// stem toward the wall - a compact, readable outlet glyph.
void SymbolFactory::buildOutlet(AcDbBlockTableRecord* btr) {
    const double r = 0.11;
    // Upper semicircle (0..180 deg) closed by the diameter line.
    addArc(btr, 0.0, 0.0, r, 0.0, 3.141592653589793);
    addLine(btr, -r, 0.0, r, 0.0);
    // Two contact prongs above the disc.
    addLine(btr, -0.04, r, -0.04, r + 0.06);
    addLine(btr,  0.04, r,  0.04, r + 0.06);
    // Stem down to the wall reference point.
    addLine(btr, 0.0, 0.0, 0.0, -0.10);
}

// Wall switch: a small circle with an angled lever line (the key).
void SymbolFactory::buildSwitch(AcDbBlockTableRecord* btr) {
    const double r = 0.06;
    addCircle(btr, 0.0, 0.0, r);
    // Lever at 45 degrees out of the circle.
    const double a = 0.7853981633974483;
    addLine(btr, r * std::cos(a), r * std::sin(a), 0.22 * std::cos(a), 0.22 * std::sin(a));
    // Short base tick marking the switch position on the wall.
    addLine(btr, 0.0, -r, 0.0, -r - 0.05);
}

// Load panel / distribution board: a rectangle with a diagonal, labelled by the
// plugin's tag text. Wider than tall to read as a board.
void SymbolFactory::buildPanel(AcDbBlockTableRecord* btr) {
    const double w = 0.50, h = 0.24;
    const double x0 = -w / 2, y0 = -h / 2, x1 = w / 2, y1 = h / 2;
    addLine(btr, x0, y0, x1, y0);
    addLine(btr, x1, y0, x1, y1);
    addLine(btr, x1, y1, x0, y1);
    addLine(btr, x0, y1, x0, y0);
    // Diagonal hatch line (half) to distinguish a board from a plain rectangle.
    addLine(btr, x0, y0, x0 + w * 0.5, y1);
}

} // namespace electrical
