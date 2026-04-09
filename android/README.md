# Android Build Guide

## Prerequisites

- **Android SDK** (platform android-35 or later) — e.g. via Android Studio
- **Android NDK** r30+ (tested with 30.0.14904198)
- **Android SDK CMake** 4.1.2 (install via SDK Manager → SDK Tools)
- **JDK 17** (e.g. Amazon Corretto: `winget install Amazon.Corretto.17`)
- **vcpkg** — clone from https://github.com/microsoft/vcpkg

## One-Time Setup

### 1. Install vcpkg dependencies (arm64-android)

```powershell
$env:ANDROID_NDK_HOME = "D:\Android\ndk\30.0.14904198"   # adjust path
$env:ANDROID_NDK = $env:ANDROID_NDK_HOME

vcpkg install --triplet=arm64-android `
    --overlay-triplets=android/triplets `
    --x-manifest-root=android `
    --x-install-root=android/vcpkg_installed
```

The custom triplet at `android/triplets/arm64-android.cmake` sets `VCPKG_CMAKE_SYSTEM_VERSION 21` (minSdk 21).

### 2. Copy SDL3 Java sources

SDL3 requires its Java shim classes. Copy them from the vcpkg build tree:

```powershell
$sdlSrc = "D:\projects\vcpkg\buildtrees\sdl3\src\*\android-project\app\src\main\java\org\libsdl\app"
# Resolve the version-hashed directory
$sdlSrc = (Resolve-Path $sdlSrc).Path
$dest = "android\app\src\main\java\org\libsdl\app"
New-Item -ItemType Directory -Path $dest -Force | Out-Null
Copy-Item "$sdlSrc\*" $dest -Recurse -Force
```

### 3. Create local.properties

```
sdk.dir=D:\\Android
```

### 4. Generate debug keystore (for sideloading)

```powershell
keytool -genkeypair -v -keystore android/debug.keystore `
    -alias debug -keyalg RSA -keysize 2048 -validity 36500 `
    -storepass android -keypass android -dname "CN=Debug,O=KysGame,C=US"
```

### 5. Launcher icon generation

The Android build now regenerates the launcher mipmaps from `android/kys_chess_icon.png` before `preBuild` runs, and the app uses an adaptive launcher icon on Android 8+ so the launcher can apply rounded masks correctly. The generator writes:

- `app/src/main/res/mipmap-mdpi/ic_launcher.png` at 48x48
- `app/src/main/res/mipmap-hdpi/ic_launcher.png` at 72x72
- `app/src/main/res/mipmap-xhdpi/ic_launcher.png` at 96x96
- `app/src/main/res/mipmap-xxhdpi/ic_launcher.png` at 144x144
- `app/src/main/res/mipmap-xxxhdpi/ic_launcher.png` at 192x192
- `app/src/main/res/drawable/ic_launcher_foreground.png` for the adaptive icon foreground

If you want to refresh them manually, run `android/generate_launcher_icons.ps1`.

## Building

```powershell
$env:JAVA_HOME = "C:\Program Files\Amazon Corretto\jdk17.0.18_9"
cd android
.\gradlew.bat assembleRelease
```

Output APK: `app/build/outputs/apk/release/app-release.apk`

The build automatically copies game assets from `../work/game-dev/` into the APK via `copy_assets.ps1`.

## Installing on Device

```powershell
adb install app/build/outputs/apk/release/app-release.apk
```

## Gotchas & Known Issues

### CMake find_package fails

The NDK toolchain sets `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY`, which ignores `CMAKE_PREFIX_PATH`. The Android CMakeLists.txt works around this by adding the vcpkg install dir to `CMAKE_FIND_ROOT_PATH`.

### CMake 4.x FindWebP casing (CMP0175)

CMake 4.x enforces that `find_package_handle_standard_args()` name matches `find_package()` casing. The vcpkg-shipped `FindWebP.cmake` has `webp` lowercase — it must be patched to `WebP` after each `vcpkg install`:

```powershell
$f = "android/vcpkg_installed/arm64-android/share/SDL3_image/FindWebP.cmake"
(Get-Content $f) `
    -replace 'find_package_handle_standard_args\(webp', 'find_package_handle_standard_args(WebP' `
    -replace 'if \(webp_FOUND\)', 'if (WebP_FOUND)' |
    Set-Content $f
```

### No Lua on Android

Lua scripting is stubbed out on Android (same as WASM) via `#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)` guards in `Script.h`/`Script.cpp`.

### No iconv on Android (minSdk < 28)

Bionic libc provides `iconv` only from API 28+. Since all game data is already UTF-8, `PotConv` is stubbed as a no-op on Android (all conversion methods return input unchanged). This allows minSdk 21.

### OpenSSL required by libzip

libzip's crypto backend needs OpenSSL — `crypto` and `ssl` are linked in CMakeLists.txt.

### Asset extraction on first launch

Game assets are bundled in the APK under `assets/game/` and extracted to internal storage (`getFilesDir()/game/`) on first launch. This is handled by `KysActivity.java`. Re-extraction triggers when the APK `versionCode` changes.

## Project Structure

```
android/
├── build.gradle              # root project (AGP 8.7.0)
├── settings.gradle
├── gradle.properties          # AndroidX, 4GB heap
├── local.properties           # sdk.dir (not committed)
├── debug.keystore             # signing key (not committed)
├── copy_assets.ps1            # asset copy + save cleanup
├── vcpkg.json                 # arm64-android dependencies
├── triplets/
│   └── arm64-android.cmake    # custom triplet (minSdk 21)
├── vcpkg_installed/           # vcpkg output (not committed)
└── app/
    ├── build.gradle           # module config
    └── src/main/
        ├── AndroidManifest.xml
        ├── cpp/CMakeLists.txt # NDK build
        ├── java/com/kysgame/kyschess/KysActivity.java
        ├── java/org/libsdl/app/   # SDL3 Java (not committed)
        ├── assets/game/           # game data (not committed)
        └── res/                   # icons, strings
```
