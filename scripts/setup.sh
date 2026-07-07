#!/usr/bin/env bash
# =============================================================================
#  setup.sh - Build & install open-electrical (Linux)
#
#  Compiles the BRX plugin into open-electrical.lrx and copies it (plus its
#  resources) into a per-user folder, then prints how to load it in BricsCAD.
#
#  Prerequisites:
#    * BRX SDK V26      (headers)  - default /opt/brx_sdk_v26
#    * BricsCAD V26     (runtime)  - default /opt/bricsys/bricscad/v26
#    * wxWidgets >= 3.2 development files
#    * CMake >= 3.20, GCC >= 9 (must match the BricsCAD build toolchain)
#
#  Override any path via environment variables, e.g.:
#    BRX_SDK_ROOT=/opt/brx_sdk_v26 BRICSCAD_ROOT=/opt/bricsys/bricscad/v26 ./setup.sh
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

MODULE_NAME="open-electrical.lrx"

# ---------------------------------------------------------------------------
#  Configurable variables (override via environment)
# ---------------------------------------------------------------------------
BRX_SDK_ROOT="${BRX_SDK_ROOT:-/opt/brx_sdk_v26}"
BRICSCAD_ROOT="${BRICSCAD_ROOT:-$(ls -d /opt/bricsys/bricscad/v* 2>/dev/null | sort | tail -1)}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build}"
INSTALL_DIR="${INSTALL_DIR:-${HOME}/.local/share/open-electrical}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL="${PARALLEL:-$(nproc)}"

# ---------------------------------------------------------------------------
#  Pre-flight checks
# ---------------------------------------------------------------------------
fail() { echo "[setup.sh] FAILED: $*" >&2; exit 1; }

command -v cmake >/dev/null 2>&1 || fail "cmake not found (need >= 3.20). \
Install: apt/dnf/pacman/zypper install cmake"

command -v wx-config >/dev/null 2>&1 || fail "wx-config not found - install \
wxWidgets dev files (e.g. apt install libwxgtk3.2-dev / dnf install wxGTK-devel)"

[ -f "${BRX_SDK_ROOT}/inc/arxHeaders.h" ] || fail "BRX SDK headers not at \
BRX_SDK_ROOT='${BRX_SDK_ROOT}'. Set BRX_SDK_ROOT to the extracted BRX SDK V26."

[ -n "${BRICSCAD_ROOT}" ] && [ -f "${BRICSCAD_ROOT}/libdrx_entrypoint.a" ] || \
    fail "BricsCAD runtime not at BRICSCAD_ROOT='${BRICSCAD_ROOT}' \
(libdrx_entrypoint.a missing). Set BRICSCAD_ROOT to the BricsCAD install."

# Private static wxWidgets (required on Linux - see build-wxstatic.sh). Build it
# once if missing; the module MUST NOT link the system's shared wx or BricsCAD
# crashes on load.
WX_VERSION="${WX_VERSION:-3.2.8}"
WXSTATIC_PREFIX="${WXSTATIC_PREFIX:-${HOME}/.local/opt/wxstatic-oel-${WX_VERSION}}"
WX_CONFIG="${WXSTATIC_PREFIX}/bin/wx-config"
if [ ! -x "$WX_CONFIG" ]; then
    echo "[setup.sh] Static wxWidgets not found - building it (one-time, slow) ..."
    WX_VERSION="$WX_VERSION" WXSTATIC_PREFIX="$WXSTATIC_PREFIX" \
        bash "${SCRIPT_DIR}/build-wxstatic.sh"
fi
[ -x "$WX_CONFIG" ] || fail "static wx-config missing at $WX_CONFIG"

# Generator: prefer Ninja if present.
if command -v ninja >/dev/null 2>&1; then
    GEN="Ninja"
else
    GEN="Unix Makefiles"
fi

echo "[setup.sh] Project      : $PROJECT_DIR"
echo "[setup.sh] BRX SDK      : $BRX_SDK_ROOT"
echo "[setup.sh] BricsCAD     : $BRICSCAD_ROOT"
echo "[setup.sh] wx (static)  : $WX_CONFIG"
echo "[setup.sh] Generator    : $GEN"
echo "[setup.sh] Install dir  : $INSTALL_DIR"

# ---------------------------------------------------------------------------
#  Configure + build
# ---------------------------------------------------------------------------
echo "[setup.sh] Configuring ..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G "$GEN" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DBRX_SDK_ROOT="$BRX_SDK_ROOT" \
      -DBRICSCAD_ROOT="$BRICSCAD_ROOT" \
      -DwxWidgets_CONFIG_EXECUTABLE="$WX_CONFIG"

echo "[setup.sh] Building ($PARALLEL jobs) ..."
cmake --build "$BUILD_DIR" --parallel "$PARALLEL"

[ -f "${BUILD_DIR}/${MODULE_NAME}" ] || fail "${MODULE_NAME} not produced."

# ---------------------------------------------------------------------------
#  Install (copy module + resources)
# ---------------------------------------------------------------------------
echo "[setup.sh] Installing into ${INSTALL_DIR}"
mkdir -p "$INSTALL_DIR"
cp -f "${BUILD_DIR}/${MODULE_NAME}" "$INSTALL_DIR/"
[ -d "${BUILD_DIR}/resources" ] && cp -rf "${BUILD_DIR}/resources" "$INSTALL_DIR/"

echo ""
echo "[setup.sh] DONE. Load in BricsCAD with either:"
echo "    APPLOAD  ->  ${INSTALL_DIR}/${MODULE_NAME}"
echo "    or command line:  (arxload \"${INSTALL_DIR}/${MODULE_NAME}\")"
echo "  Then type EL for the palette. Keep the resources/ folder beside the module."
