# Building kys-cpp for WebAssembly (WASM)

This document describes how to build kys-cpp as a browser-playable WebAssembly application.

## Overview

The WASM build uses Emscripten to compile the C++ codebase to WebAssembly. Key differences from the native build:

- **Audio**: Fully stubbed (no SDL_mixer). Can be re-implemented later via Web Audio API.
- **Scripting**: Lua is stubbed (unused in the chess mod).
- **Networking**: Disabled (`WITH_NETWORK=OFF` equivalent — no network code compiled).
- **Frame loop**: Uses `emscripten_sleep()` with JSPI (JavaScript Promise Integration) instead of `std::this_thread::sleep_for`.
- **Game assets**: Preloaded into Emscripten's virtual filesystem via `--preload-file`.

## Prerequisites

- **Git Bash** on Windows (the build scripts use Unix shell syntax)
- **Emscripten SDK (emsdk)** — tested with 5.0.1
- **vcpkg** with the `wasm32-emscripten` community triplet
- **Game assets** in `work/game-dev/`

## Step 1: Install Emscripten SDK

```bash
cd /d/projects
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 5.0.1
./emsdk activate 5.0.1
```

### Shell wrappers (Windows Git Bash only)

Emscripten's `.bat` files don't work in Git Bash. Create shell wrappers in `emsdk/bin_sh/`:

```bash
mkdir -p /d/projects/emsdk/bin_sh

# Find the bundled Python path
PYTHON="/d/projects/emsdk/python/3.13.3_64bit/python.exe"
EM_DIR="/d/projects/emsdk/upstream/emscripten"

for tool in emcc em++ emar emranlib emcmake emmake emconfigure; do
    cat > "/d/projects/emsdk/bin_sh/$tool" << EOF
#!/bin/bash
exec $PYTHON $EM_DIR/${tool}.py "\$@"
EOF
    chmod +x "/d/projects/emsdk/bin_sh/$tool"
done
```

## Step 2: Install vcpkg dependencies

```bash
cd /d/projects/vcpkg
git pull
./bootstrap-vcpkg.sh

# These all support the wasm32-emscripten triplet
vcpkg install sqlite3:wasm32-emscripten
vcpkg install yaml-cpp:wasm32-emscripten
vcpkg install "libzip[core,bzip2]:wasm32-emscripten"
```

> **Note**: Install libzip with `[core,bzip2]` only — the default openssl feature fails to build for WASM.

## Step 3: Build SDL3_image from source

Emscripten has SDL3 as a built-in port (`-sUSE_SDL=3`), but SDL3_image and SDL3_ttf are **not** available as ports and must be built manually.

```bash
DEPS_SRC="/d/projects/kys-cpp/kys-cpp/wasm/deps_src"
VCPKG_WASM="/d/projects/vcpkg/installed/wasm32-emscripten"
mkdir -p "$DEPS_SRC" && cd "$DEPS_SRC"

git clone --branch release-3.2.6 --depth 1 https://github.com/libsdl-org/SDL_image.git
cd SDL_image
mkdir build_wasm && cd build_wasm
```

Before configuring, you may need to create a version file for Emscripten's SDL3 port so `find_package(SDL3 ...)` succeeds:

```bash
cat > /d/projects/emsdk/upstream/emscripten/cache/sysroot/lib/cmake/SDL3/sdl3-config-version.cmake << 'EOF'
set(PACKAGE_VERSION "3.2.30")
if("${PACKAGE_FIND_VERSION_MAJOR}" STREQUAL "3")
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if("${PACKAGE_FIND_VERSION}" VERSION_LESS_EQUAL "${PACKAGE_VERSION}")
    set(PACKAGE_VERSION_EXACT FALSE)
  endif()
else()
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
endif()
EOF
```

Then build:

```bash
emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL3IMAGE_PNG=ON -DSDL3IMAGE_PNG_VENDORED=ON \
    -DSDL3IMAGE_BMP=ON \
    -DSDL3IMAGE_JPG=OFF -DSDL3IMAGE_WEBP=OFF \
    -DSDL3IMAGE_AVIF=OFF -DSDL3IMAGE_JXL=OFF \
    -DSDL3IMAGE_TIF=OFF \
    -DCMAKE_INSTALL_PREFIX="$VCPKG_WASM" \
    -G Ninja

emmake ninja
emmake ninja install
```

## Step 4: Build SDL3_ttf from source

