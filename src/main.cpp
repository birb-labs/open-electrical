// =============================================================================
//  main.cpp - BRX module entry point and command registration.
//
//  Registers all EL- commands manually (their hyphenated names are not valid
//  C++ identifiers), loads the localization packs sitting next to the .brx, and
//  resets the in-memory project when the active document changes.
// =============================================================================
#include "Platform.h"
#include "commands/Commands.h"
#include "services/ProjectContext.h"
#include "ui/Localization.h"
#include "ui/UiBridge.h"
#include "utilities/StringUtil.h"
#include "utilities/Diag.h"

#include <string>
#include <cstdlib>

#if defined(_ELECTRICAL_WIN)
  #include <windows.h>
#elif defined(_ELECTRICAL_LINUX)
  #include <dlfcn.h>
  #include <libgen.h>
  #include <climits>
#endif

using namespace electrical;

namespace {

// Resolve the directory containing this module so we can find ./resources.
std::string moduleDirectory() {
#if defined(_ELECTRICAL_WIN)
    HMODULE hm = nullptr;
    static int anchor = 0;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&anchor), &hm)) {
        wchar_t path[MAX_PATH] = {0};
        GetModuleFileNameW(hm, path, MAX_PATH);
        std::wstring w(path);
        const size_t slash = w.find_last_of(L"\\/");
        if (slash != std::wstring::npos) return wideToUtf8(w.substr(0, slash));
    }
    return ".";
#elif defined(_ELECTRICAL_LINUX)
    Dl_info info;
    static int anchor = 0;
    if (dladdr(reinterpret_cast<void*>(&anchor), &info) && info.dli_fname) {
        std::string p(info.dli_fname);
        const size_t slash = p.find_last_of('/');
        if (slash != std::string::npos) return p.substr(0, slash);
    }
    return ".";
#else
    return ".";
#endif
}

// Fires during dlopen (static-initialisation phase). If oe-init.log contains
// this line but not "kInit:enter", the fault is between load and app init.
struct DiagStatic { DiagStatic() { EL_DIAG_LOG("STATIC-INIT ran (module dlopen'd)"); } };
static DiagStatic g_diagStatic;

// Document reactor: drop the cached project on any document switch so each
// drawing gets its own state loaded lazily, and apply that drawing's saved UI
// language (a document is active here, so touching its database is safe).
class ElDocReactor : public AcApDocManagerReactor {
public:
    void documentActivated(AcApDocument*) override {
        EL_DIAG_LOG("reactor:documentActivated enter");
        auto& ctx = ProjectContext::instance();
        ctx.reload();
        EL_DIAG_LOG("reactor: after reload, before project() load");
        const auto& s = ctx.project().settings;
        EL_DIAG_LOG("reactor: after project() load, before setLanguage");
        Localization::instance().setLanguage(s.uiLanguage);
        EL_DIAG_LOG("reactor: documentActivated done");
    }
    void documentToBeDestroyed(AcApDocument*) override { ProjectContext::instance().reload(); }
};

ElDocReactor* g_docReactor = nullptr;

void registerCommands() {
    size_t n = 0;
    const commands::CommandDef* table = commands::commandTable(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& c = table[i];
        acedRegCmds->addCommand(kCommandGroup, c.globalName, c.localName,
                                c.flags, c.fn);
    }
}

} // namespace

// -----------------------------------------------------------------------------
//  Application object
// -----------------------------------------------------------------------------
class CElectricalApp : public AcRxArxApp {
public:
    CElectricalApp() : AcRxArxApp() {}

    // Required pure-virtual override (BRX/ARX on Linux needs this).
    void RegisterServerComponents() override {}

    virtual AcRx::AppRetCode On_kInitAppMsg(void* pkt) override {
        EL_DIAG_LOG("kInit:enter");
        AcRx::AppRetCode rc = AcRxArxApp::On_kInitAppMsg(pkt);
        EL_DIAG_LOG("kInit: after base On_kInitAppMsg");

        // Allow the module to be unloaded on demand.
        acrxUnlockApplication(pkt);
        acrxRegisterAppMDIAware(pkt);
        EL_DIAG_LOG("kInit: after unlock + MDIAware");

        // Localization: seed English (built-in) then load JSON packs. We do NOT
        // touch the drawing database here - reading project data during module
        // init is fragile (a document may not be fully available yet). The UI
        // language defaults to English and is applied from the saved project on
        // the first command / document activation (see the doc reactor).
        const std::string resDir = moduleDirectory() + "/resources";
        EL_DIAG_LOG("kInit: after moduleDirectory()");
        Localization::instance().loadPacks(resDir);
        EL_DIAG_LOG("kInit: after loadPacks");

        registerCommands();
        EL_DIAG_LOG("kInit: after registerCommands");

        if (acDocManager) {
            g_docReactor = new ElDocReactor();
            acDocManager->addReactor(g_docReactor);
        }
        EL_DIAG_LOG("kInit: after addReactor");

        acutPrintf(_T("\nopen-electrical loaded (%s). Type EL for the palette.\n"),
                   EL_PLATFORM_NAME);
        EL_DIAG_LOG("kInit: DONE (module fully loaded)");
        return rc;
    }

    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pkt) override {
        acedRegCmds->removeGroup(kCommandGroup);
        ui::shutdownWx();

        if (g_docReactor && acDocManager) {
            acDocManager->removeReactor(g_docReactor);
            delete g_docReactor;
            g_docReactor = nullptr;
        }
        return AcRxArxApp::On_kUnloadAppMsg(pkt);
    }
};

IMPLEMENT_ARX_ENTRYPOINT(CElectricalApp)
