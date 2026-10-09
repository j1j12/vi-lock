#!/bin/bash
set -e

SDK_ENV=/opt/st/stm32mp1/3.1-snapshot/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$HOME/access_control_gui_build}"

if [ ! -f "$SDK_ENV" ]; then
    echo "SDK environment not found: $SDK_ENV" >&2
    exit 1
fi

source "$SDK_ENV"

# The OpenEmbedded Qt5 CMake package deliberately returns early unless the
# native (build-host) Qt tools directory is supplied. Locate moc in the SDK
# native sysroot instead of relying on a host Qt installation.
QT_HOST_BINS=""
for candidate in \
    "$OECORE_NATIVE_SYSROOT/usr/bin/qt5" \
    "$OECORE_NATIVE_SYSROOT/usr/lib/qt5/bin" \
    "$OECORE_NATIVE_SYSROOT/usr/bin"
do
    if [ -x "$candidate/moc" ]; then
        QT_HOST_BINS="$candidate"
        break
    fi
done

if [ -z "$QT_HOST_BINS" ]; then
    echo "Qt host tool 'moc' not found under $OECORE_NATIVE_SYSROOT" >&2
    exit 1
fi

echo "==> Qt host tools: $QT_HOST_BINS"
mkdir -p "$BUILD_DIR"
rm -f "$BUILD_DIR/CMakeCache.txt"
rm -rf "$BUILD_DIR/CMakeFiles"

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOE_QMAKE_PATH_EXTERNAL_HOST_BINS="$QT_HOST_BINS"
cmake --build "$BUILD_DIR" -- -j"$(nproc)"
echo "==> Built: $BUILD_DIR/access_control_gui"