```bash
cd "$DEPS_SRC"
git clone --branch release-3.2.2 --depth 1 https://github.com/libsdl-org/SDL_ttf.git
cd SDL_ttf

# Fetch vendored submodules (freetype, plutovg, plutosvg)
git submodule update --init external/freetype
git submodule update --init external/plutovg
git submodule update --init external/plutosvg

mkdir build_wasm && cd build_wasm

emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL3TTF_VENDORED=ON \
    -DSDL3TTF_HARFBUZZ=OFF \
    -DCMAKE_INSTALL_PREFIX="$VCPKG_WASM" \
    -G Ninja

emmake ninja
emmake ninja install
```

After install, copy the vendored static libraries that don't get installed automatically:

```bash
cp external/freetype/libfreetype.a "$VCPKG_WASM/lib/"
cp external/plutosvg/libplutosvg.a "$VCPKG_WASM/lib/"
cp external/plutovg/libplutovg.a "$VCPKG_WASM/lib/"
```

## Step 5: Build kys-cpp for WASM

```bash
cd /d/projects/kys-cpp/kys-cpp/wasm
mkdir -p build && cd build

emcmake cmake ../wasm \
    -DCMAKE_BUILD_TYPE=Release \
    -DWASM_DEPS_DIR="/d/projects/vcpkg/installed/wasm32-emscripten" \
    -G Ninja

emmake ninja
```

Or use the provided `rebuild.sh` (adjust paths in the script to match your environment):

```bash
bash wasm/rebuild.sh
```

### Output files

After a successful build, `wasm/build/` will contain:

| File | Description |
|------|-------------|
| `kys.html` | HTML shell page |
| `kys.js` | Emscripten runtime (~350KB) |
| `kys.wasm` | Compiled binary (~6.5MB) |
| `kys.data` | Preloaded game assets (~164MB, excluding music/sound) |

## Step 6: Run in browser

The files must be served over HTTP (browsers block `file://` for WASM):

```bash
cd /d/projects/kys-cpp/kys-cpp/wasm/build
python -m http.server 8080
```

Open `http://localhost:8080/kys.html` in Chrome 123+ or Edge 123+ (required for JSPI support).

## Source code changes for WASM

All changes are guarded with `#ifdef __EMSCRIPTEN__` / `#ifndef __EMSCRIPTEN__` so they don't affect the native build.

### `src/Audio.h` / `src/Audio.cpp`
- Stub type aliases (`MUSIC = int`, `WAV = int`, `MIDI_FONT = void*`)
- All audio methods become no-ops
- Private methods `loadMusic`, `loadWav`, `playMusic(MUSIC)`, `playWav` excluded from WASM build (avoids signature collision since `MUSIC = int`)

### `src/Script.h` / `src/Script.cpp`
- Full stub class with all methods inlined as no-ops (no Lua dependency)

### `src/Engine.h`
- `#include <emscripten.h>` under WASM
- `Engine::delay()` uses `emscripten_sleep()` instead of `std::this_thread::sleep_for`

### `src/Engine.cpp`
- `renderPresent()`: skip `SDL_RenderClear` after present on WASM (prevents frame flashing)
- `renderMainTextureToWindow()`: clear before draw instead on WASM

### `src/GameUtil.h`
- Game data path set to `/game/` (Emscripten virtual filesystem mount point)

### `src/BattleSceneHades.cpp`
- Fixed narrowing conversion (`double` → `int`) that Emscripten's clang rejects

### `src/ChessPool.cpp`
- Replaced `std::ranges::find` with `std::find` (Emscripten's libc++ lacks ranges support)

### `mlcc/INIReader.h`
- Added `#include <sys/stat.h>` (not implicitly included on Emscripten)

### `mlcc/ZipFile.cpp`
- Wrapped `CvtStringToUTF8` (uses `MultiByteToWideChar`) in `#ifdef _WIN32`

## Known limitations

- **No audio** — stubbed out entirely. Future work: bridge to Web Audio API via JavaScript.
- **No Lua scripting** — stubbed. Not needed for the chess mod.
- **Large initial download** — `kys.data` is ~164MB. Consider lazy-loading assets or using a service worker cache.
- **Browser requirement** — JSPI requires Chrome 123+ or Edge 123+. Firefox has JSPI behind a flag (`javascript.options.wasm_js_promise_integration`). Safari does not support it yet.
