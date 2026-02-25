#!/bin/bash
# Build kys-cpp for WASM
# Prerequisites:
#   1. emsdk activated (source emsdk_env.sh)
#   2. Dependencies built (./build_deps.sh)
#
# Usage: ./build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="${SCRIPT_DIR}/wasm_deps"
BUILD_DIR="${SCRIPT_DIR}/build"

if ! command -v emcc &> /dev/null; then
    echo "ERROR: emcc not found. Activate emsdk first."
    exit 1
fi

if [ ! -d "${DEPS_DIR}/lib" ]; then
    echo "ERROR: Dependencies not built. Run ./build_deps.sh first."
    exit 1
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

emcmake cmake "${SCRIPT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWASM_DEPS_DIR="${DEPS_DIR}"

emmake make -j$(nproc)

echo ""
echo "=== Build complete ==="
echo "Output: ${BUILD_DIR}/kys.html"
echo ""
echo "To test locally:"
echo "  cd ${BUILD_DIR}"
echo "  python3 -m http.server 8080"
echo "  Open http://localhost:8080/kys.html"
