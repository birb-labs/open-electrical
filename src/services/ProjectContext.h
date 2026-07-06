// =============================================================================
//  ProjectContext.h - Process-wide access to the active project.
//
//  Commands and UI operate on a single in-memory ProjectData; it is lazily
//  loaded from the drawing on first use and written back on save(). A document
//  reactor (installed in main.cpp) resets it when the active drawing changes.
// =============================================================================
#pragma once

#include "models/ProjectData.h"

namespace electrical {

class ProjectContext {
public:
    static ProjectContext& instance();

    // Returns the active project, loading it from the drawing if needed.
    ProjectData& project();

    // Forces a reload from the current drawing (e.g. on document switch).
    void reload();

    // Writes the in-memory project back into the drawing.
    bool save();

    // Marks the in-memory project dirty (unsaved changes).
    void markDirty() { dirty_ = true; }
    bool dirty() const { return dirty_; }

private:
    ProjectContext() = default;
    ProjectData data_;
    bool loaded_ = false;
    bool dirty_  = false;
};

} // namespace electrical
