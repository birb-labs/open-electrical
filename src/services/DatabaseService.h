// =============================================================================
//  DatabaseService.h - Reads/writes the whole ProjectData to the .dwg.
//
//  Storage strategy
//  ----------------
//  All project data lives in a named object dictionary tree so it is preserved
//  across save/reload and never collides with other applications:
//
//    NamedObjectsDictionary
//      └─ EL_ELECTRICAL_PROJECT            (root dictionary)
//           ├─ Settings      -> Xrecord
//           ├─ Meta          -> Xrecord (id counters)
//           ├─ Rooms         -> sub-dictionary { "1","2",... -> Xrecord }
//           ├─ Elements      -> sub-dictionary { handle -> Xrecord }
//           ├─ Circuits      -> sub-dictionary { id -> Xrecord }
//           └─ Conduits      -> sub-dictionary { id -> Xrecord }
//
//  Each Xrecord encodes one PropertyBag as a DXF resbuf chain. In addition,
//  every placed element's block reference is stamped with XData (app name
//  EL_ELECTRICAL) carrying its dictionary key, so selecting the block on screen
//  can resolve back to the model object.
// =============================================================================
#pragma once

#include "Platform.h"
#include "models/ProjectData.h"
#include "models/Serialization.h"

#include <functional>

namespace electrical {

class DatabaseService {
public:
    // Persists the entire project into the working database.
    static bool save(const ProjectData& project);

    // Loads the entire project from the working database (clears `out` first).
    // Returns true even if no project exists yet (out is left empty).
    static bool load(ProjectData& out);

    // Ensures the XData application id (EL_ELECTRICAL) exists in the RegApp
    // table. Must be called before attaching XData to any entity.
    static bool ensureRegApp(AcDbDatabase* db);

    // Stamps a single value of XData (the dictionary key) onto an entity.
    static bool stampEntityKey(AcDbObjectId entId, const std::string& key);

    // Reads the stamped key back from an entity (empty if none).
    static std::string readEntityKey(AcDbObjectId entId);

private:
    // --- dictionary plumbing ---
    static AcDbObjectId rootDictionary(AcDbDatabase* db, bool createIfMissing);
    static AcDbObjectId subDictionary(AcDbObjectId parent, const ACHAR* name, bool create);
    static bool writeBag(AcDbObjectId dictId, const ACHAR* key, const PropertyBag& bag);
    static bool readBag(AcDbObjectId dictId, const ACHAR* key, PropertyBag& bag);
    static void forEachEntry(AcDbObjectId dictId,
                             const std::function<void(const ACHAR*, AcDbObjectId)>& fn);

    // --- PropertyBag <-> resbuf ---
    static resbuf* bagToResbuf(const PropertyBag& bag);   // caller frees w/ acutRelRb
    static void    resbufToBag(const resbuf* head, PropertyBag& bag);
};

} // namespace electrical
