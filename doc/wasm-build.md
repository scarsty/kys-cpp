# Building kys-cpp for WebAssembly (WASM)

This document describes how to build kys-cpp as a browser-playable WebAssembly application.

## Overview

The WASM build uses Emscripten to compile the C++ codebase to WebAssembly. Key differences from the native build:

- **Audio**: Uses SDL3_mixer (prerelease-3.1.2) + FluidSynth 2.5.3 built from source. Supports WAV, MP3, and MIDI (FluidSynth with sf2 soundfont).
- **Scripting**: Lua is stubbed (unused in the chess mod).
- **Networking**: Disabled (`WITH_NETWORK=OFF` equivalent — no network code compiled).
- **Threading**: Uses `-pthread` with `PROXY_TO_PTHREAD` — `main()` runs on a real worker thread, enabling standard `std::this_thread::sleep_for`.
- **Filesystem**: Uses WasmFS with fetch backend (on-demand asset loading from HTTP) and OPFS backend (persistent saves).
- **Game assets**: Served as static files and fetched on demand via WasmFS fetch backend — no monolithic `.data` file.

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
vcpkg install glaze:wasm32-emscripten
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
    -DCMAKE_C_FLAGS="-pthread" \
    -DCMAKE_CXX_FLAGS="-pthread" \
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
    -DCMAKE_C_FLAGS="-pthread" \
    -DCMAKE_CXX_FLAGS="-pthread" \
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

## Step 5: Build FluidSynth from source

FluidSynth 2.5.3+ supports a `cpp11` OS abstraction layer that eliminates the glib dependency, making it viable for WASM.

```bash
cd "$DEPS_SRC"
git clone --depth 1 https://github.com/FluidSynth/fluidsynth.git
cd fluidsynth
mkdir build_wasm && cd build_wasm

emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$VCPKG_WASM" \
    -DBUILD_SHARED_LIBS=OFF \
    -Dosal=cpp11 \
    -Denable-libinstpatch=0 \
    -DCMAKE_C_FLAGS="-fexceptions -pthread" \
    -DCMAKE_CXX_FLAGS="-fexceptions -pthread" \
    -G Ninja

emmake ninja libfluidsynth
```

Install manually:

```bash
cp src/libfluidsynth.a "$VCPKG_WASM/lib/"
cp -r ../include/fluidsynth "$VCPKG_WASM/include/"
cp include/fluidsynth/version.h "$VCPKG_WASM/include/fluidsynth/"
cp include/fluidsynth.h "$VCPKG_WASM/include/"
```

### Required: `-fexceptions` compile flag

FluidSynth's `src/utils/fluid_file.cpp` and `src/utils/fluid_sys_cpp11.cpp` use `std::filesystem` with `try/catch` blocks. Emscripten defaults to `-fno-exceptions`, which strips the `catch` handlers — any `std::filesystem` exception (e.g. from `exists()`, `is_regular_file()`, `last_write_time()`) will abort at runtime with `filesystem_error was thrown in -fno-exceptions mode`. The `-DCMAKE_C_FLAGS="-fexceptions" -DCMAKE_CXX_FLAGS="-fexceptions"` flags in the cmake command above enable exception support. The kys-cpp link flags must also include `-fexceptions` so the linker includes Emscripten's exception handling runtime.

## Step 6: Build SDL3_mixer from source

SDL3_mixer `prerelease-3.1.2` with FluidSynth for MIDI playback via sf2 soundfont.

```bash
cd "$DEPS_SRC"
git clone --branch prerelease-3.1.2 --depth 1 https://github.com/libsdl-org/SDL_mixer.git
cd SDL_mixer
mkdir build_wasm && cd build_wasm

emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$VCPKG_WASM" \
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
    -DCMAKE_FIND_ROOT_PATH="$VCPKG_WASM" \
    -G Ninja

emmake ninja
emmake ninja install
```

### Required patches for SDL3_mixer

Emscripten 5.0.1 bundles SDL 3.2.30, which is missing some newer APIs that SDL3_mixer `prerelease-3.1.2` expects. Additionally, the FluidSynth option is gated behind `NOT SDLMIXER_VENDORED` which must be patched.

**1. `CMakeLists.txt`** — Allow FluidSynth when vendored mode is ON:

```cmake
# Line 152: remove "NOT SDLMIXER_VENDORED" from the condition
cmake_dependent_option(SDLMIXER_MIDI_FLUIDSYNTH "Support FluidSynth MIDI output" ON "SDLMIXER_MIDI" OFF)

# Lines 892-895: remove the vendored guard that errors out, so it falls through to find_package
```

**2. `src/SDL_mixer_internal.h`** — Add compat shims after `#include <SDL3/SDL_intrin.h>`:

