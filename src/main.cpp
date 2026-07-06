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

#include <string>

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

// Document reactor: drop the cached project on any document switch so each
// drawing gets its own state loaded lazily.
class ElDocReactor : public AcApDocManagerReactor {
public:
    void documentActivated(AcApDocument*) override      { ProjectContext::instance().reload(); }
    void documentToBeDestroyed(AcApDocument*) override   { ProjectContext::instance().reload(); }
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

    virtual AcRx::AppRetCode On_kInitAppMsg(void* pkt) override {
        AcRx::AppRetCode rc = AcRxArxApp::On_kInitAppMsg(pkt);

        // Allow the module to be unloaded on demand.
        acrxUnlockApplication(pkt);
        acrxRegisterAppMDIAware(pkt);

        // Localization: seed English (built-in) then load JSON packs.
        Localization::instance().loadPacks(moduleDirectory() + "/resources");
        // Apply the saved UI language of the current drawing, if any.
        Localization::instance().setLanguage(
            ProjectContext::instance().project().settings.uiLanguage);

        registerCommands();

        if (acDocManager) {
            g_docReactor = new ElDocReactor();
            acDocManager->addReactor(g_docReactor);
        }

        acutPrintf(_T("\nBricsCAD.Electrical loaded (%s). Type EL for the palette.\n"),
                   EL_PLATFORM_NAME);
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
