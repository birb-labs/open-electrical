# FindBRX.cmake
# ---------------------------------------------------------------------------
# Locates the BricsCAD Runtime eXtension (BRX) SDK *and* the BricsCAD runtime
# libraries, exposing them as the imported target BRX::BRX.
#
# Two roots are involved on Linux/macOS:
#   * BRX_SDK_ROOT   - the extracted BRX SDK (headers + Windows import .libs).
#                      Default: /opt/brx_sdk_v26 (or $ENV{BRX_SDK_ROOT}).
#   * BRICSCAD_ROOT  - the installed BricsCAD program folder, which ships the
#                      Linux runtime .so libraries AND libdrx_entrypoint.a.
#                      Default: /opt/bricsys/bricscad/v26 (or $ENV{BRICSCAD_ROOT}).
#
# On Windows both headers and import libraries come from BRX_SDK_ROOT.
#
# The SDK major version MUST match the BricsCAD major version (V26 <-> V26).
#
# Result:
#   BRX_FOUND, BRX_SDK_ROOT, BRICSCAD_ROOT, BRX_VERSION
#   BRX_INCLUDE_DIRS, BRX_LIBRARIES, BRX_ENTRYPOINT_ARCHIVE
#   BRX::BRX  - link this; it carries include dirs, libs and the
#               whole-archive entry-point link options.
# ---------------------------------------------------------------------------

# ---- resolve roots --------------------------------------------------------
if(NOT BRX_SDK_ROOT AND DEFINED ENV{BRX_SDK_ROOT})
    set(BRX_SDK_ROOT "$ENV{BRX_SDK_ROOT}")
endif()
if(NOT BRX_SDK_ROOT)
    set(BRX_SDK_ROOT "/opt/brx_sdk_v26")
endif()

if(NOT BRICSCAD_ROOT AND DEFINED ENV{BRICSCAD_ROOT})
    set(BRICSCAD_ROOT "$ENV{BRICSCAD_ROOT}")
endif()
if(NOT BRICSCAD_ROOT)
    # Pick the highest-versioned install if present.
    file(GLOB _bc_candidates "/opt/bricsys/bricscad/v*")
    if(_bc_candidates)
        list(SORT _bc_candidates)
        list(GET _bc_candidates -1 BRICSCAD_ROOT)
    else()
        set(BRICSCAD_ROOT "/opt/bricsys/bricscad/v26")
    endif()
endif()

# BricsCAD major version (drives library suffixes: brx26, __BRXTARGET, ...).
if(NOT BRX_VERSION)
    if(BRICSCAD_ROOT MATCHES "v([0-9]+)")
        set(BRX_VERSION "${CMAKE_MATCH_1}")
    else()
        set(BRX_VERSION "26")
    endif()
endif()

# ---- headers --------------------------------------------------------------
find_path(BRX_INCLUDE_DIR
    NAMES arxHeaders.h dbmain.h
    HINTS "${BRX_SDK_ROOT}"
    PATH_SUFFIXES inc include
    DOC "BRX SDK include directory")

# Fake Windows headers ("substitutes") that let Windows-oriented BRX source
# compile on Linux/macOS. Harmless (unused) on Windows.
set(BRX_SUBSTITUTES_DIR "${BRX_INCLUDE_DIR}/Platform/substitutes")

set(BRX_INCLUDE_DIRS "${BRX_INCLUDE_DIR}")
if(NOT WIN32 AND EXISTS "${BRX_SUBSTITUTES_DIR}")
    list(APPEND BRX_INCLUDE_DIRS "${BRX_SUBSTITUTES_DIR}")
endif()

# ---- libraries ------------------------------------------------------------
# NOTE: do NOT pre-initialise BRX_ENTRYPOINT_ARCHIVE - find_file/find_library
# skip the search when the result variable is already set.
set(BRX_LIBRARIES "")

if(WIN32)
    # Windows: everything from the SDK's lib64.
    foreach(_name brx${BRX_VERSION} TD_Alloc TD_Root)
        find_library(BRX_LIB_${_name}
            NAMES ${_name}
            HINTS "${BRX_SDK_ROOT}"
            PATH_SUFFIXES lib64 lib lib/x64
            DOC "BRX library ${_name}")
        if(BRX_LIB_${_name})
            list(APPEND BRX_LIBRARIES "${BRX_LIB_${_name}}")
        endif()
        mark_as_advanced(BRX_LIB_${_name})
    endforeach()
    find_library(BRX_ENTRYPOINT_ARCHIVE
        NAMES drx_entrypoint
        HINTS "${BRX_SDK_ROOT}" PATH_SUFFIXES lib64 lib)
else()
    # Linux/macOS: runtime .so + entry-point archive from the BricsCAD install.
    foreach(_name brx${BRX_VERSION} TD_Alloc TD_Root)
        find_library(BRX_LIB_${_name}
            NAMES ${_name}
            HINTS "${BRICSCAD_ROOT}"
            NO_DEFAULT_PATH
            DOC "BricsCAD runtime library ${_name}")
        if(BRX_LIB_${_name})
            list(APPEND BRX_LIBRARIES "${BRX_LIB_${_name}}")
        endif()
        mark_as_advanced(BRX_LIB_${_name})
    endforeach()
    find_file(BRX_ENTRYPOINT_ARCHIVE
        NAMES libdrx_entrypoint.a
        HINTS "${BRICSCAD_ROOT}"
        PATHS "${BRICSCAD_ROOT}"
        NO_DEFAULT_PATH)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BRX
    REQUIRED_VARS BRX_INCLUDE_DIR BRX_ENTRYPOINT_ARCHIVE
    FAIL_MESSAGE "BRX SDK / BricsCAD runtime not found - set -DBRX_SDK_ROOT and -DBRICSCAD_ROOT (runtime libs ship with BricsCAD, e.g. /opt/bricsys/bricscad/v26)")

if(BRX_FOUND AND NOT TARGET BRX::BRX)
    add_library(BRX::BRX INTERFACE IMPORTED)
    set_target_properties(BRX::BRX PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${BRX_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${BRX_LIBRARIES}")

    # Re-export the ARX entry points (acrxEntryPoint / acrxGetApiVersion) from
    # our module so BricsCAD can find them; the archive must be linked whole.
    if(WIN32)
        set_property(TARGET BRX::BRX APPEND PROPERTY
            INTERFACE_LINK_OPTIONS "/WHOLEARCHIVE:${BRX_ENTRYPOINT_ARCHIVE}")
    else()
        set_property(TARGET BRX::BRX APPEND PROPERTY
            INTERFACE_LINK_OPTIONS
                "-Wl,--whole-archive"
                "${BRX_ENTRYPOINT_ARCHIVE}"
                "-Wl,--no-whole-archive")
    endif()
endif()

mark_as_advanced(BRX_INCLUDE_DIR)