```c
/* Compat shim for older SDL3 (e.g. Emscripten's bundled 3.2.30) */
#ifndef SDL_ALIGNED
#  ifdef __GNUC__
#    define SDL_ALIGNED(x) __attribute__((aligned(x)))
#  elif defined(_MSC_VER)
#    define SDL_ALIGNED(x) __declspec(align(x))
#  else
#    define SDL_ALIGNED(x)
#  endif
#endif

#ifndef SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN
#  define SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN "SDL.audiostream.auto_cleanup"
#endif

/* Compat: SDL_PutAudioStreamDataNoCopy added after SDL 3.2.30 */
#ifndef SDL_PutAudioStreamDataNoCopy
static SDL_INLINE bool SDL_PutAudioStreamDataNoCopy(SDL_AudioStream *stream,
    const void *buf, int len, void *userdata,
    void (*callback)(void *, const void *, int)) {
    (void)userdata; (void)callback;
    return SDL_PutAudioStreamData(stream, buf, len);
}
#endif

/* Compat: SDL_PutAudioStreamPlanarData added after SDL 3.2.30 */
#ifndef SDL_PutAudioStreamPlanarData
static SDL_INLINE bool SDL_PutAudioStreamPlanarData(SDL_AudioStream *stream,
    const void * const *channel_data, int num_channels, int num_frames) {
    (void)stream; (void)channel_data; (void)num_channels; (void)num_frames;
    return false;  /* stub - only used by disabled decoders */
}
#endif
```

## Step 7: Generate asset manifest

The WasmFS fetch backend requires all files to be pre-registered before they can be accessed. A Python script generates a C++ include file with `wasmfs_create_directory`/`wasmfs_create_file` calls for every game asset:

```bash
cd /d/projects/kys-cpp/kys-cpp
python wasm/gen_manifest.py work/game-dev > wasm/wasm_manifest.inc
```

This must be re-run whenever game assets are added or removed.

## Step 8: Build kys-cpp for WASM

```bash
cd /d/projects/kys-cpp/kys-cpp/wasm
mkdir -p build && cd build

emcmake cmake .. \
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
| `kyschess.html` | HTML shell page |
| `kyschess.js` | Emscripten runtime |
| `kyschess.wasm` | Compiled binary |
| `kyschess.worker.js` | Web Worker script (for pthreads) |

No `.data` file — game assets are fetched on demand from the server.

## Step 9: Run in browser

The files must be served over HTTP with COOP/COEP headers (required for `SharedArrayBuffer` which pthreads needs). A helper script is provided:

```bash
cd /d/projects/kys-cpp/kys-cpp/wasm/build
python ../serve.py 8080
```

The `game/` directory must be accessible relative to the HTML file. Create a symlink if needed:

```bash
ln -s /d/projects/kys-cpp/kys-cpp/work/game-dev /d/projects/kys-cpp/kys-cpp/wasm/build/game
```

Open `http://localhost:8080/kyschess.html` in Chrome 102+ or Edge 102+ (required for SharedArrayBuffer + OPFS).

## Step 10: Deploy to a server with nginx

This section covers deploying the WASM build to a server running nginx (e.g. inside a Docker container).

### MIME type for `.wasm`

nginx 1.19+ includes `application/wasm` in its default `mime.types`. For older versions, add it manually inside the `types { }` block in `/etc/nginx/mime.types`:

```
application/wasm                wasm;
```

### nginx location block

Add a location block to your server config (e.g. `/etc/nginx/sites-enabled/default`). The COOP/COEP headers are required for `SharedArrayBuffer` (pthreads):

```nginx
location /kys/ {
    alias /var/www/html/kys/;
    gzip_static on;
    expires 7d;
    add_header Cache-Control "public, immutable";
    add_header Cross-Origin-Opener-Policy "same-origin";
    add_header Cross-Origin-Embedder-Policy "require-corp";
}
```

`gzip_static on` tells nginx to serve pre-compressed `.gz` files when available, avoiding on-the-fly compression overhead.

### Enable gzip for WASM content

In `/etc/nginx/nginx.conf`, uncomment or add `gzip_types` to include `application/wasm`:

```nginx
gzip_types text/plain text/css application/json application/javascript text/xml application/xml application/xml+rss text/javascript application/wasm;
```

### Upload build files

Copy the build output files and game assets to the server:

```bash
# From local machine — upload build files to server
scp wasm/build/kyschess.html wasm/build/kyschess.js \
    wasm/build/kyschess.wasm wasm/build/kyschess.worker.js \
    user@server:/tmp/

# On the server — copy into nginx root (adjust container ID as needed)
docker exec $(docker ps -q) mkdir -p /var/www/html/kys
docker cp /tmp/kyschess.html $(docker ps -q):/var/www/html/kys/
docker cp /tmp/kyschess.js $(docker ps -q):/var/www/html/kys/
docker cp /tmp/kyschess.wasm $(docker ps -q):/var/www/html/kys/
docker cp /tmp/kyschess.worker.js $(docker ps -q):/var/www/html/kys/

# Upload game assets — these are served on demand by the fetch backend
docker cp work/game-dev $(docker ps -q):/var/www/html/kys/game
```

