# Scripture for Android

A port of the Scripture desktop app (PySide6 for Windows/macOS) to Android.
Qt Quick QML UI is **reused unchanged in spirit** (`main.qml` + a settings
panel); the Python backend (~1,700 lines) is reimplemented as a C++ Qt 6
backend that exposes the exact same `ScriptureRT` QML singletons (`App`,
`Updater`), so the desktop Python version and this project stay in sync.

The app shows a full-screen verse overlay, fetches from the ESV API (key in
Settings, shown in a password field — never bundled) or keyless bible-api.com
(WEB/KJV), reveals the verse with a typewriter effect, keeps history, 8-chip
favorites, and a curricular no-repeat verse deck. Decoded faithfully from the
Windows app: 25 s fetch timeout, single plain-anchor retry per fetch, at-most
once-per-day auto-open.

## Project layout

```
src/
  main.cpp            bootstrap; --smoke load test; Android auto-open
  controller.cpp/.h   AppController (the `App` QML singleton)
  fetcher.cpp/.h      QNAM fetchers for api.esv.org / bible-api.com
  favorites.cpp/.h    atomic favorites.json store (QSaveFile)
  references.cpp/.h   verse deck, range/focal parsing, rich text, URLs
  updater.cpp/.h      stub `Updater` singleton (no self-update on Android)
  qml/main.qml        verse overlay + embedded settings layers
  qml/ScriptureSettings.qml   settings form (was a separate desktop window)
android/
  AndroidManifest.xml INTERNET permission, Qt6 activity template, launcher icon
  res/                 launcher icons (legacy mipmaps + adaptive foreground)
  res/values/libs.xml  androiddeployqt fills the Qt lib list at build time
tools/
  make_android_icons.cpp  regenerates the Android launcher icons from the
                          same gold-cross-on-navy art as the Windows/macOS
                          icons (see ../../../scripture-windows/scripts/)
```

## Desktop build (quick local verification)

```sh
cmake -S . -B build -GNinja
cmake --build build
QT_QPA_PLATFORM=offscreen ./build/scriptureandroid --smoke   # exits 0
```

`--smoke` mirrors the Python app's headless test: loads `main.qml`, opens the
overlay and settings panel, then quits.

## Android APK build

Prerequisites (one-time):

1. **Qt 6.x for Android** — either the Qt online installer's `Android` kit or
   via `aqtinstall`:

   ```sh
   pip install aqtinstall
   aqt install-qt linux desktop 6.8.0 linux_gcc_64        # host tools
   aqt install-qt linux android 6.8.0 android_arm64_v8a   # cross kit
   ```

2. **JDK 17+**, **Android SDK** (platform-tools, build-tools, `platforms`),
   **Android NDK** (the version the chosen Qt release pins, e.g. r28 for
   Qt 6.11), and `gradle` in PATH. Set:
   `export ANDROID_SDK_ROOT=...  export ANDROID_NDK_ROOT=...`

3. Configure with Qt's Android toolchain:

   ```sh
   cmake -S . -B build-android -GNinja \
     -DCMAKE_TOOLCHAIN_FILE=<qt>/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
     -DQT_HOST_PATH=<qt>/gcc_64 \
     -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-android
   ```

   The build runs `androiddeployqt`/gradle and produces a **debug-signed** APK
   at `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk`
   (a Release build yields an *unsigned* `.../release/android-build-release-unsigned.apk`,
   which needs `apksigner` before it will install).

4. Install on a phone (debug signing is automatic):

   ```sh
   adb install -r build-android/android-build/build/outputs/apk/debug/android-build-debug.apk
   ```

> **AGP 9 notes:** the manifest must **not** declare `<uses-sdk>` or
> `android:extractNativeLibs` — Android Gradle Plugin 9 rejects both. min/
> target SDK come from the `QT_ANDROID_MIN_SDK_VERSION` /
> `QT_ANDROID_TARGET_SDK_VERSION` CMake properties (min 28 for Qt 6.11).
> androiddeployqt warns that the runtime `ScriptureRT` singleton import can't
> be resolved at package time — that is expected and harmless (the singletons
> are registered in `main.cpp` before the engine loads).

## Launcher icon

Rendered from the exact desktop art (`make_icon.py`'s navy `#0d1b2a` rounded
square + gold `#f5c542` Latin cross). Generated PNGs are committed under
`android/res/`; regenerate with the one-off tool:

```sh
cmake -S tools -B tools/build -GNinja -DCMAKE_PREFIX_PATH=<system-qt-prefix>
cmake --build tools/build
./tools/build/make_android_icons android/res
```

The output covers: legacy launcher tiles at all densities, an adaptive
foreground (gold cross kept inside the 66dp safe zone so it survives any
launcher mask), a white monochrome layer (Android 13+ themed icons), and the
`anydpi-v26` adaptive definitions in `res/mipmap-anydpi-v26/`.

## Deliberate Android differences from the desktop app

- **No self-update.** Play Store / sideloading own updates; the `Updater`
  singleton always reports "no update" so the update chip never appears.
- **No system tray.** The app opens straight to a full-screen verse.
- **Settings is embedded** in the single window (Android has one Activity /
  window; the desktop hosted it as a second QQuickView window).
- `Esc` is the Android **Back** key; the Close button and a bare-scrim tap
  also dismiss the overlay.
- The ESV API key is entered in Settings at runtime, same as desktop.

## Notes

- The verse deck, parsing, and text-composition rules mirror
  `references.py` (and the Omarchy `Scripture.js`); keep them in sync when
  either changes.
- Fonts: "Segoe UI" is a desktop font; Android falls back to the system font —
  the layout still auto-scales the content cluster to fit any screen.