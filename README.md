# Scripture for Android

A port of the Scripture desktop app (PySide6 for Windows/macOS) to Android.
Qt Quick QML UI is **reused unchanged in spirit** (`main.qml` + a settings
panel); the Python backend (~1,700 lines) is reimplemented as a C++ Qt 6
backend that exposes the exact same `ScriptureRT` QML singletons (`App`,
`Updater`), so the desktop Python version and this project stay in sync.

The app shows a full-screen verse overlay, fetches from the ESV API (key in
Settings, shown in a password field — never bundled) or keyless bible-api.com
(WEB/KJV), reveals the verse with a typewriter effect, keeps history, 8-chip
favorites, and a curated no-repeat verse deck. Decoded faithfully from the
Windows app: 25 s fetch timeout, one plain-anchor retry only for an explicit
provider range-unsupported response, and at-most-once-per-day auto-open.

Successful passages are cached in app-private storage after strict provider
validation. The cache is atomic and bounded to 256 entries/1 MiB, uses an exact
versioned schema with a non-secret ESV key fingerprint, compacts entries older
than 30 days, and is consulted only after a transport, timeout, or transient
server failure. Auth/schema failures
never fall back, and removing an ESV key makes its cached entries inaccessible.
It never contains the API key or raw provider payload, and an explicit offline
notice is shown for a valid fallback. Plain verse text can be copied or
shared, and a 1080×1350 deterministic PNG card can be saved in app-private
storage or shared through an Android `ACTION_SEND` chooser backed by a narrow,
read-only content provider. Daily notifications use an Android alarm, request runtime permission
when required, and persist a one-per-day dedupe record; the notification uses
the last validated verse snapshot and opens the app when tapped. Unsupported
platforms and denied permission disable the feature without affecting verse
reading.

Book and topic selectors filter random draws using only the same curated 185
references. Fixed verses, favorites, history navigation, and jump-to-reference
remain direct paths. Display settings persist safe clamps for verse font size
(16–56 px), scrim opacity (0–100%), and reveal speed (0–100); reveal speed `0`
and scrim opacity `0` are explicit off settings.

## Versioning

`VERSION` is the only Android version input. It must contain `MAJOR.MINOR.PATCH`
with each component in the range 0–999 and at least one component nonzero. CMake
derives the monotonic `versionCode` as `major * 1000000 + minor * 1000 + patch`
and passes the same string as `versionName`; do not edit the manifest placeholders or CMake
properties independently.

## Project layout

```
src/
  main.cpp            bootstrap; --smoke load test; Android auto-open
  controller.cpp/.h   AppController (the `App` QML singleton)
  fetcher.cpp/.h      QNAM fetchers for api.esv.org / bible-api.com
  favorites.cpp/.h    atomic favorites.json store (QSaveFile)
  passage_cache.cpp/.h validated, bounded, atomic offline passage cache
  references.cpp/.h   verse deck, filters, range/focal parsing, rich text, URLs
  verse_card.cpp/.h   deterministic plain-text/PNG card rendering and storage
  android_notifications.cpp/.h  Android alarm/permission bridge
  android_share.cpp/.h          Android ACTION_SEND bridge
  secrets.cpp/.h      Android Keystore/file-backed API key storage
  updater.cpp/.h      stub `Updater` singleton (no self-update on Android)
  qml/main.qml        verse overlay + embedded settings layers
  qml/ScriptureSettings.qml   settings form (was a separate desktop window)
android/
  AndroidManifest.xml network/notification permissions, Qt6 activity, launcher icon
  res/                 launcher icons (legacy mipmaps + adaptive foreground)
  res/values/libs.xml  androiddeployqt fills the Qt lib list at build time
  res/xml/             backup exclusions for the device-keystore setting
  src/                 Keystore, notification, alarm, and share-provider bridges
tests/
  test_core.cpp        native invariants, storage, cache, and card tests
  test_qml.cpp         headless QML resource/settings test
tools/
  make_android_icons.cpp  regenerates the Android launcher icons from the
                          same gold-cross-on-navy art as the Windows/macOS
                          icons (see ../../../scripture-windows/scripts/)
```

## Desktop build (quick local verification)

```sh
cmake -S . -B build -GNinja -DBUILD_TESTING=ON
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/scriptureandroid --smoke   # exits 0
```

`--smoke` mirrors the Python app's headless test: loads `main.qml`, opens the
overlay and settings panel, then quits.

## Android APK build

Prerequisites (one-time):

1. **Qt 6.9.0 through 6.11.x for Android** — CMake rejects other Qt 6 ranges;
   use the Qt online installer's `Android` kit or `aqtinstall`:

   ```sh
    python -m pip install -r requirements-android.txt

   aqt install-qt linux desktop 6.9.0 linux_gcc_64        # host tools
   aqt install-qt linux android 6.9.0 android_arm64_v8a   # cross kit
   ```

