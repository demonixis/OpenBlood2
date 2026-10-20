# Unused SDL build options

Date: 2026-10-19. Scope: the user's request to disable safe, unused SDL features.
Implementation baseline: Blood2 `39c3af6`; pinned SDL remains
`6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1`.
Implementation commit: `9e8a02d`.

## Change

[Dependencies.cmake](../../../cmake/Dependencies.cmake) forces six options off
before FetchContent configures SDL: `SDL_RENDER`, `SDL_CAMERA`, `SDL_DIALOG`,
`SDL_TRAY`, `SDL_NOTIFICATION` and `SDL_DISKAUDIO`. The game, HUD, menus and
inspector use SDL_GPU directly. No source gameplay or dependency revision changes.

GPU/OpenXR, controllers, haptics, sensors, power reporting, platform audio/video
backends and dummy test drivers retain their previous settings. In particular,
dummy audio is required by existing tests. The dev preset was also reconfigured;
its executable was not rebuilt in this run.

OpenGL/OpenGL ES were initially disabled as candidates. That attempted Debug build
failed in `SDL_kmsdrmvideo.c` because cached EGL detection kept KMSDRM enabled while
EGL support was disabled. SDL's CheckEGL requires OpenGL or OpenGL ES, and CheckKMSDRM
requires EGL. Both options were restored to their existing ON values in the local
Debug, Release and dev caches and are not overridden by the final change.

SDL's configure summary can still list OpenGL renderer and PipeWire/dummy camera
drivers with the corresponding subsystem OFF. `SDL_internal.h` applies the
subsystem-disable macros; the summary alone does not describe usable services.

## Verification

Linux, GCC 16.2.1, real Wayland display, NVIDIA Vulkan. Debug includes the inspector;
Release excludes it. Existing local caches already enabled private retail/demo
data and GPU tests. Commands ran inside `Blood2/`:

```sh
cmake --preset debug -DSDL_OPENGL=ON -DSDL_OPENGLES=ON
cmake --preset release -DSDL_OPENGL=ON -DSDL_OPENGLES=ON
cmake --preset dev -DSDL_OPENGL=ON -DSDL_OPENGLES=ON
```

For each of Debug and Release, built the application and selected test targets
successfully (`--parallel 6` in Debug and `--parallel 4` in Release):

```sh
cmake --build --preset debug --parallel 6 --target blood2_app sdl_properties_tests platform_lifetime_tests config_tests cinematic_tests hud_tests menu_tests demo_menu_tests
ctest --preset debug -L headless -R '^(platform\.|config\.|cli\.|cinematics\.(decode|mixer|spatial|sequence|lifetime|failures|device)$|hud\.|menus\.|demo_menu\.)' --parallel 4
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json SDL_AUDIODRIVER=dummy \
  ctest --preset debug -R '^gpu\.(platform_(video|window|gpu)|lifecycle|hud|menus|menu_traversal|demo_menu)$'
```

The same test commands use `--preset release` for Release. The ICD and content
paths are local validation settings, not production requirements.

Each headless selection passed 34/35 tests, including all six private-data tests.
`cli.help` remains a failure in both builds: it expects `Blood II reconstruction`,
but the pre-existing rename in `47d4f87` changed the output to `Open Blood II`.
The command itself returns zero. Neither the help text nor its test was changed.

The final GPU selection passed 8/8 in each build: repeated video/window/device
lifetime, application lifecycle, HUD, menu artwork and retail/demo menu traversal.

| Build | Configuration | Headless results | GPU results |
| --- | --- | --- | --- |
| Debug | [log](evidence/sdl-options-debug-configure.log) | [34/35](evidence/sdl-options-debug-headless.log) | [8/8](evidence/sdl-options-debug-gpu.log) |
| Release | [log](evidence/sdl-options-release-configure.log) | [34/35](evidence/sdl-options-release-headless.log) | [8/8](evidence/sdl-options-release-gpu.log) |

The initial Debug selection failed `platform.destroyed_properties` (segfault) and
`platform.sdl_properties_patch`, and `gpu.platform_gpu` timed out. Both local SDL
source caches still had only the historical assignment fix (SHA-256 `b4f13bce...`).
Applied the repository's existing `cmake/PatchSdlProperties.cmake` to each cache,
which accepts that exact historical input and produced the expected `811c3da3...`
hash. Rebuilt all selected targets before repeating the checks. The property
tests then passed; no tracked patch or dependency source was changed. The initial
timeout is recorded as a failure, not proven to have the same cause. An earlier
full Debug build was deliberately interrupted to limit compilation to relevant
targets; a full build/test-suite pass is not claimed.

No performance or binary-size improvement was measured. Other platforms and
OpenXR execution were not tested. M2 closure and reservation M2-GPU-001 are
unchanged; this work does not reopen M2 or accept M3.
