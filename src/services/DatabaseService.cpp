#include "services/DatabaseService.h"
#include "utilities/StringUtil.h"

#include <cstdlib>
#include <cstring>

namespace electrical {

namespace {

// DXF group codes used inside our Xrecords.
constexpr short kCodeKey   = 1;    // AcDb::kDxfText           - property name
constexpr short kCodeText  = 300;  // AcDb::kDxfXTextString    - text value
constexpr short kCodeReal  = 40;   // AcDb::kDxfReal           - real value
constexpr short kCodeInt   = 90;   // AcDb::kDxfInt32          - integer value
constexpr short kCodePoint = 10;   // AcDb::kDxfXCoord         - point value

// Duplicate an ACHAR string into malloc'd memory that acutRelRb will free().
ACHAR* dupAcharForRb(const ACHAR* s) {
    const size_t n = s ? ::wcslen(s) : 0;
    ACHAR* p = static_cast<ACHAR*>(std::malloc((n + 1) * sizeof(ACHAR)));
    if (p) { if (n) ::wmemcpy(p, s, n); p[n] = 0; }
    return p;
}

resbuf* newRb(short code) {
    resbuf* rb = acutNewRb(code);
    return rb;
}

} // namespace

// ---------------------------------------------------------------------------
//  PropertyBag  <->  resbuf chain
// ---------------------------------------------------------------------------
resbuf* DatabaseService::bagToResbuf(const PropertyBag& bag) {
    resbuf* head = nullptr;
    resbuf* tail = nullptr;

    auto append = [&](resbuf* rb) {
        if (!head) head = tail = rb;
        else { tail->rbnext = rb; tail = rb; }
    };

    for (const auto& [key, prop] : bag.entries()) {
        // key
        resbuf* rbKey = newRb(kCodeKey);
        rbKey->resval.rstring = dupAcharForRb(toAcString(key).kwszPtr());
        append(rbKey);

        // value
        switch (prop.kind) {
            case Property::Kind::Int: {
                resbuf* rb = newRb(kCodeInt);
                rb->resval.rlong = static_cast<Adesk::Int32>(prop.i);
                append(rb);
                break;
            }
            case Property::Kind::Real: {
                resbuf* rb = newRb(kCodeReal);
                rb->resval.rreal = prop.r;
                append(rb);
                break;
            }
            case Property::Kind::Text: {
                resbuf* rb = newRb(kCodeText);
                rb->resval.rstring = dupAcharForRb(toAcString(prop.s).kwszPtr());
                append(rb);
                break;
            }
            case Property::Kind::Point: {
                resbuf* rb = newRb(kCodePoint);
                rb->resval.rpoint[0] = prop.p.x;
                rb->resval.rpoint[1] = prop.p.y;
                rb->resval.rpoint[2] = prop.p.z;
                append(rb);
                break;
            }
        }
    }
    return head;
}

void DatabaseService::resbufToBag(const resbuf* head, PropertyBag& bag) {
    const resbuf* rb = head;
    while (rb) {
        // Expect a key.
        if (rb->restype != kCodeKey || !rb->resval.rstring) { rb = rb->rbnext; continue; }
        const std::string key = fromAcString(rb->resval.rstring);
        rb = rb->rbnext;
        if (!rb) break;

        switch (rb->restype) {
            case kCodeInt:   bag.putInt(key, rb->resval.rlong); break;
            case kCodeReal:  bag.putReal(key, rb->resval.rreal); break;
            case kCodeText:  bag.putText(key, fromAcString(rb->resval.rstring)); break;
            case kCodePoint: bag.putPoint(key, Point3(rb->resval.rpoint[0],
                                                      rb->resval.rpoint[1],
                                                      rb->resval.rpoint[2])); break;
            default: break;
        }
        rb = rb->rbnext;
    }
}

// ---------------------------------------------------------------------------
//  Dictionary plumbing
// ---------------------------------------------------------------------------
AcDbObjectId DatabaseService::rootDictionary(AcDbDatabase* db, bool createIfMissing) {
    AcDbObjectId rootId = AcDbObjectId::kNull;
    AcDbDictionary* nod = nullptr;
    if (db->getNamedObjectsDictionary(nod, AcDb::kForRead) != Acad::eOk)
        return rootId;

    if (nod->getAt(kRootDictionary, rootId) != Acad::eOk) {
        if (createIfMissing) {
            nod->upgradeOpen();
            AcDbDictionary* root = new AcDbDictionary();
            if (nod->setAt(kRootDictionary, root, rootId) != Acad::eOk) {
                delete root;
                rootId = AcDbObjectId::kNull;
            } else {
                root->close();
            }
        }
    }
    nod->close();
    return rootId;
}

AcDbObjectId DatabaseService::subDictionary(AcDbObjectId parentId, const ACHAR* name, bool create) {
    AcDbObjectId subId = AcDbObjectId::kNull;
    AcDbDictionary* parent = nullptr;
    if (acdbOpenObject(parent, parentId, AcDb::kForRead) != Acad::eOk)
        return subId;

    if (parent->getAt(name, subId) != Acad::eOk && create) {
        parent->upgradeOpen();
        AcDbDictionary* sub = new AcDbDictionary();
        if (parent->setAt(name, sub, subId) != Acad::eOk) {
            delete sub;
            subId = AcDbObjectId::kNull;
        } else {
            sub->close();
        }
    }
    parent->close();
    return subId;
}

bool DatabaseService::writeBag(AcDbObjectId dictId, const ACHAR* key, const PropertyBag& bag) {
    AcDbDictionary* dict = nullptr;
    if (acdbOpenObject(dict, dictId, AcDb::kForWrite) != Acad::eOk)
        return false;

    resbuf* chain = bagToResbuf(bag);
    AcDbXrecord* xrec = new AcDbXrecord();
    xrec->setFromRbChain(*chain);

    AcDbObjectId existing;
    if (dict->getAt(key, existing) == Acad::eOk) {
        // Replace in place.
        AcDbXrecord* old = nullptr;
        if (acdbOpenObject(old, existing, AcDb::kForWrite) == Acad::eOk) {
            old->setFromRbChain(*chain);
            old->close();
            delete xrec;
            xrec = nullptr;
        }
    }
    if (xrec) {
        AcDbObjectId newId;
        if (dict->setAt(key, xrec, newId) != Acad::eOk) {
            delete xrec;
            acutRelRb(chain);
            dict->close();
            return false;
        }
        xrec->close();
    }

    acutRelRb(chain);
    dict->close();
    return true;
}

bool DatabaseService::readBag(AcDbObjectId dictId, const ACHAR* key, PropertyBag& bag) {
    AcDbDictionary* dict = nullptr;
    if (acdbOpenObject(dict, dictId, AcDb::kForRead) != Acad::eOk)
        return false;

    AcDbObjectId xid;
    if (dict->getAt(key, xid) != Acad::eOk) { dict->close(); return false; }

    AcDbXrecord* xrec = nullptr;
    bool ok = false;
    if (acdbOpenObject(xrec, xid, AcDb::kForRead) == Acad::eOk) {
        resbuf* head = nullptr;
        if (xrec->rbChain(&head) == Acad::eOk && head) {
            resbufToBag(head, bag);
            acutRelRb(head);
            ok = true;
        }
        xrec->close();
    }
    dict->close();
    return ok;
}

void DatabaseService::forEachEntry(AcDbObjectId dictId,
                                   const std::function<void(const ACHAR*, AcDbObjectId)>& fn) {
    AcDbDictionary* dict = nullptr;
    if (acdbOpenObject(dict, dictId, AcDb::kForRead) != Acad::eOk)
        return;

    AcDbDictionaryIterator* it = dict->newIterator();
    for (; it && !it->done(); it->next())
        fn(it->name(), it->objectId());
    delete it;
    dict->close();
}

// ---------------------------------------------------------------------------
//  XData (RegApp + entity stamping)
// ---------------------------------------------------------------------------
bool DatabaseService::ensureRegApp(AcDbDatabase* db) {
    AcDbRegAppTable* rat = nullptr;
    if (db->getRegAppTable(rat, AcDb::kForRead) != Acad::eOk)
        return false;

    bool ok = true;
    if (!rat->has(kXDataAppName)) {
        rat->upgradeOpen();
        AcDbRegAppTableRecord* rec = new AcDbRegAppTableRecord();
        rec->setName(kXDataAppName);
        if (rat->add(rec) != Acad::eOk) { delete rec; ok = false; }
        else rec->close();
    }
    rat->close();
    return ok;
}

bool DatabaseService::stampEntityKey(AcDbObjectId entId, const std::string& key) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, entId, AcDb::kForWrite) != Acad::eOk)
        return false;
    ensureRegApp(ent->database());

    resbuf* xd = acutBuildList(AcDb::kDxfRegAppName, kXDataAppName,
                               AcDb::kDxfXdAsciiString, toAcString(key).kwszPtr(),
                               RTNONE);
    ent->setXData(xd);
    acutRelRb(xd);
    ent->close();
    return true;
}

