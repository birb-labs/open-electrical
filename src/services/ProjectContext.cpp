#include "services/ProjectContext.h"
#include "services/DatabaseService.h"

namespace electrical {

ProjectContext& ProjectContext::instance() {
    static ProjectContext ctx;
    return ctx;
}

ProjectData& ProjectContext::project() {
    if (!loaded_) {
        DatabaseService::load(data_);
        loaded_ = true;
        dirty_  = false;
    }
    return data_;
}

void ProjectContext::reload() {
    loaded_ = false;
    dirty_  = false;
    data_.clear();
}

bool ProjectContext::save() {
    const bool ok = DatabaseService::save(project());
    if (ok) dirty_ = false;
    return ok;
}

} // namespace electrical
