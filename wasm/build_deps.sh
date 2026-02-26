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
    emcc -O2 -pthread -DSQLITE_OMIT_LOAD_EXTENSION \
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
        -DYAML_BUILD_SHARED_LIBS=OFF \
        -DCMAKE_C_FLAGS="-pthread" \
        -DCMAKE_CXX_FLAGS="-pthread"
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
        git clone --depth 1 --branch v1.11.4 \
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
        -DENABLE_BZIP2=ON \
        -DENABLE_LZMA=OFF \
        -DENABLE_ZSTD=OFF \
        -DENABLE_OPENSSL=OFF \
        -DENABLE_GNUTLS=OFF \
        -DENABLE_MBEDTLS=OFF \
        -DCMAKE_C_FLAGS="-pthread" \
        -DCMAKE_CXX_FLAGS="-pthread" \
        -DCMAKE_PREFIX_PATH="${INSTALL_DIR}"
    emmake make -j$(nproc) install
    echo "libzip done."
else
    echo "libzip already built, skipping."
fi

# ============================================================
# 4. SDL3_image (built from source — not available as Emscripten port)
# ============================================================
echo "--- Building SDL3_image ---"
SDLIMAGE_DIR="${DEPS_DIR}/SDL_image"
if [ ! -f "${INSTALL_DIR}/lib/libSDL3_image.a" ]; then
    if [ ! -d "${SDLIMAGE_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 --branch release-3.2.6 \
            https://github.com/libsdl-org/SDL_image.git
    fi
    # Ensure SDL3 version file exists for find_package
    SDL3_CMAKE_DIR="$(dirname "$(which emcc)")/../cache/sysroot/lib/cmake/SDL3"
    if [ ! -f "${SDL3_CMAKE_DIR}/sdl3-config-version.cmake" ]; then
        cat > "${SDL3_CMAKE_DIR}/sdl3-config-version.cmake" << 'VEOF'
set(PACKAGE_VERSION "3.2.30")
if("${PACKAGE_FIND_VERSION_MAJOR}" STREQUAL "3")
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if("${PACKAGE_FIND_VERSION}" VERSION_LESS_EQUAL "${PACKAGE_VERSION}")
    set(PACKAGE_VERSION_EXACT FALSE)
  endif()
else()
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
endif()
VEOF
    fi
    mkdir -p "${SDLIMAGE_DIR}/build_wasm"
    cd "${SDLIMAGE_DIR}/build_wasm"
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL3IMAGE_PNG=ON -DSDL3IMAGE_PNG_VENDORED=ON \
        -DSDL3IMAGE_BMP=ON \
        -DSDL3IMAGE_JPG=OFF -DSDL3IMAGE_WEBP=OFF \
        -DSDL3IMAGE_AVIF=OFF -DSDL3IMAGE_JXL=OFF \
        -DSDL3IMAGE_TIF=OFF \
        -DCMAKE_C_FLAGS="-pthread" \
        -DCMAKE_CXX_FLAGS="-pthread" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -G Ninja
    emmake ninja
    emmake ninja install
    echo "SDL3_image done."
else
    echo "SDL3_image already built, skipping."
fi

# ============================================================
# 5. SDL3_ttf (built from source — not available as Emscripten port)
# ============================================================
echo "--- Building SDL3_ttf ---"
SDLTTF_DIR="${DEPS_DIR}/SDL_ttf"
if [ ! -f "${INSTALL_DIR}/lib/libSDL3_ttf.a" ]; then
    if [ ! -d "${SDLTTF_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 --branch release-3.2.2 \
            https://github.com/libsdl-org/SDL_ttf.git
        cd SDL_ttf
        git submodule update --init external/freetype
        git submodule update --init external/plutovg
        git submodule update --init external/plutosvg
    fi
    mkdir -p "${SDLTTF_DIR}/build_wasm"
    cd "${SDLTTF_DIR}/build_wasm"
    emcmake cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL3TTF_VENDORED=ON \
        -DSDL3TTF_HARFBUZZ=OFF \
        -DCMAKE_C_FLAGS="-pthread" \
        -DCMAKE_CXX_FLAGS="-pthread" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -G Ninja
    emmake ninja
    emmake ninja install
    # Copy vendored static libs not installed automatically
    cp external/freetype/libfreetype.a "${INSTALL_DIR}/lib/"
    cp external/plutosvg/libplutosvg.a "${INSTALL_DIR}/lib/"
    cp external/plutovg/libplutovg.a "${INSTALL_DIR}/lib/"
    echo "SDL3_ttf done."
else
    echo "SDL3_ttf already built, skipping."
fi

# # ============================================================
# # 6. OpenCC
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
# 7. FluidSynth (MIDI synthesizer - built from source with cpp11 osal)
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
        -DCMAKE_C_FLAGS="-fexceptions -pthread" \
        -DCMAKE_CXX_FLAGS="-fexceptions -pthread" \
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
# 8. SDL3_mixer (prerelease - build from source with FluidSynth)
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
        -DCMAKE_C_FLAGS="-pthread" \
        -DCMAKE_CXX_FLAGS="-pthread" \
        -DCMAKE_FIND_ROOT_PATH="${INSTALL_DIR}" \
        -G Ninja
    emmake ninja
    emmake ninja install
    echo "SDL3_mixer done."
else
    echo "SDL3_mixer already built, skipping."
fi

# ============================================================
# 9. marisa-trie (used by Hanz2Piny)
# ============================================================
echo "--- Building marisa ---"
MARISA_DIR="${DEPS_DIR}/marisa-trie"
if [ ! -f "${INSTALL_DIR}/lib/libmarisa.a" ]; then
    if [ ! -d "${MARISA_DIR}" ]; then
        cd "${DEPS_DIR}"
        git clone --depth 1 https://github.com/s-yata/marisa-trie.git
    fi
    # marisa-trie doesn't have a CMake build — compile manually
    cd "${MARISA_DIR}"
    MARISA_SRCS="lib/marisa/grimoire/io/mapper.cc
lib/marisa/grimoire/io/reader.cc
lib/marisa/grimoire/io/writer.cc
lib/marisa/grimoire/trie/louds-trie.cc
lib/marisa/grimoire/trie/tail.cc
lib/marisa/grimoire/vector/bit-vector.cc
lib/marisa/agent.cc
lib/marisa/keyset.cc
lib/marisa/trie.cc"
    OBJS=""
    for src in $MARISA_SRCS; do
        obj="$(basename "${src}" .cc).o"
        em++ -O2 -pthread -I lib -c "$src" -o "$obj"
        OBJS="$OBJS $obj"
    done
    emar rcs libmarisa.a $OBJS
    cp libmarisa.a "${INSTALL_DIR}/lib/"
    cp -r lib/marisa "${INSTALL_DIR}/include/"
    rm -f $OBJS
    echo "marisa done."
else
    echo "marisa already built, skipping."
fi

echo ""
echo "=== All dependencies built ==="
echo "Use -DWASM_DEPS_DIR=${INSTALL_DIR} when building kys"
