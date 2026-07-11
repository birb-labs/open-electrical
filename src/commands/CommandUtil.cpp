#include "commands/CommandUtil.h"
#include "models/ProjectData.h"
#include "models/Room.h"
#include "services/DatabaseService.h"
#include "services/SymbolFactory.h"
#include "utilities/GeometryHelper.h"
#include "utilities/StringUtil.h"

#include <cmath>
#include <cwchar>

namespace electrical {
namespace cmdutil {

const ACHAR* kLayerLighting = _T("EL_LIGHTING");
const ACHAR* kLayerOutlets  = _T("EL_OUTLETS");
const ACHAR* kLayerSwitches = _T("EL_SWITCHES");
const ACHAR* kLayerPanels   = _T("EL_PANELS");
const ACHAR* kLayerConduit  = _T("EL_CONDUIT");
const ACHAR* kLayerWiring   = _T("EL_WIRING");
const ACHAR* kLayerRooms    = _T("EL_ROOMS");
const ACHAR* kLayerText     = _T("EL_TEXT");

AcDbObjectId ensureLayer(const ACHAR* name, short colorIndex) {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    AcDbLayerTable* lt = nullptr;
    if (db->getLayerTable(lt, AcDb::kForRead) != Acad::eOk)
        return AcDbObjectId::kNull;

    AcDbObjectId id = AcDbObjectId::kNull;
    if (lt->getAt(name, id) == Acad::eOk) { lt->close(); return id; }

    lt->upgradeOpen();
    AcDbLayerTableRecord* rec = new AcDbLayerTableRecord();
    rec->setName(name);
    AcCmColor col; col.setColorIndex(colorIndex);
    rec->setColor(col);
    if (lt->add(id, rec) != Acad::eOk) { delete rec; id = AcDbObjectId::kNull; }
    else rec->close();
    lt->close();
    return id;
}

// Standard colour for each plugin layer (per the project convention): panels red,
// lighting green, outlets cyan, switches magenta, conduit blue, wiring yellow,
// rooms gray, text white. Blue (ACI 5) is reserved exclusively for conduit
// (eletrodutos); text, which used to be blue, is now white (ACI 7) so no other
// layer shares the conduit colour. Ensures the layer exists BEFORE the insertion
// functions call setLayer() - a missing layer name makes setLayer() a silent
// no-op, leaving the entity on layer 0 (the "everything lands on layer 0" bug this
// fixes).
static void ensurePluginLayer(const ACHAR* name) {
    if (!name) return;
    struct LC { const ACHAR* n; short c; };
    static const LC kColors[] = {
        { kLayerPanels,   1 }, { kLayerLighting, 3 }, { kLayerOutlets, 4 },
        { kLayerSwitches, 6 }, { kLayerConduit,  5 }, { kLayerWiring,  2 },
        { kLayerRooms,    8 }, { kLayerText,     7 },
    };
    for (const auto& lc : kColors)
        if (wcscmp(name, lc.n) == 0) { ensureLayer(name, lc.c); return; }
    ensureLayer(name, 7);   // unknown layer name: default to white
}

static AcDbObjectId modelSpaceId() {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* bt = nullptr;
    db->getBlockTable(bt, AcDb::kForRead);
    AcDbObjectId ms;
    bt->getAt(ACDB_MODEL_SPACE, ms);
    bt->close();
    return ms;
}

static std::string handleOf(AcDbEntity* ent) {
    AcDbHandle h; ent->getAcDbHandle(h);
    ACHAR buf[64]; h.getIntoAsciiBuffer(buf);
    return fromAcString(buf);
}

bool pickPoint(const std::wstring& prompt, Point3& out) {
    ads_point p;
    const std::wstring full = L"\n" + prompt + L" ";
    if (acedGetPoint(nullptr, full.c_str(), p) != RTNORM) return false;
    out = Point3(p[X], p[Y], p[Z]);
    return true;
}

bool pickDirection(const std::wstring& prompt, const Point3& from, double& angleOut) {
    ads_point base = { from.x, from.y, from.z };
    ads_point p;
    const std::wstring full = L"\n" + prompt + L" ";
    if (acedGetPoint(base, full.c_str(), p) != RTNORM) return false;
    const double dx = p[X] - from.x, dy = p[Y] - from.y;
    if (std::hypot(dx, dy) < 1e-9) { angleOut = 0.0; return true; }
    // Symbol local +Y points away from the wall (into the room); rotate so +Y
    // aims at the picked point: rotation = atan2(dir) - 90 degrees.
    angleOut = std::atan2(dy, dx) - 1.5707963267948966;
    return true;
}

std::map<std::wstring, std::wstring> attrMap(const std::vector<SymbolAttr>& attribs) {
    std::map<std::wstring, std::wstring> m;
    for (const auto& a : attribs) m[a.tag] = a.value;
    return m;
}

// Looks a tag up in `values`. Returns nullptr when the block defines an attribute
// the caller said nothing about - those keep whatever text they already carry.
static const std::wstring* lookup(const std::map<std::wstring, std::wstring>& values,
                                  const ACHAR* tag) {
    if (!tag) return nullptr;
    const auto it = values.find(std::wstring(tag));
    return (it == values.end()) ? nullptr : &it->second;
}

// Creates AcDbAttribute references on `br` for every non-constant attribute
// definition in its block, filling their text from `attribs` matched BY TAG.
// (tagConst() borrows the string; unlike tag() it transfers no ownership, so the
// old order-matching workaround is unnecessary.) Must be called while `br` is
// open for write and database-resident.
static void appendAttributes(AcDbBlockReference* br, AcDbObjectId blockId,
                             const ACHAR* layer, const std::vector<SymbolAttr>& attribs) {
    AcDbBlockTableRecord* def = nullptr;
    if (acdbOpenObject(def, blockId, AcDb::kForRead) != Acad::eOk) return;
    AcDbBlockTableRecordIterator* it = nullptr;
    if (def->newIterator(it) != Acad::eOk || !it) { def->close(); return; }

    const std::map<std::wstring, std::wstring> values = attrMap(attribs);
    for (; !it->done(); it->step()) {
        AcDbEntity* ent = nullptr;
        if (it->getEntity(ent, AcDb::kForRead) != Acad::eOk) continue;
        AcDbAttributeDefinition* ad = AcDbAttributeDefinition::cast(ent);
        if (ad && !ad->isConstant()) {
            AcDbAttribute* at = new AcDbAttribute();
            at->setPropertiesFrom(ad);
            at->setAttributeFromBlock(ad, br->blockTransform());
            at->setLayer(layer);
            if (const std::wstring* v = lookup(values, ad->tagConst()))
                at->setTextString(v->c_str());
            br->appendAttribute(at);
            at->close();
        }
        ent->close();
    }
    delete it;
    def->close();
}

namespace {
constexpr double kHalfPi = 1.5707963267948966;
}

bool pickEntity(const std::wstring& prompt, AcDbObjectId& out) {
    ads_name en;
    ads_point p;
    const std::wstring full = L"\n" + prompt + L" ";
    if (acedEntSel(full.c_str(), en, p) != RTNORM) return false;
    return acdbGetObjectId(out, en) == Acad::eOk;
}

bool pickRotationOnWall(const Point3& pos, double& angleOut) {
    ads_name en;
    ads_point p;
    if (acedEntSel(_T("\nSelect the wall (line/polyline), or press Enter for a free angle: "),
                   en, p) == RTNORM) {
        AcDbObjectId wallId;
        if (acdbGetObjectId(wallId, en) == Acad::eOk) {
            AcDbEntity* ent = nullptr;
            if (acdbOpenObject(ent, wallId, AcDb::kForRead) == Acad::eOk) {
                AcGeVector3d der(0, 0, 0);
                bool ok = false;
                if (AcDbCurve* cv = AcDbCurve::cast(ent)) {
                    AcGePoint3d cp;
                    double param = 0.0;
                    if (cv->getClosestPointTo(toAcGe(pos), cp) == Acad::eOk &&
                        cv->getParamAtPoint(cp, param) == Acad::eOk &&
                        cv->getFirstDeriv(param, der) == Acad::eOk)
                        ok = der.length() > 1e-9;
                }
                ent->close();
                if (ok) {
                    // Only the two wall normals are admissible (+/-90 to the wall).
                    // The side pick decides which one points into the room.
                    Point3 side;
                    if (!pickPoint(L"Pick a point inside the room (side the symbol faces)", side))
                        return false;
                    const double nx = -der.y, ny = der.x;   // left normal
                    const double dot = nx * (side.x - pos.x) + ny * (side.y - pos.y);
                    const double na = (dot >= 0.0) ? std::atan2(ny, nx)
                                                   : std::atan2(-ny, -nx);
                    angleOut = na - kHalfPi;   // map the symbol's local +Y onto the normal
                    return true;
                }
                acutPrintf(_T("\nThat entity has no usable direction; using a free angle.\n"));
            }
        }
    }
    return pickDirection(L"Pick a point toward the room interior (free angle)", pos, angleOut);
}

bool entityIdByHandle(const std::string& hexHandle, AcDbObjectId& out) {
    if (hexHandle.empty()) return false;
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    AcDbHandle h(toAcString(hexHandle).kwszPtr());
    return db->getAcDbObjectId(out, false, h) == Acad::eOk;
}

bool blockRefTransform(AcDbObjectId brId, Point3& pos, double& rotation) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, brId, AcDb::kForRead) != Acad::eOk) return false;
    bool ok = false;
    if (AcDbBlockReference* br = AcDbBlockReference::cast(ent)) {
        const AcGePoint3d p = br->position();
        pos = Point3(p.x, p.y, p.z);
        rotation = br->rotation();
        ok = true;
    }
    ent->close();
    return ok;
}

