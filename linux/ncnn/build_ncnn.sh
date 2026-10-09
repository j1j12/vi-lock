#!/bin/bash
# Build NCNN for STM32MP157 (ARM Cortex-A7) using the OpenSTLinux SDK toolchain.
# Uses NCNN tag 20210507 (no fp16-storage code, full ARMv7 support).

set -e

SDK_ENV=/opt/st/stm32mp1/3.1-snapshot/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
TOOLCHAIN="$(cd "$(dirname "$0")" && pwd)/arm-ostl-linux-gnueabi.toolchain.cmake"

if [ -f "$SDK_ENV" ]; then
    echo "==> Sourcing SDK environment: $SDK_ENV"
    source "$SDK_ENV"
else
    echo "SDK environment script not found: $SDK_ENV"
    exit 1
fi

NCNN_SRC="${NCNN_SRC:-$HOME/ncnn}"
BUILD_DIR="$NCNN_SRC/build-arm"

if [ ! -d "$NCNN_SRC" ]; then
    echo "==> Cloning NCNN (tag 20210507) ..."
    git clone --depth 1 --branch 20210507 https://github.com/Tencent/ncnn.git "$NCNN_SRC"
else
    echo "==> $NCNN_SRC exists, checking out tag 20210507 ..."
    cd "$NCNN_SRC"
    git fetch --depth 1 origin tag 20210507
    git checkout -f 20210507
    cd - >/dev/null
fi

rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "==> Configuring with toolchain: $TOOLCHAIN"
cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install" \
      -DCMAKE_BUILD_TYPE=Release \
      -DNCNN_BUILD_TOOLS=OFF \
      -DNCNN_BUILD_EXAMPLES=OFF \
      -DNCNN_BUILD_BENCHMARK=OFF \
      -DNCNN_BUILD_TESTS=OFF \
      -DNCNN_VULKAN=OFF \
      -DNCNN_OPENMP=OFF \
      ..

echo "==> Building ..."
make -j"$(nproc)"

echo "==> Installing to $BUILD_DIR/install"
make install

echo "==> Done. Library: $BUILD_DIR/install/lib/libncnn.a"
