// =============================================================================
//  Platform.h - Cross-platform compatibility layer
//
//  Central place for the (few) #ifdef seams between Windows and Linux. The core
//  (models / services) includes this and stays platform-agnostic; only the UI
//  host binding needs to know the difference (BcUiPanelMFC vs wxWindow).
// =============================================================================
#pragma once

// -----------------------------------------------------------------------------
//  BRX / ObjectARX-compatible headers. On Linux/macOS the BRX SDK supplies the
//  "Windows Platform Emulation" layer via brx_platform_linux.h, which MUST be
//  included before any other BRX header so TCHAR / CString / CArray and the
//  Win32 substitutes resolve. On Windows arxHeaders.h is self-sufficient.
// -----------------------------------------------------------------------------
#ifndef _WIN32   // Linux, macOS
  #include "brx_platform_linux.h"
#endif

#include "arxHeaders.h"

#include <string>
#include <cstdint>

// -----------------------------------------------------------------------------
//  Text: BricsCAD uses wide TCHAR (ACHAR) throughout its API. We standardise the
//  plugin on std::wstring for internal string handling and convert at UI edges.
// -----------------------------------------------------------------------------
namespace electrical {

using String = std::wstring;

#if defined(_ELECTRICAL_WIN)
    #define EL_PLATFORM_NAME L"Windows"
#elif defined(_ELECTRICAL_LINUX)
    #define EL_PLATFORM_NAME L"Linux"
#else
    #define EL_PLATFORM_NAME L"Unknown"
#endif

// The plugin's own command group name (used in ACED_ARXCOMMAND_ENTRY_AUTO and
// when tearing commands down on unload). Command *global* names never carry a
// leading underscore (BricsCAD only honours the underscore on the local name).
constexpr const ACHAR* kCommandGroup = _T("ELECTRICAL");

// Named-object-dictionary key under which all project data is persisted in the
// .dwg. See DatabaseService.
constexpr const ACHAR* kRootDictionary = _T("EL_ELECTRICAL_PROJECT");

// XData registered application name (must be registered in the RegApp table
// before attaching XData to entities).
constexpr const ACHAR* kXDataAppName = _T("EL_ELECTRICAL");

} // namespace electrical