// Core: rewrite the attributes of an open block reference, matched by tag.
static bool applyAttributes(AcDbBlockReference* br,
                            const std::map<std::wstring, std::wstring>& values) {
    if (!br) return false;
    AcDbObjectIterator* it = br->attributeIterator();
    for (; it && !it->done(); it->step()) {
        AcDbAttribute* at = nullptr;
        if (acdbOpenObject(at, it->objectId(), AcDb::kForWrite) == Acad::eOk) {
            if (const std::wstring* v = lookup(values, at->tagConst()))
                at->setTextString(v->c_str());
            at->close();
        }
    }
    delete it;
    return true;
}

// Widens a UTF-8 tag->value map into the wide-string map the matching uses.
static std::map<std::wstring, std::wstring>
widenTagValues(const std::map<std::string, std::string>& tagValues) {
    std::map<std::wstring, std::wstring> out;
    for (const auto& [tag, value] : tagValues)
        out[std::wstring(toAcString(tag).kwszPtr())] =
            std::wstring(toAcString(value).kwszPtr());
    return out;
}

bool updateBlockAttributes(AcDbBlockReference* blockRef,
                           const std::map<std::string, std::string>& tagValues) {
    return applyAttributes(blockRef, widenTagValues(tagValues));
}

bool updateBlockAttributes(AcDbObjectId brId,
                           const std::map<std::string, std::string>& tagValues) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, brId, AcDb::kForRead) != Acad::eOk) return false;
    const bool ok = applyAttributes(AcDbBlockReference::cast(ent),
                                    widenTagValues(tagValues));
    ent->close();
    return ok;
}