### Pre-compress for faster downloads

Pre-compressing the `.wasm` file avoids on-the-fly gzip overhead and pairs with `gzip_static on`:

```bash
docker exec $(docker ps -q) bash -c "gzip -kf /var/www/html/kys/kyschess.wasm"
```

### Test and reload nginx

```bash
docker exec $(docker ps -q) nginx -t
docker exec $(docker ps -q) nginx -s reload
```

The app will be available at `https://your-domain/kys/kyschess.html`.

## Source code changes for WASM

All changes are guarded with `#ifdef __EMSCRIPTEN__` / `#ifndef __EMSCRIPTEN__` so they don't affect the native build.

### `src/Audio.h` / `src/Audio.cpp`
- Uses SDL3_mixer (same code path as native non-BASS build)
- MIDI files use FluidSynth decoder with `mid.sf2` soundfont
- WAV and MP3 playback via SDL3_mixer's vendored decoders

### `src/Script.h` / `src/Script.cpp`
- Full stub class with all methods inlined as no-ops (no Lua dependency)

### `src/kys.cpp`
- `mount_wasmfs_backends()` called from `main()` (which runs on a worker thread via `PROXY_TO_PTHREAD`):
  - Creates WasmFS fetch backend and mounts at `/game` for on-demand HTTP asset loading
  - Pre-registers all game asset directories and files from `wasm/wasm_manifest.inc` (generated by `wasm/gen_manifest.py`) so the fetch backend knows they exist
  - Creates WasmFS OPFS backend and mounts at `/persist` for persistent saves
- Both backends must be created on a worker thread (not the main browser thread), which is why they are in `main()` rather than `wasmfs_before_preload()`

### `src/Save.cpp`
- Save files written to `/persist/` (OPFS-backed, persists automatically — no `FS.syncfs()` needed)

### `src/TitleScene.cpp`
- Auto-save detection uses direct `filefunc::fileExist("/persist/4.db")` instead of JS `Module.hasAutoSave`

### `src/Engine.h`
- `Engine::delay()` uses `std::this_thread::sleep_for` unconditionally (works because `PROXY_TO_PTHREAD` runs `main()` on a real thread)

### `src/Engine.cpp`
- `renderPresent()`: skip `SDL_RenderClear` after present on WASM (prevents frame flashing)
- `renderMainTextureToWindow()`: clear before draw instead on WASM
- Sets `SDL_HINT_EMSCRIPTEN_ASYNCIFY` to `"0"` before `SDL_Init` — SDL3 defaults to calling `emscripten_sleep()` during `SDL_RenderPresent` for vsync, but we use pthreads instead of ASYNCIFY

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

### `wasm/gen_manifest.py`
- Script that walks the game asset directory and generates `wasm/wasm_manifest.inc` — a C++ include file with `wasmfs_create_directory`/`wasmfs_create_file` calls for every asset

### `wasm/shell.html`
- Custom HTML shell with loading overlay, right-click button for touch devices, and fullscreen support
- Sets `Module.canvas` and COOP/COEP-compatible configuration

## Known limitations

- **All dependencies must be built with `-pthread`** — the `-pthread` flag (which enables `atomics` and `bulk-memory` features) must be passed to all dependency builds. Without it, `wasm-ld` will error with `--shared-memory is disallowed because it was not compiled with 'atomics' or 'bulk-memory' features`.
- **Asset manifest** — `wasm/wasm_manifest.inc` must be regenerated (via `wasm/gen_manifest.py`) whenever game assets are added or removed. The WasmFS fetch backend cannot auto-discover files from the server.
- **Audio** — supported via SDL3_mixer (prerelease-3.1.2) with FluidSynth 2.5.3 for MIDI (using `mid.sf2` soundfont), plus WAV and MP3 (vendored drmp3) decoders. Music looping (`MIX_PROP_PLAY_LOOPS_NUMBER`) may not work correctly in this prerelease.
- **No Lua scripting** — stubbed. Not needed for the chess mod.
- **WasmFS** — marked experimental in Emscripten. Core functionality is well-tested but edge cases may exist.
- **Browser requirement** — `SharedArrayBuffer` (pthreads) requires COOP/COEP headers. OPFS requires Chrome 102+, Edge 102+, Firefox 111+. Safari has partial/no OPFS support.
- **COOP/COEP headers** — all resources on the page must be same-origin or have CORS headers. Cross-origin iframes without proper headers will not work.
