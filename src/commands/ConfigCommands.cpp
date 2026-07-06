// =============================================================================
//  ConfigCommands.cpp - EL (palette) and EL-CONFIG (project settings).
// =============================================================================
#include "commands/Commands.h"
#include "services/ProjectContext.h"
#include "ui/UiBridge.h"
#include "ui/Localization.h"
#include "utilities/StringUtil.h"

namespace electrical {
namespace commands {

void showPalette() {
    // EL toggles the dockable main palette (wxWidgets).
    ui::togglePalette();
}

void configProject() {
    // Load the current settings, show the modal wxWidgets dialog, persist.
    ProjectData& project = ProjectContext::instance().project();
    ProjectSettings edited = project.settings;

    if (ui::showConfigDialog(edited)) {
        project.settings = edited;
        // A language change takes effect immediately across the UI.
        Localization::instance().setLanguage(edited.uiLanguage);
        ProjectContext::instance().save();
        acutPrintf(_T("\n%s\n"), EL_TRW("msg.settingsSaved").c_str());
        ui::refreshPalette();
    }
}

} // namespace commands
} // namespace electrical