bool updateBlockAttributes(AcDbObjectId brId, const std::vector<SymbolAttr>& attribs) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, brId, AcDb::kForRead) != Acad::eOk) return false;
    const bool ok = applyAttributes(AcDbBlockReference::cast(ent), attrMap(attribs));
    ent->close();
    return ok;
}

bool eraseEntity(AcDbObjectId id) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, id, AcDb::kForWrite) != Acad::eOk) return false;
    const bool ok = (ent->erase() == Acad::eOk);
    ent->close();
    return ok;
}

std::string insertSymbol(const ACHAR* blockName, const ACHAR* layer,
                         const Point3& pos, double scale,
                         double rotation, const std::vector<SymbolAttr>& attribs) {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();

    // Define the NBR 5444 symbol block on demand (no external symbol library
    // needed); falls back to a placeholder only for unknown block names.
    SymbolFactory::ensure(db, blockName);
    ensurePluginLayer(layer);   // create the target layer so setLayer() takes effect

    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForRead) != Acad::eOk) return {};

    AcDbObjectId blockId = AcDbObjectId::kNull;
    const bool haveBlock = (bt->getAt(blockName, blockId) == Acad::eOk);
    bt->close();

    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    std::string handle;
    if (haveBlock) {
        AcDbBlockReference* br = new AcDbBlockReference(toAcGe(pos), blockId);
        br->setScaleFactors(AcGeScale3d(scale));
        br->setRotation(rotation);
        br->setLayer(layer);
        br->setDatabaseDefaults();
        if (ms->appendAcDbEntity(br) == Acad::eOk) {
            handle = handleOf(br);
            appendAttributes(br, blockId, layer, attribs);
        }
        br->close();
    } else {
        // Placeholder marker so the workflow proceeds without a symbol library.
        AcDbCircle* c = new AcDbCircle(toAcGe(pos), AcGeVector3d::kZAxis, 0.15 * scale);
        c->setLayer(layer);
        c->setDatabaseDefaults();
        if (ms->appendAcDbEntity(c) == Acad::eOk) handle = handleOf(c);
        c->close();
    }
    ms->close();
    return handle;
}

