# SubwayA in stereo on Meta Quest 3 (M8 lot 8.3, stage A)

2026-10-21, on top of `028f0669`. Contract: [OpenXR runtime](../../contracts/xr_runtime_contracts.md#stereo-world-lot-83).
Plan: [OpenXR plan](../openxr_plan.md).

## What changed

- Game data is read from the shared folders `/sdcard/openblood2/game` (retail, preferred)
  and `/sdcard/openblood2/demo`; the Quest flavor declares and requests all-files access.
- `--xr` (and the Quest APK with data) enters the world directly and renders it in
  stereo: the game camera's position and yaw carry the tracked head, calibrated at the
  first frame, with a 0.5 m lean clamp and a scale derived from the player's eye height.
- `ScenePresenter::record_eyes` records one scene pass per eye into the XR swapchain
  layers; renderers take an asymmetric per-eye projection and cull per eye.
- `world.vert`/`skinned.vert` gained the asymmetric offset terms (zero on desktop).
- The sRGB swapchain is rendered through UNORM views (mutable-format images), with a
  one-line local SDL patch, so colours are no longer encoded twice.

## Results

| Check | Result |
| --- | --- |
| Headless: `xr.math_*` (4), `platform.sdl_properties_patch`, `platform.sdl_xr_swapchain_patch` | Pass |
| Shader regeneration | Only `world_vert`/`skinned_vert` (SPIR-V and MSL) changed |
| Desktop GPU suite (`ctest -L gpu`, macOS `dev`, Apple M5, retail data) | 126/127 pass, including every world_view, surface, lightmap, model-shadow and sprite readback test. `gpu.entry_wait` fails: it expects the pre-menu direct launch (`Session playback:` is printed only by the smoke loop since the menu frontend), so the failure predates this change. |
| Quest build | No warnings in the changed files |
| Data | `/sdcard/openblood2/game`: retail 2.1 archives detected (BLOOD2, SOUND, BLOOD2P, BLOOD2P2, BLOOD2NM) |
| Session | `01_SubwayA`, LOCAL_FLOOR, 1680×1760 per eye, Touch controller profile, scale 48.12 units/m |
| Frame rate | 72.0 frames/s, every frame rendered; compositor `FPS=72/72`, `Stale=0`, `Tear=0` |
| GPU | App GPU 3.15–3.18 ms per frame at GPU level 2 (`GPU%=0.33`) — both eyes, shadow tiles, original renderer |
| CPU | Step 1.2–1.9 ms, presentation update 0.5 ms, recording 0.9–1.3 ms; 488 draw calls for both eyes |
| Colour | Without UNORM views the image was washed out; with them the contrast matches the desktop renderer (device screenshots, not committed: commercial art) |
| Swapchain | Quest 3 lists no `XR_KHR_vulkan_swapchain_format_list`; the core mutable-format usage bit is accepted |

Raw log: [evidence/xr-stereo-subwaya-quest3.log](evidence/xr-stereo-subwaya-quest3.log).

## Test conditions

The headset sat on a desk, so the proximity sensor and the Guardian were paused with
Meta's developer tooling for the run and re-enabled afterwards. All-files access was
granted with `adb shell appops set --uid org.openblood2.game MANAGE_EXTERNAL_STORAGE allow`.
The image was checked through the compositor screenshot (one eye); per-eye stereo
comfort and depth still need a worn run.

## Not verified or not included

Stereo comfort in the headset, other worlds, NEW_LEVEL in XR, cinematic and death
cameras in XR, the all-files access page flow on a fresh install, Vulkan validation on
the device (no layers), multiview (8.4), input (8.5), HUD and menus (8.6–8.7), the data
setup screen (8.8) and view weapons on the controllers (8.9).
