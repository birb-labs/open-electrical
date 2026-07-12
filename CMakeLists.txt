# =============================================================================
#  open-electrical - Low-voltage electrical installation design plugin
#  Cross-platform BRX (C++) plugin for BricsCAD V26 Ultimate (Windows + Linux)
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(open-electrical
    VERSION 0.1.0
    DESCRIPTION "Low-voltage electrical installation design plugin for BricsCAD"
    LANGUAGES CXX)

# -----------------------------------------------------------------------------
#  Global settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# The BRX SDK must match the target BricsCAD major version (V26 <-> BRX SDK V26).
#   BRX_SDK_ROOT  - extracted BRX SDK          (default /opt/brx_sdk_v26)
#   BRICSCAD_ROOT - installed BricsCAD program (default /opt/bricsys/bricscad/v26)
# On Linux the runtime libraries + libdrx_entrypoint.a come from BRICSCAD_ROOT.
find_package(BRX REQUIRED)

# wxWidgets drives the dockable palettes and the complex dialogs.
#
# IMPORTANT (Linux): BricsCAD loads its OWN wxWidgets (3.1). Linking our module
# against the system's *shared* wxWidgets puts two wx runtimes in one process
# and crashes the host on load. We therefore link a PRIVATE, STATIC wxWidgets
# and localise its symbols (see src/exports.map). Build it with
# scripts/build-wxstatic.sh, which produces a static wx-config; point CMake at
# it via -DwxWidgets_CONFIG_EXECUTABLE=<prefix>/bin/wx-config (setup.sh does
# this automatically). If a static prefix is discoverable, prefer it.
if(NOT WIN32 AND NOT wxWidgets_CONFIG_EXECUTABLE)
    file(GLOB _wxstatic_cfgs "$ENV{HOME}/.local/opt/wxstatic-oel-*/bin/wx-config")
    if(_wxstatic_cfgs)
        list(SORT _wxstatic_cfgs)
        list(GET _wxstatic_cfgs -1 wxWidgets_CONFIG_EXECUTABLE)
    endif()
endif()

set(wxWidgets_USE_UNICODE ON)
if(NOT WIN32)
    set(wxWidgets_USE_STATIC ON)
endif()
find_package(wxWidgets REQUIRED COMPONENTS core base adv aui)
include(${wxWidgets_USE_FILE})

if(NOT WIN32 AND wxWidgets_CONFIG_EXECUTABLE MATCHES "wxstatic-oel")
    message(STATUS "  wxWidgets     : PRIVATE STATIC (${wxWidgets_CONFIG_EXECUTABLE})")
elseif(NOT WIN32)
    message(WARNING "Using SHARED wxWidgets on Linux - this will likely CRASH "
                    "BricsCAD on load. Run scripts/build-wxstatic.sh first.")
endif()

# -----------------------------------------------------------------------------
#  Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ELECTRICAL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ELECTRICAL_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_library(open_electrical MODULE
    ${ELECTRICAL_SOURCES}
    ${ELECTRICAL_HEADERS})

target_include_directories(open_electrical PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(open_electrical PRIVATE
    BRX::BRX
    ${wxWidgets_LIBRARIES})

# BRX macros/headers expect these to describe an ARX application targeting
# BricsCAD (mirrors the BRX sample project's build flags).
target_compile_definitions(open_electrical PRIVATE
    _ACRXAPP
    BRX_APP
    __BRXTARGET=${BRX_VERSION}
    _UNICODE UNICODE)

# The plugin binary carries the platform's BRX extension:
#   Windows -> .brx   |   Linux -> .lrx (Linux Runtime eXtension)
# BricsCAD loads either exactly like an ObjectARX .arx.
if(WIN32)
    set(_brx_suffix ".brx")
else()
    set(_brx_suffix ".lrx")
endif()
set_target_properties(open_electrical PROPERTIES
    OUTPUT_NAME "open-electrical"
    PREFIX ""
    SUFFIX "${_brx_suffix}")

# -----------------------------------------------------------------------------
#  Platform-specific configuration
# -----------------------------------------------------------------------------
if(WIN32)
    # V26 requires the v143 toolset + Windows SDK 10.0.19041.0 so the plugin
    # shares the host CRT. Configure with:
    #   -G "Visual Studio 17 2022" -A x64 -T v143
    target_compile_definitions(open_electrical PRIVATE
        _ELECTRICAL_WIN=1
        WIN32_LEAN_AND_MEAN
        _AFXEXT
        _CRT_SECURE_NO_WARNINGS)
    target_compile_options(open_electrical PRIVATE /W3 /MP /EHsc)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/open-electrical.def")
        target_link_options(open_electrical PRIVATE
            "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/open-electrical.def")
    endif()
else()
    # Linux: match the host toolchain (V26 Linux builds against a modern GCC /
    # libstdc++). Do NOT mix Clang objects into a GCC-built host.
    #   _AFXEXT / _FILE_OFFSET_BITS / _LARGE_FILES follow the BRX sample so the
    #   Windows Platform Emulation layer and 64-bit file I/O behave correctly.
    target_compile_definitions(open_electrical PRIVATE
        _ELECTRICAL_LINUX=1
        _AFXEXT
        _FILE_OFFSET_BITS=64
        _LARGE_FILES)
    target_compile_options(open_electrical PRIVATE
        -fPIC -fexceptions -Wall
        -Wno-unknown-pragmas -Wno-attributes)
    # Export ONLY the ARX entry points; localise everything else (crucially the
    # statically-linked wxWidgets) so it cannot collide with BricsCAD's own wx.
    target_link_options(open_electrical PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/exports.map")
endif()

# -----------------------------------------------------------------------------
#  Resources (localization + symbol blocks + DCL)
# -----------------------------------------------------------------------------
add_custom_command(TARGET open_electrical POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:open_electrical>/resources"
    COMMENT "Copying resources (lang, symbols, dcl) next to the module")

# -----------------------------------------------------------------------------
#  Install layout (a .bundle-friendly folder)
# -----------------------------------------------------------------------------
install(TARGETS open_electrical
    RUNTIME DESTINATION open-electrical
    LIBRARY DESTINATION open-electrical)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    DESTINATION open-electrical)

# -----------------------------------------------------------------------------
#  Unit tests (BRX-free core: polygon math, lumen method, Room serialization).
#  These deliberately link NO BRX/wxWidgets - only the platform-independent
#  translation units - so they build and run headless.
#    Build + run:  cmake --build build --target check
# -----------------------------------------------------------------------------
enable_testing()
add_executable(oe_tests EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/Room.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/ProjectData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/ProjectSettings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/ElectricalElement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/LightPoint.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/Outlet.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/Switch.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/Panel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/Circuit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/Conduit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/services/LightingCalculator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/services/CircuitDistributor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/services/ReportGenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utilities/PolyMath.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utilities/ElementOutline.cpp")
target_include_directories(oe_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")
add_test(NAME oe_tests COMMAND oe_tests)

# `check` builds the tests (EXCLUDE_FROM_ALL keeps the plugin build independent)
# and runs them, failing the invocation if any test fails.
add_custom_target(check
    COMMAND oe_tests
    DEPENDS oe_tests
    COMMENT "Running open-electrical unit tests")

message(STATUS "open-electrical configured:")
message(STATUS "  BRX SDK root  : ${BRX_SDK_ROOT}")
message(STATUS "  BricsCAD root : ${BRICSCAD_ROOT}")
message(STATUS "  BRX version   : ${BRX_VERSION}")
message(STATUS "  wxWidgets     : ${wxWidgets_VERSION}")
message(STATUS "  Output        : open-electrical${_brx_suffix}")
