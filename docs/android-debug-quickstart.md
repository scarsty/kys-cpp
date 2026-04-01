# Android Debug Quick Start

This is the short version for getting the Android build onto an emulator and starting the app.

## One-Time Setup

Install these in Android Studio or with the SDK manager:

- Android SDK Platform 35
- Android NDK 30.0.14904198 or newer
- Android SDK CMake 4.1.2
- JDK 17

Create or confirm your `android/local.properties` points at the SDK root:

```properties
sdk.dir=D:\Android
```

If the emulator image is missing, install it once:

```powershell
$env:JAVA_HOME = "C:\Program Files\Amazon Corretto\jdk17.0.18_9"
$env:PATH = "$env:JAVA_HOME\bin;" + $env:PATH
$sdk = "D:\Android"

& "$sdk\cmdline-tools\latest\bin\sdkmanager.bat" --sdk_root=$sdk --install `
    emulator `
    platforms;android-35 `
    system-images;android-35;google_apis;x86_64
```

Create an AVD once:

```powershell
$env:JAVA_HOME = "C:\Program Files\Amazon Corretto\jdk17.0.18_9"
$env:PATH = "$env:JAVA_HOME\bin;" + $env:PATH
$sdk = "D:\Android"

@('no') | & "$sdk\cmdline-tools\latest\bin\avdmanager.bat" create avd `
    -n kys_api35 `
    -k system-images;android-35;google_apis;x86_64 `
    -d pixel `
    --force
```

Start it whenever you need it:

```powershell
& "D:\Android\emulator\emulator.exe" -avd kys_api35
```

## Build

From the repo root:

```powershell
$env:JAVA_HOME = "C:\Program Files\Amazon Corretto\jdk17.0.18_9"
cd android
.\gradlew.bat assembleRelease
```

APK output:

```text
android/app/build/outputs/apk/release/app-release.apk
```

## Install

With the emulator running:

```powershell
adb install -r android/app/build/outputs/apk/release/app-release.apk
```

If `adb` cannot find the device, check first:

```powershell
adb devices -l
```

## Launch

Start the app on the emulator:

```powershell
adb shell monkey -p com.kysgame.kyschess -c android.intent.category.LAUNCHER 1
```

## Android Studio Option

If you prefer the GUI, open the `android/` folder in Android Studio, start the `kys_api35` AVD from Device Manager, then use Run or Debug on the `app` module.

## One-Shot Script

Use [android/android-debug-quickstart.ps1](android/android-debug-quickstart.ps1) from the repo root if you want the whole flow in one command. It starts the emulator if needed, waits for boot, builds the APK, installs it, and launches the app.

If you want, save that as a `.ps1` file later and keep the markdown as the reference doc.