std::string DatabaseService::readEntityKey(AcDbObjectId entId) {
    AcDbEntity* ent = nullptr;
    if (acdbOpenObject(ent, entId, AcDb::kForRead) != Acad::eOk)
        return {};
    std::string result;
    resbuf* xd = ent->xData(kXDataAppName);
    for (resbuf* rb = xd; rb; rb = rb->rbnext) {
        if (rb->restype == AcDb::kDxfXdAsciiString && rb->resval.rstring) {
            result = fromAcString(rb->resval.rstring);
            break;
        }
    }
    if (xd) acutRelRb(xd);
    ent->close();
    return result;
}

// ---------------------------------------------------------------------------
//  Whole-project save / load
// ---------------------------------------------------------------------------
bool DatabaseService::save(const ProjectData& project) {
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    if (!db) return false;
    ensureRegApp(db);

    AcDbObjectId root = rootDictionary(db, true);
    if (root.isNull()) return false;

    // Settings + meta.
    {
        PropertyBag bag; project.settings.serialize(bag);
        writeBag(root, _T("Settings"), bag);
    }
    {
        PropertyBag meta;
        meta.putInt("nextRoomId", project.nextRoomId);
        meta.putInt("nextCircuitId", project.nextCircuitId);
        meta.putInt("nextConduitId", project.nextConduitId);
        writeBag(root, _T("Meta"), meta);
    }

    // Rooms.
    AcDbObjectId roomsDict = subDictionary(root, _T("Rooms"), true);
    for (const auto& r : project.rooms) {
        PropertyBag bag; r.serialize(bag);
        writeBag(roomsDict, toAcString(std::to_string(r.id)).kwszPtr(), bag);
    }

    // Elements (keyed by block-reference handle).
    AcDbObjectId elemsDict = subDictionary(root, _T("Elements"), true);
    for (const auto& e : project.elements) {
        PropertyBag bag; e->serialize(bag);
        const std::string k = e->handle.empty() ? e->tag : e->handle;
        writeBag(elemsDict, toAcString(k).kwszPtr(), bag);
    }

    // Circuits.
    AcDbObjectId circDict = subDictionary(root, _T("Circuits"), true);
    for (const auto& c : project.circuits) {
        PropertyBag bag; c.serialize(bag);
        writeBag(circDict, toAcString(std::to_string(c.id)).kwszPtr(), bag);
    }

    // Conduits.
    AcDbObjectId condDict = subDictionary(root, _T("Conduits"), true);
    for (const auto& c : project.conduits) {
        PropertyBag bag; c.serialize(bag);
        writeBag(condDict, toAcString(std::to_string(c.id)).kwszPtr(), bag);
    }

    return true;
}

