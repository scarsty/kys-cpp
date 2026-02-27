#!/bin/bash
export PATH="$PATH:/d/projects/emsdk/bin_sh"
export PATH="$PATH:/d/projects/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin"
export PATH="$PATH:/d/projects/vcpkg/downloads/tools/ninja-1.13.2-windows"
export EMSDK=/d/projects/emsdk
export EM_CONFIG=/d/projects/emsdk/.emscripten

WASM_DIR="/d/projects/kys-cpp/kys-cpp/wasm"
BUILD_DIR="$WASM_DIR/build"

# rm -f d:/projects/kys-cpp/kys-cpp/wasm/build/kys.html d:/projects/kys-cpp/kys-cpp/wasm/build/kys.js d:/projects/kys-cpp/kys-cpp/wasm/build/kys.wasm
mkdir -p "$BUILD_DIR"

# Create game directory junction if it doesn't exist
GAME_LINK="$BUILD_DIR/kys/game"
GAME_TARGET="D:\\projects\\kys-cpp\\kys-cpp\\work\\game-dev"
if [ ! -e "$GAME_LINK" ]; then
    echo "=== Creating game junction ==="
    mkdir -p "$BUILD_DIR/kys"
    GAME_LINK_WIN=$(cygpath -w "$GAME_LINK")
    cmd.exe //c mklink //J "$GAME_LINK_WIN" "$GAME_TARGET"
fi

cd "$BUILD_DIR"

echo "=== Generating manifest ==="
python3 "$WASM_DIR/gen_manifest.py" "/d/projects/kys-cpp/kys-cpp/work/game-dev" "$WASM_DIR/wasm_manifest.inc"

echo "=== Configuring ==="
emcmake cmake "$WASM_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWASM_DEPS_DIR="/d/projects/vcpkg/installed/wasm32-emscripten" \
    -G Ninja 2>&1

echo ""
echo "=== Building ==="
emmake ninja 2>&1

cp "$WASM_DIR/serve.py" "$BUILD_DIR/serve.py"