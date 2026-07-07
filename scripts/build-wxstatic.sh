#!/usr/bin/env bash
# =============================================================================
#  build-wxstatic.sh - Build a private, STATIC wxWidgets for open-electrical.
#
#  BricsCAD ships and loads its own wxWidgets (3.1). Linking our plugin against
#  the system's *shared* wxWidgets (3.2) puts two wx runtimes in one process
#  and crashes BricsCAD on load. The supported fix (per the BRX brxWxSample
#  docs) is to link wxWidgets *statically* into the module and keep its symbols
#  private, so it cannot collide with BricsCAD's wx.
#
#  This script downloads a wxWidgets release and builds it static (GTK3). The
#  module link step then localises every wx symbol via src/exports.map.
#
#  Result: a wx-config at ${WXSTATIC_PREFIX}/bin/wx-config that reports static
#  libraries. Pass it to CMake with -DwxWidgets_CONFIG_EXECUTABLE=...
# =============================================================================
set -euo pipefail

WX_VERSION="${WX_VERSION:-3.2.8}"
WXSTATIC_PREFIX="${WXSTATIC_PREFIX:-${HOME}/.local/opt/wxstatic-oel-${WX_VERSION}}"
WORK_DIR="${WORK_DIR:-${HOME}/.cache/open-electrical/wxbuild}"
JOBS="${JOBS:-$(nproc)}"

WX_URL="https://github.com/wxWidgets/wxWidgets/releases/download/v${WX_VERSION}/wxWidgets-${WX_VERSION}.tar.bz2"

# Already built? Skip.
if [ -x "${WXSTATIC_PREFIX}/bin/wx-config" ]; then
    echo "[build-wxstatic] Already present: ${WXSTATIC_PREFIX}/bin/wx-config"
    exit 0
fi

command -v curl >/dev/null 2>&1 || { echo "curl required"; exit 1; }
command -v make >/dev/null 2>&1 || { echo "make required"; exit 1; }

mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

TARBALL="wxWidgets-${WX_VERSION}.tar.bz2"
if [ ! -f "$TARBALL" ]; then
    echo "[build-wxstatic] Downloading ${WX_URL}"
    curl -L --fail -o "$TARBALL" "$WX_URL"
fi

SRC_DIR="${WORK_DIR}/wxWidgets-${WX_VERSION}"
if [ ! -d "$SRC_DIR" ]; then
    echo "[build-wxstatic] Extracting ..."
    tar xf "$TARBALL"
fi

BUILD_DIR="${SRC_DIR}/build-static"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Static, Unicode, GTK3. -fPIC so the archives can be linked into our shared
# module. We deliberately do NOT need OpenGL / tests / webview.
if [ ! -f Makefile ]; then
    echo "[build-wxstatic] Configuring (static, GTK3) ..."
    ../configure \
        --prefix="$WXSTATIC_PREFIX" \
        --disable-shared \
        --enable-unicode \
        --with-gtk=3 \
        --disable-tests \
        --without-opengl \
        --disable-webview \
        --enable-vendor=oel \
        CFLAGS="-fPIC" \
        CXXFLAGS="-fPIC"
fi

echo "[build-wxstatic] Building ($JOBS jobs) ... this can take a while."
make -j"$JOBS"

echo "[build-wxstatic] Installing into ${WXSTATIC_PREFIX}"
make install

echo "[build-wxstatic] Done."
echo "    wx-config: ${WXSTATIC_PREFIX}/bin/wx-config"
"${WXSTATIC_PREFIX}/bin/wx-config" --version
