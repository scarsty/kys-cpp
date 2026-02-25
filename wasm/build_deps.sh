#!/bin/bash
# Build WASM dependencies for kys-cpp
# Prerequisites: emsdk must be installed and activated (source emsdk_env.sh)
#
# Usage: ./build_deps.sh [install_prefix]
# Default install prefix: ./wasm_deps

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="${SCRIPT_DIR}/deps_src"
INSTALL_DIR="${1:-${SCRIPT_DIR}/wasm_deps}"

mkdir -p "${DEPS_DIR}" "${INSTALL_DIR}" "${INSTALL_DIR}/lib" "${INSTALL_DIR}/include"

echo "=== Building WASM dependencies ==="
echo "Install prefix: ${INSTALL_DIR}"

if ! command -v emcc &> /dev/null; then
    echo "ERROR: emcc not found. Activate emsdk first: source emsdk_env.sh"
    exit 1
fi
echo "Using emcc: $(which emcc)"

# ============================================================
# 1. sqlite3 (amalgamation - single file)
# ============================================================
echo "--- Building sqlite3 ---"
SQLITE_DIR="${DEPS_DIR}/sqlite"
if [ ! -f "${INSTALL_DIR}/lib/libsqlite3.a" ]; then
    mkdir -p "${SQLITE_DIR}"
    if [ ! -f "${SQLITE_DIR}/sqlite3.c" ]; then
        cd "${SQLITE_DIR}"
        curl -L -o sqlite.zip \
            "https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip"
        unzip -o sqlite.zip
        mv sqlite-amalgamation-*/sqlite3.c \
           sqlite-amalgamation-*/sqlite3.h .
        rm -rf sqlite-amalgamation-* sqlite.zip
    fi
    cd "${SQLITE_DIR}"
    emcc -O2 -DSQLITE_OMIT_LOAD_EXTENSION \
        -c sqlite3.c -o sqlite3.o
    emar rcs libsqlite3.a sqlite3.o
    cp libsqlite3.a "${INSTALL_DIR}/lib/"
    cp sqlite3.h "${INSTALL_DIR}/include/"
    echo "sqlite3 done."
else
    echo "sqlite3 already built, skipping."
fi

