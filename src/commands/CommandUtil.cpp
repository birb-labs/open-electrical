#include "commands/CommandUtil.h"
#include "models/ProjectData.h"
#include "models/Room.h"
#include "services/DatabaseService.h"
#include "utilities/GeometryHelper.h"
#include "utilities/StringUtil.h"

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

std::string insertSymbol(const ACHAR* blockName, const ACHAR* layer,
                         const Point3& pos, double scale) {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForRead) != Acad::eOk) return {};

    AcDbObjectId blockId = AcDbObjectId::kNull;
    const bool haveBlock = (bt->getAt(blockName, blockId) == Acad::eOk);
    bt->close();

    AcDbBlockTableRecord* ms = nullptr;
    if (acdbOpenObject(ms, modelSpaceId(), AcDb::kForWrite) != Acad::eOk) return {};

    std::string handle;
    AcDbEntity* created = nullptr;

    if (haveBlock) {
        AcDbBlockReference* br =
            new AcDbBlockReference(toAcGe(pos), blockId);
        br->setScaleFactors(AcGeScale3d(scale));
        br->setLayer(layer);
        created = br;
    } else {
        // Placeholder marker so the workflow proceeds without a symbol library.
        AcDbCircle* c = new AcDbCircle(toAcGe(pos), AcGeVector3d::kZAxis, 0.15 * scale);
        c->setLayer(layer);
        created = c;
    }

    created->setDatabaseDefaults();
    if (ms->appendAcDbEntity(created) == Acad::eOk) {
        handle = handleOf(created);
    }
    created->close();
    ms->close();
    return handle;
}

std::string drawPolyline(const ACHAR* layer, const std::vector<Point3>& pts, bool closed) {
    if (pts.size() < 2) return {};
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

Room* roomAt(ProjectData& project, const Point3& pt) {
    for (auto& r : project.rooms)
        if (geom::pointInPolygon(pt, r.boundary)) return &r;
    return nullptr;
}

} // namespace cmdutil
} // namespace electrical
