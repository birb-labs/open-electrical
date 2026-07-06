# FindBRX.cmake
# ---------------------------------------------------------------------------
# Locates the BricsCAD Runtime eXtension (BRX) SDK and exposes it as the
# imported target BRX::BRX.
#
# The BRX SDK is NOT redistributable and must be obtained from Bricsys after
# registering as an ARX/BRX/TX developer. The SDK major version MUST match the
# target BricsCAD major version (BRX SDK V26 <-> BricsCAD V26).
#
# Configure with:  -DBRX_SDK_ROOT=<path to extracted SDK>
# or set the environment variable BRX_SDK_ROOT.
#
# Result variables / targets:
#   BRX_FOUND        - TRUE when the SDK was located
#   BRX_SDK_ROOT     - resolved SDK root
#   BRX_INCLUDE_DIRS - header directories
#   BRX_LIBRARIES    - import/link libraries
#   BRX::BRX         - imported interface target (link this)
# ---------------------------------------------------------------------------

if(NOT BRX_SDK_ROOT)
    if(DEFINED ENV{BRX_SDK_ROOT})
        set(BRX_SDK_ROOT "$ENV{BRX_SDK_ROOT}")
    endif()
endif()

find_path(BRX_INCLUDE_DIR
    NAMES arxHeaders.h dbmain.h
    HINTS "${BRX_SDK_ROOT}"
    PATH_SUFFIXES inc include
    DOC "BRX SDK include directory")

# Windows ships import libraries (.lib); Linux ships shared objects (.so).
if(WIN32)
    set(_brx_lib_names acdb26 acge26 acrx26 acedapi rxapi acdb acge acrx)
    set(_brx_lib_suffixes lib lib/x64 lib-x64)
else()
    set(_brx_lib_names brxbase acdb acge acrx acad)
    set(_brx_lib_suffixes lib lib/linux exe)
endif()

set(BRX_LIBRARIES "")
foreach(_name ${_brx_lib_names})
    find_library(BRX_LIB_${_name}
        NAMES ${_name} lib${_name}
        HINTS "${BRX_SDK_ROOT}"
        PATH_SUFFIXES ${_brx_lib_suffixes}
        DOC "BRX library ${_name}")
    if(BRX_LIB_${_name})
        list(APPEND BRX_LIBRARIES "${BRX_LIB_${_name}}")
    endif()
    mark_as_advanced(BRX_LIB_${_name})
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BRX
    REQUIRED_VARS BRX_INCLUDE_DIR
    FAIL_MESSAGE
        "BRX SDK not found. Set -DBRX_SDK_ROOT=<path to BRX SDK V26>. \
The SDK is obtained from Bricsys after registering as an ARX/BRX/TX developer.")

if(BRX_FOUND)
    # On Linux the BRX SDK provides a "Windows Platform Emulation" layer that
    # supplies <windows.h> and the non-GUI MFC classes (CString, CArray, ...)
    # needed by the SDK headers.  We must add the Platform substitutes dir to
    # the include path so that #include <windows.h> resolves there instead of
    # the (non-existent) system header.
    set(BRX_INCLUDE_DIRS "${BRX_INCLUDE_DIR}")
    if(NOT WIN32)
        set(_brx_platform_subst "${BRX_INCLUDE_DIR}/Platform/substitutes")
        if(IS_DIRECTORY "${_brx_platform_subst}")
            list(APPEND BRX_INCLUDE_DIRS "${_brx_platform_subst}")
            message(STATUS "BRX Platform substitutes: ${_brx_platform_subst}")
        endif()
    endif()

    if(NOT TARGET BRX::BRX)
        add_library(BRX::BRX INTERFACE IMPORTED)
        set_target_properties(BRX::BRX PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${BRX_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${BRX_LIBRARIES}")
    endif()
endif()

mark_as_advanced(BRX_INCLUDE_DIR)
