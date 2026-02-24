#!/bin/bash
export PATH="$PATH:/d/projects/emsdk/bin_sh"
export PATH="$PATH:/d/projects/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin"
export PATH="$PATH:/d/projects/vcpkg/downloads/tools/ninja-1.13.2-windows"
export EMSDK=/d/projects/emsdk
export EM_CONFIG=/d/projects/emsdk/.emscripten

WASM_DIR="/d/projects/kys-cpp/kys-cpp/wasm"
BUILD_DIR="$WASM_DIR/build"

cd "$BUILD_DIR"

echo "=== Configuring ==="
emcmake cmake "$WASM_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWASM_DEPS_DIR="/d/projects/vcpkg/installed/wasm32-emscripten" \
    -G Ninja 2>&1

echo ""
echo "=== Building ==="
emmake ninja 2>&1