std::string drawPolyline(const ACHAR* layer, const std::vector<Point3>& pts, bool closed) {
    if (pts.size() < 2) return {};
    ensurePluginLayer(layer);
    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    AcDbPolyline* pl = new AcDbPolyline(static_cast<unsigned int>(pts.size()));
    for (size_t i = 0; i < pts.size(); ++i)
        pl->addVertexAt(static_cast<unsigned int>(i), AcGePoint2d(pts[i].x, pts[i].y));
    pl->setClosed(closed);
    pl->setLayer(layer);
    pl->setDatabaseDefaults();

    std::string handle;
    if (ms->appendAcDbEntity(pl) == Acad::eOk) handle = handleOf(pl);
    pl->close();
    ms->close();
    return handle;
}

std::string drawPolyline3D(const ACHAR* layer, const std::vector<Point3>& pts) {
    if (pts.size() < 2) return {};
    ensurePluginLayer(layer);
    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    // AcDb3dPolyline is built empty then fed AcDb3dPolylineVertex children, so it
    // must be database-resident (appended) before vertices are added.
    AcDb3dPolyline* pl = new AcDb3dPolyline();
    pl->setLayer(layer);
    pl->setDatabaseDefaults();

    std::string handle;
    if (ms->appendAcDbEntity(pl) == Acad::eOk) {
        for (const auto& p : pts) {
            AcDb3dPolylineVertex* v = new AcDb3dPolylineVertex(AcGePoint3d(p.x, p.y, p.z));
            pl->appendVertex(v);
            v->close();
        }
        handle = handleOf(pl);
    }
    pl->close();
    ms->close();
    return handle;
}

std::string drawArc2D(const ACHAR* layer, const Point3& a, const Point3& b, double bulge) {
    ensurePluginLayer(layer);
    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    AcDbPolyline* pl = new AcDbPolyline(2);
    pl->addVertexAt(0, AcGePoint2d(a.x, a.y), bulge);   // bulge lives on the first vertex
    pl->addVertexAt(1, AcGePoint2d(b.x, b.y));
    pl->setLayer(layer);
    pl->setDatabaseDefaults();

    std::string handle;
    if (ms->appendAcDbEntity(pl) == Acad::eOk) handle = handleOf(pl);
    pl->close();
    ms->close();
    return handle;
}

std::string drawColoredPolyline(const ACHAR* layer, const std::vector<Point3>& pts,
                                int r, int g, int b, bool closed, double width) {
    if (pts.size() < 2) return {};
    ensurePluginLayer(layer);
    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    AcDbPolyline* pl = new AcDbPolyline(static_cast<unsigned int>(pts.size()));
    for (size_t i = 0; i < pts.size(); ++i)
        pl->addVertexAt(static_cast<unsigned int>(i), AcGePoint2d(pts[i].x, pts[i].y));
    pl->setClosed(closed);
    if (width > 0.0) pl->setConstantWidth(width);   // filled band -> volumetric bar
    pl->setLayer(layer);
    pl->setDatabaseDefaults();
    AcCmColor col;
    col.setRGB(static_cast<Adesk::UInt8>(r), static_cast<Adesk::UInt8>(g), static_cast<Adesk::UInt8>(b));
    pl->setColor(col);

    std::string handle;
    if (ms->appendAcDbEntity(pl) == Acad::eOk) handle = handleOf(pl);
    pl->close();
    ms->close();
    return handle;
}