2. **JDK 17+**, **Android SDK** (platform-tools, build-tools, `platforms`),
   **Android NDK 27.2.12479018** (Clang 17.0.2), `patchelf`, `llvm-readelf`
   or `readelf`, `strings`, and `gradle` in PATH. Set:
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

> **HTTPS needs OpenSSL.** The Android Qt kit does not bundle OpenSSL.
> Build each packaged ABI with the pinned source, NDK, API level, and flags:
>
> ```sh
> ANDROID_NDK_ROOT=/path/to/ndk/27.2.12479018 \
>   tools/build_android_openssl.sh arm64-v8a
> ```
>
> The helper verifies the source archive, rejects a different NDK, fixes the
> library names/dependencies, and checks ABI, ELF machine, and the embedded
> OpenSSL version. `CMakeLists.txt` repeats those library checks. Equivalent
> manual commands are:
>
> ```sh
> OPENSSL_VERSION=3.0.22
> OPENSSL_SHA256=67ebca7e50d17383028045486653492195b83db95f8558709701bb47b5c1ef81
> repo_root="$(pwd)"
> curl --fail --location --output "openssl-$OPENSSL_VERSION.tar.gz" \
>   "https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz"
> printf '%s  %s\n' "$OPENSSL_SHA256" "openssl-$OPENSSL_VERSION.tar.gz" | sha256sum -c -
> tar xzf "openssl-$OPENSSL_VERSION.tar.gz"
> cd "openssl-$OPENSSL_VERSION"
> ./Configure shared android-arm64 -D__ANDROID_API__=28 no-asm no-tests
> make -j"$(nproc)" SHLIB_VERSION_NUMBER= build_libs
> mkdir -p "$repo_root/android/libs/arm64-v8a"
> cp libcrypto.so "$repo_root/android/libs/arm64-v8a/libcrypto_3.so"
> cp libssl.so "$repo_root/android/libs/arm64-v8a/libssl_3.so"
> patchelf --set-soname libcrypto_3.so "$repo_root/android/libs/arm64-v8a/libcrypto_3.so"
> patchelf --set-soname libssl_3.so "$repo_root/android/libs/arm64-v8a/libssl_3.so"
> patchelf --replace-needed libcrypto.so libcrypto_3.so "$repo_root/android/libs/arm64-v8a/libssl_3.so"
> ```
>
> `CMakeLists.txt` passes the `_3` library names to Qt and
> `main.cpp` sets `ANDROID_OPENSSL_SUFFIX=_3` before Qt Network is used. Build
> every ABI from the same OpenSSL 3.0.22 source with NDK 27.2.12479018 and API
>    level 28; do not copy host libraries or mix source/toolchain outputs.

The Android CI job pins Qt `6.9.0`, NDK `27.2.12479018`, and Android API
`36`; it builds OpenSSL, runs the manifest/Java/native Android build, and checks
the APK contents. A local machine without that exact NDK cannot complete the
Android package build and should use the pinned CI job for Android coverage.


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
- The ESV API key is entered in Settings at runtime and stored with the
  Android Keystore; the manifest disables backup and cleartext traffic. A
  desktop build uses an app-private file with owner-only permissions, while
  Windows uses DPAPI.
- ESV responses retain provider copyright/attribution only in validated
  cache/provider state. It is never rendered, copied, shared, included in a
  card, or placed in a notification body; user-facing output contains the
  passage, reference, and compact translation name. Legacy notification
  snapshots are compacted before display as well. The key is never written to
  logs, cache data, cards, notifications, or release metadata.
- Android notification permission is requested only when the user enables a
  daily time. Runtime denial, app-level blocking, and channel disabling are
  reported separately with Android Settings guidance; a denied prompt is not
  repeatedly shown. Alarms use the explicit daily action and re-arm after
  time/timezone changes and package replacement.
- Android 16 predictive Back is explicitly opted out in the manifest so the
  current Qt `Keys.onBackPressed` handling remains the active Back path.
- Card sharing uses a private, read-only `content://` provider restricted to
  generated PNGs; it does not expose arbitrary app files.

## Tests

`scripture_core_tests` covers reference/filter invariants, fetcher retry rules,
favorites, secure storage, cache validation/bounds, key generations, and
removal of provider legal text. `scripture_manifest_tests` checks Android
manifest/bridge contracts. `scripture_qml_tests` loads the real QML resource
tree headlessly and opens the embedded settings panel. All three run in CI with
the offscreen Qt platform plugin.

## Notes

- The verse deck, parsing, and text-composition rules mirror
  `references.py` (and the Omarchy `Scripture.js`); keep them in sync when
  either changes.
- Fonts: "Segoe UI" is a desktop font; Android falls back to the system font —
  the layout still auto-scales the content cluster to fit any screen.