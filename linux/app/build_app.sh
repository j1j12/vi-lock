#!/bin/bash
# Build the face recognition app for STM32MP157 (A7) using the OpenSTLinux SDK.
# Uses the SDK's own CMake toolchain file; builds in a LOCAL directory.

set -e

SDK_ENV=/opt/st/stm32mp1/3.1-snapshot/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLCHAIN="$SRC_DIR/../ncnn/arm-ostl-linux-gnueabi.toolchain.cmake"
BUILD_DIR="${BUILD_DIR:-$HOME/face_app_build}"

if [ -f "$SDK_ENV" ]; then
    echo "==> Sourcing SDK environment: $SDK_ENV"
    source "$SDK_ENV"
else
    echo "SDK environment not found: $SDK_ENV"
    exit 1
fi

NCNN_DIR="${NCNN_DIR:-$HOME/ncnn/build-arm/install}"

echo "==> Source dir: $SRC_DIR"
echo "==> Build dir:  $BUILD_DIR"

rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DNCNN_DIR="$NCNN_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      "$SRC_DIR"

make -j"$(nproc)"

echo "==> Built: $BUILD_DIR/face_app"