std::string drawSolidRect(const ACHAR* layer, double cx, double cy,
                          double halfW, double halfH, int r, int g, int b) {
    ensurePluginLayer(layer);
    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    const AcGePoint3d bl(cx - halfW, cy - halfH, 0.0), br(cx + halfW, cy - halfH, 0.0);
    const AcGePoint3d tl(cx - halfW, cy + halfH, 0.0), tr(cx + halfW, cy + halfH, 0.0);
    AcDbSolid* s = new AcDbSolid(bl, br, tl, tr);   // vertex order 0,1,2,3 = quad
    s->setLayer(layer);
    s->setDatabaseDefaults();
    AcCmColor col;
    col.setRGB(static_cast<Adesk::UInt8>(r), static_cast<Adesk::UInt8>(g), static_cast<Adesk::UInt8>(b));
    s->setColor(col);

    std::string handle;
    if (ms->appendAcDbEntity(s) == Acad::eOk) handle = handleOf(s);
    s->close();
    ms->close();
    return handle;
}

void drawLabel(const ACHAR* layer, const Point3& pos, double h,
               const std::wstring& text, int r, int g, int b, bool mask,
               std::vector<std::string>& outHandles) {
    ensurePluginLayer(layer);
    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return;

    // Dark backing quad (drawn first, so the glyphs sit on top) for legibility of
    // a white label over the white rail / a light drawing background.
    if (mask) {
        const size_t nchar = text.empty() ? 1 : text.size();
        const double hw = 0.42 * h * static_cast<double>(nchar);   // half width
        const double hh = 0.72 * h;                                // half height
        const AcGePoint3d bl(pos.x - hw, pos.y - hh, 0.0), br(pos.x + hw, pos.y - hh, 0.0);
        const AcGePoint3d tl(pos.x - hw, pos.y + hh, 0.0), tr(pos.x + hw, pos.y + hh, 0.0);
        AcDbSolid* sol = new AcDbSolid(bl, br, tl, tr);   // vertex order 0,1,2,3 = quad
        sol->setLayer(layer);
        sol->setDatabaseDefaults();
        AcCmColor blk; blk.setRGB(0, 0, 0);
        sol->setColor(blk);
        if (ms->appendAcDbEntity(sol) == Acad::eOk) outHandles.push_back(handleOf(sol));
        sol->close();
    }

    AcDbText* t = new AcDbText();
    t->setTextString(text.c_str());
    t->setHeight(h);
    t->setHorizontalMode(AcDb::kTextMid);        // centre horizontally + vertically
    t->setVerticalMode(AcDb::kTextVertMid);      // on `pos` (alignment point)
    t->setPosition(toAcGe(pos));
    t->setAlignmentPoint(toAcGe(pos));
    t->setLayer(layer);
    t->setDatabaseDefaults();
    AcCmColor col;
    col.setRGB(static_cast<Adesk::UInt8>(r), static_cast<Adesk::UInt8>(g), static_cast<Adesk::UInt8>(b));
    t->setColor(col);
    if (ms->appendAcDbEntity(t) == Acad::eOk) outHandles.push_back(handleOf(t));
    t->close();
    ms->close();
}

bool blockRefName(AcDbObjectId id, std::wstring& out) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, id, AcDb::kForRead) != Acad::eOk) return false;
    bool ok = false;
    if (AcDbBlockReference* br = AcDbBlockReference::cast(ent)) {
        AcDbBlockTableRecord* btr = nullptr;
        if (acdbOpenObject(btr, br->blockTableRecord(), AcDb::kForRead) == Acad::eOk) {
            const ACHAR* name = nullptr;
            if (btr->getName(name) == Acad::eOk && name) { out = name; ok = true; }
            btr->close();
        }
    }
    ent->close();
    return ok;
}

Room* roomAt(ProjectData& project, const Point3& pt) {
    for (auto& r : project.rooms)
        if (geom::pointInPolygon(pt, r.boundary)) return &r;
    return nullptr;
}

} // namespace cmdutil
} // namespace electrical
