# =============================================================================
#  BricsCAD.Electrical - Low-voltage electrical installation design plugin
#  Cross-platform BRX (C++) plugin for BricsCAD V26 Ultimate (Windows + Linux)
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(BricsCAD.Electrical
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
# Point BRX_SDK_ROOT at the extracted SDK, e.g.:
#   Windows: -DBRX_SDK_ROOT="C:/BRX_SDK_V26"
#   Linux:   -DBRX_SDK_ROOT="/opt/brx_sdk_v26"
find_package(BRX REQUIRED)

# wxWidgets drives the dockable palettes and the complex dialogs.
# On Linux BricsCAD itself is a wxWidgets application, so we link the same major
# version (3.2.x recommended). On Windows the palette is hosted through the MFC
# bridge but the dialog content is still wxWidgets.
set(wxWidgets_USE_UNICODE ON)
find_package(wxWidgets REQUIRED COMPONENTS core base adv aui)
include(${wxWidgets_USE_FILE})

# -----------------------------------------------------------------------------
#  Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE ELECTRICAL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ELECTRICAL_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_library(BricsCADElectrical MODULE
    ${ELECTRICAL_SOURCES}
    ${ELECTRICAL_HEADERS})

target_include_directories(BricsCADElectrical PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(BricsCADElectrical PRIVATE
    BRX::BRX
    ${wxWidgets_LIBRARIES})

# The plugin binary must carry the .brx extension on every platform. BricsCAD
# loads a .brx exactly like an ObjectARX .arx / a shared library.
set_target_properties(BricsCADElectrical PROPERTIES
    OUTPUT_NAME "BricsCAD.Electrical"
    PREFIX ""
    SUFFIX ".brx")

# -----------------------------------------------------------------------------
#  Platform-specific configuration
# -----------------------------------------------------------------------------
if(WIN32)
    # V26 requires the v143 toolset + Windows SDK 10.0.19041.0 so the plugin
    # shares the host CRT. Configure with:
    #   -G "Visual Studio 17 2022" -A x64 -T v143
    target_compile_definitions(BricsCADElectrical PRIVATE
        _ELECTRICAL_WIN=1
        WIN32_LEAN_AND_MEAN
        _UNICODE UNICODE
        _CRT_SECURE_NO_WARNINGS)
    target_compile_options(BricsCADElectrical PRIVATE /W3 /MP /EHsc)
    # BRX extensions export their entry points through a .def or the ARX macros;
    # the module definition below keeps the exported ordinals stable.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/BricsCADElectrical.def")
        target_link_options(BricsCADElectrical PRIVATE
            "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/BricsCADElectrical.def")
    endif()
else()
    # Linux: match the host toolchain (V26 Linux builds against a modern GCC /
    # libstdc++). Do NOT mix Clang objects into a GCC-built host.
    target_compile_definitions(BricsCADElectrical PRIVATE
        _ELECTRICAL_LINUX=1
        _UNICODE UNICODE)
    target_compile_options(BricsCADElectrical PRIVATE
        -Wall -Wextra -fPIC -fexceptions)
    # The BRX Linux SDK ships a "Windows Platform Emulation" layer that provides
    # the non-GUI MFC classes (CString, CArray, ...) and TCHAR mappings.
    target_compile_definitions(BricsCADElectrical PRIVATE _ADESK_LINUX_=1)
endif()

# -----------------------------------------------------------------------------
#  Resources (localization + symbol blocks + DCL)
# -----------------------------------------------------------------------------
add_custom_command(TARGET BricsCADElectrical POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:BricsCADElectrical>/resources"
    COMMENT "Copying resources (lang, symbols, dcl) next to the .brx")

# -----------------------------------------------------------------------------
#  Install layout (a .bundle-friendly folder)
# -----------------------------------------------------------------------------
install(TARGETS BricsCADElectrical
    RUNTIME DESTINATION BricsCAD.Electrical
    LIBRARY DESTINATION BricsCAD.Electrical)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    DESTINATION BricsCAD.Electrical)

message(STATUS "BricsCAD.Electrical configured:")
message(STATUS "  BRX SDK root : ${BRX_SDK_ROOT}")
message(STATUS "  wxWidgets    : ${wxWidgets_VERSION}")
message(STATUS "  Output       : BricsCAD.Electrical.brx")
