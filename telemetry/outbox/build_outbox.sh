#!/bin/bash
set -e
# Vendor environment reads optional unset variables: do not enable nounset here.
SDK_ENV=/opt/st/stm32mp1/3.1-snapshot/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
[ -f "$SDK_ENV" ] || { echo "SDK missing: $SDK_ENV" >&2; exit 1; }
source "$SDK_ENV"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$HOME/access_event_outbox_build}"
QT_HOST_BINS=""
for candidate in "$OECORE_NATIVE_SYSROOT/usr/bin/qt5" "$OECORE_NATIVE_SYSROOT/usr/lib/qt5/bin" "$OECORE_NATIVE_SYSROOT/usr/bin"; do
    if [ -x "$candidate/moc" ]; then QT_HOST_BINS="$candidate"; break; fi
done
[ -n "$QT_HOST_BINS" ] || { echo "SDK host Qt moc not found under $OECORE_NATIVE_SYSROOT" >&2; exit 1; }
echo "SDK Qt host tools: $QT_HOST_BINS"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DOE_QMAKE_PATH_EXTERNAL_HOST_BINS="$QT_HOST_BINS"
cmake --build "$BUILD_DIR" -- -j2
