# Default launcher build and window names

2026-10-19 continuation of the [compact launcher](report_launcher_redesign.md), from
Blood2 baseline `86404e8`, implemented in Blood2 commit `2322e02`. The user requested a default launcher build with the
game, a neighboring game executable by default, and the display names
**OpenBlood 2 Launcher** and **OpenBlood 2**.

## Implementation

`BLOOD2_BUILD_LAUNCHER` now defaults to ON, including in the shared game preset.
A normal Debug/dev/Release build invokes `cmake/LauncherBuild.cmake`, which
configures the same source in an isolated `build/<preset>/launcher/` tree,
builds its launcher target and installs the Launcher component beside `blood2`.
Explicit `blood2_app` builds include that dependency too. The output directory
contains `blood2`, `blood2-launcher` (with `.exe` on Windows) and `licenses/`.
The child shares compiler/toolchain, configuration and sanitizer settings, has
its own dependency binaries, and limits its nested build to two jobs. Re-entering
the incremental child build on each parent build notices source changes and
restores missing installed outputs. The independent launcher presets and
`-DBLOOD2_BUILD_LAUNCHER=OFF` game-only opt-out remain supported.

Native dialogs remain ON only in launcher-only configurations and OFF in game
configurations. HTTP/ZIP dependencies stay in the child tree. All recorded game
SDL cache entries are identical before and after configuration, in both Debug
and Release. SDL/ImGui pins, source patches, renderer code and shader artifacts
are unchanged. The only game runtime edit changes its display name/window title.
Existing protocol identifiers and user preference paths remain compatible.

Without a remembered Advanced path or explicit `--game`, the launcher resolves
`blood2[.exe]` beside itself through SDL_GetBasePath. It no longer searches an
adjacent preset directory. Missing or invalid games keep Play disabled. This
works independently of the current working directory.

The optional `gpu_registry_tests` target previously linked only `engine`, even
though legacy types in its headers reference callbacks defined in `blood2_game`.
Its link dependency now includes that existing library, matching the other
model/GPU tests. No test expectation or renderer implementation was changed.
This fixes the previously documented all-target Debug link blocker, independently
of the still-open platform reservation M2-GPU-001.

## Verification

Linux, GCC, Ninja; GPU checks use the local NVIDIA Vulkan ICD. Native Windows
and macOS execution remains unqualified. No M2/M3 acceptance, graphics-stack
migration, Nightmares support or gameplay change is included.

Builds and checks ran against the `86404e8-dirty` working implementation.

| Check | Result and raw record |
| --- | --- |
| Default all-target Debug build, including enabled GPU/private test binaries | [Passed](evidence/launcher-default-debug-build-final.log) |
| Default all-target inspector-free Release build | [Passed](evidence/launcher-default-release-build-final.log) |
| Game SDL options and isolated child configuration | [Unchanged game cache; correct child compiler/type/options](evidence/launcher-default-isolation.log) |
| Deployed executables and seven dependency notices | [Child/deployed hashes identical](evidence/launcher-default-output-audit.log) |
| Explicit game-target build after touching launcher source and removing deployed launcher | [Recompiled and restored the sibling output](evidence/launcher-default-explicit-game-restore.log) |
| Fresh game-only opt-out configuration | [Game target present; launcher/HTTP targets absent](evidence/launcher-default-optout-check.log) |
| Standalone Release launcher headless checks, including private retail/demo queries | [15/15](evidence/launcher-default-standalone-headless.log) |
| Launcher GPU: process lifecycle, default sibling/missing-game gate, navigation | [3 GPU checks plus fixture setup, 4/4](evidence/launcher-default-standalone-gpu.log) |
| Game Debug and Release config/REZ/capability checks | [11/11 Debug](evidence/launcher-default-game-debug-headless.log), [11/11 Release](evidence/launcher-default-game-release-headless.log) |
| GPU registry and original retail/demo menus | [3/3 Debug](evidence/launcher-default-game-debug-gpu.log), [3/3 Release](evidence/launcher-default-game-release-gpu.log) |
| Actual window titles, actual colocated launcher/game, unrelated working directory | [Debug](evidence/launcher-default-window-titles-debug-x11.log), [Release](evidence/launcher-default-window-titles-release-x11.log) |

The new sibling GPU test copies the launcher and a synthetic game into an
isolated distribution layout. It first places the game only in a neighboring
preset directory and checks Play remains disabled. It then adds the sibling,
drives Play/hide/return without `--game`, and checks the remembered executable.
Both runs start from another working directory. This is a subprocess fixture,
not a gameplay claim. Separate actual game/menu checks use external data; no
commercial assets are committed. Title checks read X11 window properties;
rendering uses the same NVIDIA Vulkan backend. The actual launcher screenshot
also shows validated demo data and enabled Play without an executable override.

Commands run from `Blood2/`:

```sh
cmake --preset debug
cmake --preset release
cmake --build --preset debug --parallel 4
cmake --build --preset release --parallel 6
cmake --build --preset debug --target blood2_app --parallel 4
cmake --preset launcher-release
cmake --build --preset launcher-release --parallel 2
ctest --preset launcher-release -L headless --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset launcher-release -R '^gpu.launcher_(sibling|process|navigation)$' --output-on-failure
ctest --preset debug -R '^(config\.|world\.(rez|resource)|cli.launcher_info)' --output-on-failure
ctest --preset release -R '^(config\.|world\.(rez|resource)|cli.launcher_info)' --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset debug -R '^gpu\.(registry|menus|demo_menu)$' --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset release -R '^gpu\.(registry|menus|demo_menu)$' --output-on-failure
```

The opt-out check uses a fresh ignored `build/launcher-optout-check` tree with
`BLOOD2_BUILD_LAUNCHER=OFF` and `BUILD_TESTING=OFF`, reusing already downloaded
source archives. Title/capture checks use isolated `/tmp/blood2-launcher-default`
state and XDG directories, `SDL_VIDEODRIVER=x11`, the same local ICD, and finite
180-frame actual game/launcher runs. Source formatting (clang-format 21.1.8),
CMake preset JSON, whitespace and maintained documentation links were checked.

## Retained intermediate outcomes

The [initial Debug full build](evidence/launcher-default-debug-build-first.log)
reproduced the known missing legacy callbacks in `gpu_registry_tests`; that run
failed. The corrected dependency passes the final full build and its actual GPU
test. The [initial Release build](evidence/launcher-default-release-build-first.log)
was interrupted deliberately to increase parallelism after Debug completed;
the resumed final build passes. The [initial title observation](evidence/launcher-default-window-titles-debug.log)
failed to find the default SDL window through X11 properties. The subsequent
explicit X11 runs observe both exact names. These intermediate outcomes remain
in the evidence and do not count as successful validations.
