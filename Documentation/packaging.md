# Packaging, versions and platform presets

## Version

`project(Blood2 VERSION x.y.z)` in the root `CMakeLists.txt` is the only version
number. Everything else derives from it:

| Consumer | Value |
| --- | --- |
| `build_info.h` (`--help`, logs, setup screen, saves, SDL metadata) | `x.y.z` |
| Windows `VERSIONINFO` | `x,y,z,build` |
| Apple `CFBundleShortVersionString` / Android `versionName` | `x.y.z` |
| Apple `CFBundleVersion` / Android `versionCode` | `x*1000000 + y*10000 + z*100 + build` |
| CI archives | `OpenBlood2-x.y.z-<platform>` and `BUILD.txt`; `OpenBlood2-x.y.z-<android/quest>-arm64.apk` |

`BLOOD2_BUILD_NUMBER` (cache variable, 0–99, default 0) separates rebuilds of the
same version for the stores. Minor and patch components must stay below 100.

To release: bump `project(VERSION)`, commit, then push the tag `vx.y.z` to GitHub. CI
packaging refuses a tag that does not match the project version; when every job passes,
CI publishes the GitHub release with the desktop archives and the Android/Quest APKs
([CI builds](ci_builds.md#releases)). Published releases:
`v0.1.0` (Windows pre-release); `0.5.0` is planned once the current input and
save/load work lands.

## Identifiers

Every platform uses one application identifier, `net.demonixis.openblood2` (Apple
`CFBundleIdentifier`, Android `applicationId`/`namespace` and Java package, SDL app
metadata), set in `cmake/Packaging.cmake`. An installed build with the former `org.openblood2.game`
identifier is a separate app: uninstall it before installing the new one.

## Icons

The game has a project-owned icon (no original Blood II artwork): a blood drop. One
procedural design produces every platform format:

```bash
python3 tools/icons/generate_icons.py
```

It needs only the Python standard library and rewrites, under `platform/`:
`icons/*.png` (512 px previews), `windows/*.ico`, `apple/macos/*.icns`, the iOS
`AppIcon` asset catalog (opaque full-bleed 1024 px) and the Android adaptive icon
(foreground mipmaps, background colour, monochrome layer). Regenerate them all
together; never edit an output by hand.

| Platform | Where the icon goes |
| --- | --- |
| Windows | `platform/windows/application.rc.in`, compiled into the executable (also the SDL window icon). |
| macOS | `.icns` in the `.app` bundle; bundles are built by the Xcode preset (`BLOOD2_APPLE_BUNDLE`). |
| iOS | `Assets.xcassets` compiled by Xcode (`ASSETCATALOG_COMPILER_APPICON_NAME`). |
| Android / Quest | `platform/android/app/src/main/res`. |

The Ninja macOS builds (and therefore the current CI archive) stay plain
executables without an icon; pass `-DBLOOD2_APPLE_BUNDLE=ON` to bundle them.

## Presets

| Preset | Output | Notes |
| --- | --- | --- |
| `xcode-macos` | `build/xcode-macos/Blood2.xcodeproj` → `Release/OpenBlood2.app` | Inspector-free, no tests. |
| `xcode-ios` | `build/xcode-ios/Blood2.xcodeproj` → `OpenBlood2.app` (iPhone/iPad, arm64 device) | Game only. |
| `android` | `build/android/android/gradle/app/outputs/apk/phone/` | arm64-v8a, minSdk 29, targetSdk 36. |
| `quest` | `build/quest/android/gradle/app/outputs/apk/quest/` | Immersive Meta Horizon OS OpenXR app (`BLOOD2_XR`): minSdk 32, targetSdk 34, `supportedDevices`, head tracking required, arm64 only, Khronos loader AAR. |

One Xcode project cannot hold both platforms: SDL and the dependencies select
Cocoa or UIKit while CMake configures, so macOS and iOS are two presets.

```bash
cmake --preset xcode-macos
cmake --build --preset xcode-macos   # or open the .xcodeproj
cmake --preset xcode-ios
cmake --build --preset xcode-ios
```

Signing is disabled unless `BLOOD2_APPLE_TEAM_ID` is set (for example in an ignored
`CMakeUserPresets.json`); set it to run on a device. Both Apple presets target
version 26.0, required by the floating-point `std::from_chars` calls.

Android presets need `ANDROID_SDK_ROOT` with NDK `29.0.14206865` and platform 36
installed, and a JDK 17+. The checked-in wrapper downloads the pinned Gradle 9.6.0
(checksum-verified) and the Android Gradle Plugin 9.4.1 on first use:

```bash
cmake --preset quest
cmake --build --preset quest         # builds libmain.so, then the blood2_apk target
```

CMake drives the build: the game links statically into `libmain.so`, which is
staged per ABI, and `platform/android` is a thin Gradle packager that receives the
native libraries, the pinned SDL's own Java sources (`android-project`) and the
version through `-Pblood2.*` properties. Gradle outputs stay in the CMake build
tree. The presets use RelWithDebInfo and produce debug-signed, installable APKs;
`-DCMAKE_BUILD_TYPE=Release` selects the unsigned store variant until a signing
configuration exists.

## Android phone/tablet

Use the build preset (or name the target): `blood2_apk` is not part of `all`, so
a plain `cmake --build build/android` or `ninja` only stages `libmain.so` under
`build/android/android/jniLibs` and produces no APK.

```bash
cmake --preset android
cmake --build --preset android       # or: cmake --build build/android --target blood2_apk
```

```bash
adb install -r build/android/android/gradle/app/outputs/apk/phone/debug/app-phone-debug.apk
```

A Release configuration writes the unsigned APK under `apk/phone/release/` instead.

## Quest (OpenXR)

The `quest` flavor depends on `org.khronos.openxr:openxr_loader_for_android:1.1.63`
(AAR SHA-256 `622419d2f6741c3443a3beb4779af0764318edd01830de967f24c741ebcded73`),
matching the OpenXR headers vendored in the pinned SDL. SDL loads
`libopenxr_loader.so`; the manifest overlay adds the VR launcher categories, head
tracking, the Khronos broker permissions/queries and `com.oculus.vr.focusaware`.
See the [OpenXR runtime contract](contracts/xr_runtime_contracts.md).

```bash
adb install -r build/quest/android/gradle/app/outputs/apk/quest/debug/app-quest-debug.apk
```

```bash
adb logcat -s OpenBlood2
```

On Android phones and the Quest, game data goes in the shared storage, as for other
sideloaded ports: `/sdcard/openblood2/game` for the retail game (`BLOOD2.REZ` and the rest
of the installation) and `/sdcard/openblood2/demo` for the 1998 demo (`B2Demo.rez`); any
other folder directly under `/sdcard/openblood2` is found too. Copy them over USB or with
adb, or let the [setup screen](contracts/setup_screen_contracts.md) download the demo into
the app's own storage:

```bash
adb push /path/to/Blood2/. /sdcard/openblood2/game/
```

The app needs "all files access" to read them (Android hides files placed by other apps);
the setup screen's "Allow file access" button opens the system page. During development the
permission can be granted directly:

```bash
adb shell appops set --uid net.demonixis.openblood2 MANAGE_EXTERNAL_STORAGE allow
```

Launched without arguments the APK opens the setup screen (a panel at 0.75 m) until a playable
folder is chosen, then the original title and main menu with that choice. The headset must be
worn, or its proximity sensor paused, for the session to reach FOCUSED; a Guardian dialog
blocks immersive launches until the boundary is set.

## Scope

iOS and Android phones are anticipation: they are verified to build, not to run, and no
tests run on them. Their data comes from the setup screen: the shared folders above on
Android, the app's Documents folder on iOS (Files app or Finder file sharing), or the
demo download. The Quest APK plays the game on a Quest 3 with
the original menus, HUD and Touch controllers; a worn play test is still pending. Do not use a simulator or emulator
for them without asking the user (see AGENTS.md).
