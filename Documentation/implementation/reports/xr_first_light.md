# XR first light on Meta Quest 3 (M8 lots 8.1–8.2)

2026-10-21, on top of `fca8b5a8`. Contract: [OpenXR runtime](../../contracts/xr_runtime_contracts.md).
Plan: [OpenXR plan](../openxr_plan.md).

## What changed

- `BLOOD2_XR` option; the `quest` preset builds an immersive OpenXR APK (VR launcher
  category, head tracking required, Khronos broker permissions and queries) with the
  Khronos loader AAR 1.1.63.
- `XrRuntime` (`src/engine/xr/`): SDL_GPU-created instance, session, LOCAL_FLOOR space,
  controller grip/aim poses, a two-layer sRGB swapchain, the session state machine and
  the frame loop.
- `--xr-test` (`src/app/xr_scene.cpp`): floor grid, origin axes, wireframe controller
  boxes and aim rays, one pass per eye. The Quest APK opens it when launched without
  arguments.
- New unlit line shaders (`line.vert/frag`) with an asymmetric projection; pure
  conversion math in `xr_math.h` with headless tests.
- spdlog writes to logcat on Android.

## Results

| Check | Result |
| --- | --- |
| `xr.math_axes`, `xr.math_projection`, `xr.math_eye_pose` (macOS `dev`) | Pass |
| Shader regeneration | The 18 existing artifacts are byte-identical; 4 new line artifacts |
| Desktop `dev` build (`BLOOD2_XR` off) | Builds; unchanged paths |
| `quest` preset | APK builds with no warnings in the new code; contains `libmain.so` and `libopenxr_loader.so` (arm64 only) |
| Quest 3, Horizon OS, sideloaded debug APK | Session reaches FOCUSED; system "Meta Quest 3", 1680×1760 per eye, 3 images, sRGB format, `LOCAL_FLOOR`, 72 Hz |
| Frame rate | 72.0 frames/s, every frame rendered; compositor `FPS=72/72`, `Stale=0`, `Tear=0`, app GPU 0.33–0.34 ms, CPU record 0.2 ms |
| Image | Floor grid at floor level with a level horizon (device screenshot) |
| Lifecycle | HOME: `visible → synchronized → stopping` (session ended), `idle`, then activity destroy and a clean exit (code 0) |
| Controllers | Not verified: they were asleep on the desk (`controllers false/false`). Needs a run with the headset worn. |

Raw log: [evidence/xr-first-light-quest3.log](evidence/xr-first-light-quest3.log).

## Test conditions

The headset sat on a desk. For the automated run the proximity sensor and the
Guardian were paused with Meta's developer tooling, and both were re-enabled
afterwards. A Guardian dialog blocks immersive launches while the boundary is not
set up. The headset was not worn, so stereo comfort and controller tracking remain
user checks.

## Not verified or not included

Controller boxes and aim rays in the headset; the stereo image as seen by each eye;
game rendering, multiview, gamma-space UNORM views, input actions, HUD/menus, the
data screen and PCVR (all later lots of the plan).