bool DatabaseService::load(ProjectData& out) {
    out.clear();
    AcDbDatabase* db = acdbHostApplicationServices()->workingDatabase();
    if (!db) return false;

    AcDbObjectId root = rootDictionary(db, false);
    if (root.isNull()) return true;   // no project yet - not an error

    // Settings + meta.
    { PropertyBag bag; if (readBag(root, _T("Settings"), bag)) out.settings.deserialize(bag); }
    {
        PropertyBag meta;
        if (readBag(root, _T("Meta"), meta)) {
            out.nextRoomId    = static_cast<int>(meta.getInt("nextRoomId", 1));
            out.nextCircuitId = static_cast<int>(meta.getInt("nextCircuitId", 1));
            out.nextConduitId = static_cast<int>(meta.getInt("nextConduitId", 1));
        }
    }

    // Rooms.
    AcDbObjectId roomsDict = subDictionary(root, _T("Rooms"), false);
    if (!roomsDict.isNull()) {
        forEachEntry(roomsDict, [&](const ACHAR*, AcDbObjectId id) {
            AcDbXrecord* x = nullptr;
            if (acdbOpenObject(x, id, AcDb::kForRead) == Acad::eOk) {
                resbuf* head = nullptr;
                if (x->rbChain(&head) == Acad::eOk && head) {
                    PropertyBag bag; resbufToBag(head, bag);
                    Room r; r.deserialize(bag); out.rooms.push_back(std::move(r));
                    acutRelRb(head);
                }
                x->close();
            }
        });
    }

    // Elements (polymorphic - rebuilt via the type tag).
    AcDbObjectId elemsDict = subDictionary(root, _T("Elements"), false);
    if (!elemsDict.isNull()) {
        forEachEntry(elemsDict, [&](const ACHAR*, AcDbObjectId id) {
            AcDbXrecord* x = nullptr;
            if (acdbOpenObject(x, id, AcDb::kForRead) == Acad::eOk) {
                resbuf* head = nullptr;
                if (x->rbChain(&head) == Acad::eOk && head) {
                    PropertyBag bag; resbufToBag(head, bag);
                    auto elem = ProjectData::makeElement(bag.getText("type"));
                    if (elem) { elem->deserialize(bag); out.elements.push_back(std::move(elem)); }
                    acutRelRb(head);
                }
                x->close();
            }
        });
    }

    // Circuits.
    AcDbObjectId circDict = subDictionary(root, _T("Circuits"), false);
    if (!circDict.isNull()) {
        forEachEntry(circDict, [&](const ACHAR*, AcDbObjectId id) {
            AcDbXrecord* x = nullptr;
            if (acdbOpenObject(x, id, AcDb::kForRead) == Acad::eOk) {
                resbuf* head = nullptr;
                if (x->rbChain(&head) == Acad::eOk && head) {
                    PropertyBag bag; resbufToBag(head, bag);
                    Circuit c; c.deserialize(bag); out.circuits.push_back(std::move(c));
                    acutRelRb(head);
                }
                x->close();
            }
        });
    }

    // Conduits.
    AcDbObjectId condDict = subDictionary(root, _T("Conduits"), false);
    if (!condDict.isNull()) {
        forEachEntry(condDict, [&](const ACHAR*, AcDbObjectId id) {
            AcDbXrecord* x = nullptr;
            if (acdbOpenObject(x, id, AcDb::kForRead) == Acad::eOk) {
                resbuf* head = nullptr;
                if (x->rbChain(&head) == Acad::eOk && head) {
                    PropertyBag bag; resbufToBag(head, bag);
                    Conduit c; c.deserialize(bag); out.conduits.push_back(std::move(c));
                    acutRelRb(head);
                }
                x->close();
            }
        });
    }

    return true;
}

} // namespace electrical
