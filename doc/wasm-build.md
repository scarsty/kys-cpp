# Building kys-cpp for WebAssembly (WASM)

This document describes how to build kys-cpp as a browser-playable WebAssembly application.

## Overview

The WASM build uses Emscripten to compile the C++ codebase to WebAssembly. Key differences from the native build:

- **Audio**: Uses SDL3_mixer (prerelease-3.1.2) + FluidSynth 2.5.3 built from source. Supports WAV, MP3, and MIDI (FluidSynth with sf2 soundfont).
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
    -DCMAKE_C_FLAGS="-fexceptions" \
    -DCMAKE_CXX_FLAGS="-fexceptions" \
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

### Required patch: `pthread_setschedparam`

FluidSynth's `src/utils/fluid_sys.c` calls `pthread_setschedparam` for thread priority, which Emscripten doesn't provide. Wrap the call in `#ifdef __EMSCRIPTEN__` to stub it out:

```c
// In fluid_thread_self_set_prio(), add at the top of the function body:
#ifdef __EMSCRIPTEN__
    (void)prio_level;
    return;
}
#else
    // ... original code ...
#endif /* !__EMSCRIPTEN__ */
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

## Step 7: Build kys-cpp for WASM

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
| `kyschess.html` | HTML shell page |
| `kyschess.js` | Emscripten runtime (~350KB) |
| `kyschess.wasm` | Compiled binary (~6.5MB) |
| `kyschess.data` | Preloaded game assets (including music/sound) |

## Step 8: Run in browser

The files must be served over HTTP (browsers block `file://` for WASM):

```bash
cd /d/projects/kys-cpp/kys-cpp/wasm/build
python -m http.server 8080
```

Open `http://localhost:8080/kyschess.html` in Chrome 123+ or Edge 123+ (required for JSPI support).

## Step 9: Deploy to a server with nginx

This section covers deploying the WASM build to a server running nginx (e.g. inside a Docker container).

### MIME type for `.wasm`

nginx 1.19+ includes `application/wasm` in its default `mime.types`. For older versions, add it manually inside the `types { }` block in `/etc/nginx/mime.types`:

```
application/wasm                wasm;
```

### nginx location block

Add a location block to your server config (e.g. `/etc/nginx/sites-enabled/default`):

```nginx
location /kys/ {
    alias /var/www/html/kys/;
    gzip_static on;
    expires 7d;
    add_header Cache-Control "public, immutable";
}
```

`gzip_static on` tells nginx to serve pre-compressed `.gz` files when available, avoiding on-the-fly compression overhead.

### Enable gzip for WASM content

In `/etc/nginx/nginx.conf`, uncomment or add `gzip_types` to include `application/wasm`:

```nginx
gzip_types text/plain text/css application/json application/javascript text/xml application/xml application/xml+rss text/javascript application/wasm;
```

### Upload build files

Copy the four build output files to the server, then into the nginx document root:

```bash
# From local machine — upload to server
scp wasm/build/kyschess.html wasm/build/kyschess.js \
    wasm/build/kyschess.wasm wasm/build/kyschess.data \
    user@server:/tmp/

# On the server — copy into nginx root (adjust container ID as needed)
docker exec $(docker ps -q) mkdir -p /var/www/html/kys
docker cp /tmp/kyschess.html $(docker ps -q):/var/www/html/kys/
docker cp /tmp/kyschess.js $(docker ps -q):/var/www/html/kys/
docker cp /tmp/kyschess.wasm $(docker ps -q):/var/www/html/kys/
docker cp /tmp/kyschess.data $(docker ps -q):/var/www/html/kys/
```

### Pre-compress for faster downloads

The `.data` and `.wasm` files are large. Pre-compressing them avoids on-the-fly gzip overhead and pairs with `gzip_static on`:

```bash
docker exec $(docker ps -q) bash -c "gzip -kf /var/www/html/kys/kyschess.data"
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

- **Audio** — supported via SDL3_mixer (prerelease-3.1.2) with FluidSynth 2.5.3 for MIDI (using `mid.sf2` soundfont), plus WAV and MP3 (vendored drmp3) decoders. Music looping (`MIX_PROP_PLAY_LOOPS_NUMBER`) may not work correctly in this prerelease.
- **No Lua scripting** — stubbed. Not needed for the chess mod.
- **Large initial download** — `kys.data` includes music/sound assets. Consider lazy-loading or using a service worker cache.
- **Browser requirement** — JSPI requires Chrome 123+ or Edge 123+. Firefox has JSPI behind a flag (`javascript.options.wasm_js_promise_integration`). Safari does not support it yet.
