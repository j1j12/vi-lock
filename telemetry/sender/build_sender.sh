#!/bin/bash
# The vendor SDK reads optional variables such as LD_LIBRARY_PATH directly.
# Do not enable nounset while sourcing it (same policy as build_gui.sh).
set -e
SDK_ENV=/opt/st/stm32mp1/3.1-snapshot/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
[ -f "$SDK_ENV" ] || { echo "SDK missing: $SDK_ENV" >&2; exit 1; }
source "$SDK_ENV"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$HOME/access_probe_sender_build}"
QT_HOST_BINS="$OECORE_NATIVE_SYSROOT/usr/bin/qt5"
for candidate in "$OECORE_NATIVE_SYSROOT/usr/bin/qt5" "$OECORE_NATIVE_SYSROOT/usr/lib/qt5/bin" "$OECORE_NATIVE_SYSROOT/usr/bin"; do
    if [ -x "$candidate/moc" ]; then QT_HOST_BINS="$candidate"; break; fi
done
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DOE_QMAKE_PATH_EXTERNAL_HOST_BINS="$QT_HOST_BINS"
cmake --build "$BUILD_DIR" -- -j2
