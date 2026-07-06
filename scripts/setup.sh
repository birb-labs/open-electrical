#!/usr/bin/env bash
# =============================================================================
#  setup.sh - Build & install BricsCAD.Electrical (Linux)
#
#  Compiles the plugin and copies the artefacts into the BricsCAD bundle
#  directory so it is auto-loaded the next time BricsCAD starts.
#
#  Prerequisites:
#    • BRX SDK V26 installed and BRX_SDK_ROOT exported (e.g. /opt/brx_sdk_v26)
#    • wxWidgets ≥ 3.2, development headers installed
#    • CMake ≥ 3.20, a C++17-capable compiler (GCC ≥ 9 / Clang ≥ 10)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# ---------------------------------------------------------------------------
#  Configurable variables  (override via environment)
# ---------------------------------------------------------------------------
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build}"
INSTALL_DIR="${INSTALL_DIR:-${HOME}/.local/share/BricsCAD/V26x64/BricsCAD.Electrical}"
CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL="${PARALLEL:-$(nproc)}"

# ---------------------------------------------------------------------------
#  Step 1 - Configure
# ---------------------------------------------------------------------------
echo "[setup.sh] Configuring with CMake ..."
cmake -S "$PROJECT_DIR" \
      -B "$BUILD_DIR" \
      -G "$CMAKE_GENERATOR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# ---------------------------------------------------------------------------
#  Step 2 - Build
# ---------------------------------------------------------------------------
echo "[setup.sh] Building ($PARALLEL parallel jobs) ..."
cmake --build "$BUILD_DIR" --parallel "$PARALLEL"

# ---------------------------------------------------------------------------
#  Step 3 - Install (copy bundle)
# ---------------------------------------------------------------------------
echo "[setup.sh] Installing into: ${INSTALL_DIR}"
mkdir -p "$INSTALL_DIR"

# Copy the .brx itself.
cp -f "$BUILD_DIR/BricsCAD.Electrical.brx" "$INSTALL_DIR/"

# Copy the resources folder (localisation, symbols, etc.).
if [ -d "$BUILD_DIR/resources" ]; then
    cp -rf "$BUILD_DIR/resources" "$INSTALL_DIR/"
fi

echo "[setup.sh] ✓ Done — BricsCAD.Electrical installed."
echo "[setup.sh]   Bundle : ${INSTALL_DIR}"