# ============================================================
# 2. yaml-cpp
# ============================================================
echo "--- Building yaml-cpp ---"
YAMLCPP_DIR="${DEPS_DIR}/yaml-cpp"
if [ ! -f "${INSTALL_DIR}/lib/libyaml-cpp.a" ]; then
    if [ ! -d "${YAMLCPP_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 --branch 0.8.0 \
            https://github.com/jbeder/yaml-cpp.git
    fi
    mkdir -p "${YAMLCPP_DIR}/build_wasm"
    cd "${YAMLCPP_DIR}/build_wasm"
    emcmake cmake .. \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DYAML_CPP_BUILD_TESTS=OFF \
        -DYAML_CPP_BUILD_TOOLS=OFF \
        -DYAML_BUILD_SHARED_LIBS=OFF
    emmake make -j$(nproc) install
    echo "yaml-cpp done."
else
    echo "yaml-cpp already built, skipping."
fi

# ============================================================
# 3. libzip (depends on zlib which emscripten provides)
# ============================================================
echo "--- Building libzip ---"
LIBZIP_DIR="${DEPS_DIR}/libzip"
if [ ! -f "${INSTALL_DIR}/lib/libzip.a" ]; then
    if [ ! -d "${LIBZIP_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 --branch v1.10.1 \
            https://github.com/nih-at/libzip.git
    fi
    mkdir -p "${LIBZIP_DIR}/build_wasm"
    cd "${LIBZIP_DIR}/build_wasm"
    emcmake cmake .. \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TOOLS=OFF \
        -DBUILD_REGRESS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_DOC=OFF \
        -DENABLE_BZIP2=OFF \
        -DENABLE_LZMA=OFF \
        -DENABLE_ZSTD=OFF \
        -DENABLE_OPENSSL=OFF \
        -DENABLE_GNUTLS=OFF \
        -DENABLE_MBEDTLS=OFF
    emmake make -j$(nproc) install
    echo "libzip done."
else
    echo "libzip already built, skipping."
fi

# # ============================================================
# # 4. OpenCC
# # ============================================================
# echo "--- Building OpenCC ---"
# OPENCC_DIR="${DEPS_DIR}/OpenCC"
# if [ ! -f "${INSTALL_DIR}/lib/libopencc.a" ]; then
#     if [ ! -d "${OPENCC_DIR}" ]; then
#         cd "${DEPS_DIR}"
#         git clone --depth 1 --branch ver.1.1.7 \
#             https://github.com/BYVoid/OpenCC.git
#     fi
#     mkdir -p "${OPENCC_DIR}/build_wasm"
#     cd "${OPENCC_DIR}/build_wasm"
#     emcmake cmake .. \
#         -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DBUILD_SHARED_LIBS=OFF \
#         -DENABLE_BENCHMARK=OFF \
#         -DENABLE_GTEST=OFF \
#         -DENABLE_DARTS=ON
#     emmake make -j$(nproc) install
#     echo "OpenCC done."
# else
#     echo "OpenCC already built, skipping."
# fi

# ============================================================
# 5. FluidSynth (MIDI synthesizer - built from source with cpp11 osal)
# ============================================================
echo "--- Building FluidSynth ---"
FLUIDSYNTH_DIR="${DEPS_DIR}/fluidsynth"
if [ ! -f "${INSTALL_DIR}/lib/libfluidsynth.a" ]; then
    if [ ! -d "${FLUIDSYNTH_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 https://github.com/FluidSynth/fluidsynth.git
    fi
    mkdir -p "${FLUIDSYNTH_DIR}/build_wasm"
    cd "${FLUIDSYNTH_DIR}/build_wasm"
    emcmake cmake .. \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -Dosal=cpp11 \
        -Denable-libinstpatch=0 \
        -DCMAKE_C_FLAGS="-fexceptions" \
        -DCMAKE_CXX_FLAGS="-fexceptions" \
        -G Ninja
    emmake ninja libfluidsynth
    # Manual install (skip fluidsynth CLI which may fail to link)
    cp src/libfluidsynth.a "${INSTALL_DIR}/lib/"
    cp -r "${FLUIDSYNTH_DIR}/include/fluidsynth" "${INSTALL_DIR}/include/"
    cp include/fluidsynth/version.h "${INSTALL_DIR}/include/fluidsynth/"
    cp include/fluidsynth.h "${INSTALL_DIR}/include/"
    echo "FluidSynth done."
else
    echo "FluidSynth already built, skipping."
fi

# ============================================================
# 6. SDL3_mixer (prerelease - build from source with FluidSynth)
# ============================================================
echo "--- Building SDL3_mixer ---"
SDLMIXER_DIR="${DEPS_DIR}/SDL_mixer"
if [ ! -f "${INSTALL_DIR}/lib/libSDL3_mixer.a" ]; then
    if [ ! -d "${SDLMIXER_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 --branch prerelease-3.1.2 \
            https://github.com/libsdl-org/SDL_mixer.git
    fi
    mkdir -p "${SDLMIXER_DIR}/build_wasm"
    cd "${SDLMIXER_DIR}/build_wasm"
    emcmake cmake .. \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDLMIXER_VENDORED=ON \
        -DSDLMIXER_INSTALL=ON \
        -DSDLMIXER_DEPS_SHARED=OFF \
        -DSDLMIXER_SAMPLES=OFF \
        -DSDLMIXER_TESTS=OFF \
        -DSDLMIXER_WAVE=ON \
        -DSDLMIXER_MP3=ON \
        -DSDLMIXER_MP3_DRMP3=ON \
        -DSDLMIXER_MP3_MPG123=OFF \
        -DSDLMIXER_MIDI=ON \
        -DSDLMIXER_MIDI_TIMIDITY=OFF \
        -DSDLMIXER_MIDI_FLUIDSYNTH=ON \
        -DSDLMIXER_MIDI_FLUIDSYNTH_SHARED=OFF \
        -DSDLMIXER_FLAC=OFF \
        -DSDLMIXER_OPUS=OFF \
        -DSDLMIXER_VORBIS_STB=OFF \
        -DSDLMIXER_VORBIS_VORBISFILE=OFF \
        -DSDLMIXER_VORBIS_TREMOR=OFF \
        -DSDLMIXER_MOD=OFF \
        -DSDLMIXER_GME=OFF \
        -DSDLMIXER_WAVPACK=OFF \
        -DCMAKE_FIND_ROOT_PATH="${INSTALL_DIR}" \
        -G Ninja
    emmake ninja
    emmake ninja install
    echo "SDL3_mixer done."
else
    echo "SDL3_mixer already built, skipping."
fi

echo ""
echo "=== All dependencies built ==="
echo "Use -DWASM_DEPS_DIR=${INSTALL_DIR} when building kys"
