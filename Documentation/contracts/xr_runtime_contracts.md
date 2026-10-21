# OpenXR runtime and first light (M8 lots 8.1–8.2)

Implements the first two lots of the [OpenXR plan](../implementation/openxr_plan.md):
an immersive Quest APK and an OpenXR session that draws a world-locked test scene.
No game world, HUD or menu is rendered in XR yet.

## Build

- `BLOOD2_XR` (CMake option, OFF by default) compiles `src/engine/xr/xr_runtime.cpp`
  and `src/app/xr_scene.cpp`. The `quest` preset turns it on; desktop presets keep it
  off, so their behaviour and frames are unchanged.
- The OpenXR headers are the 1.1.63 copies vendored in the pinned SDL
  (`src/video/khronos/openxr`), included as system headers. No new source dependency.
- The `quest` Gradle flavor depends on `org.khronos.openxr:openxr_loader_for_android:1.1.63`
  (version matched to those headers) and packages arm64 only. SDL `dlopen`s
  `libopenxr_loader.so`; Meta's forward loader is not used.
- The Quest manifest overlay replaces the activity: `com.oculus.intent.category.VR`
  and `org.khronos.openxr.intent.category.IMMERSIVE_HMD` on the launcher filter,
  `headtracking required=true`, the Khronos broker permissions and `<queries>`,
  `com.oculus.vr.focusaware`, landscape, `singleTask`, black fullscreen theme.
- spdlog's console sink is logcat on Android (tag `OpenBlood2`).

## Runtime rules (`XrRuntime`)

- SDL_GPU creates the OpenXR instance (`SDL_PROP_GPU_DEVICE_CREATE_XR_*`, Vulkan only)
  and destroys it with the device. The runtime requests API 1.0.
- Optional instance extensions are enabled only when the loader lists them:
  `XR_EXT_local_floor` and `XR_FB_display_refresh_rate` (refresh rate is logged, not
  changed).
- Reference space preference: `LOCAL_FLOOR`, then `STAGE`, then `LOCAL` with a nominal
  floor at −1.6 m.
- One action set (`controllers`) holds the grip and aim pose actions for both hands,
  with suggested bindings for `oculus/touch_controller` and `khr/simple_controller`.
  It is attached once; lot 8.5 adds the gameplay and menu actions to that attach.
- One colour swapchain with `arraySize = 2` at the runtime's recommended per-eye size;
  layer *n* is eye *n*. The format is the runtime's sRGB choice when it offers one.
- Session states drive begin/end: READY begins the session, STOPPING ends it,
  EXITING, LOSS_PENDING and instance loss quit. Events are logged.
- Frame order: `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`; actions sync and
  controller spaces are located only when FOCUSED, so poses are invalid (and the boxes
  hidden) while the system menu has focus. Views with invalid poses or FOVs are not
  rendered.
- The command buffer is submitted before `xrReleaseSwapchainImage`; `xrEndFrame` gets a
  projection layer only for a frame that was rendered.

## First light (`--xr-test`)

- `blood2 --xr-test` on an XR build, and the Quest APK launched without arguments,
  open the XR test scene. It needs no game data.
- The scene is in metres (one engine unit per metre): a 10 × 10 m floor grid at the
  reference space's floor, red/green/blue axes at the origin (+X, +Y, engine +Z =
  the initial forward), a wireframe box at each grip pose (cyan left, orange right)
  and a 2 m yellow ray along each aim pose. Colours are linear values for the sRGB
  swapchain.
- Rendering is one pass per eye (stage A of the plan) with a two-layer depth array.
  The line pipeline (`line.vert/frag`) uses the asymmetric projection from
  [`xr_math.h`](../../src/engine/xr/xr_math.h).
- Android keeps polling OpenXR while the activity is paused
  (`SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0`). The loop sleeps 10 ms while no session runs.
- Every 5 s the log records frames per second, display period, CPU record time, the
  session phase and controller validity.

## Conversions

`xr_math.h` converts OpenXR (right-handed, −Z forward, metres) to the engine
(left-handed, +Z forward, units) by the reflection z → −z and a scale:
positions `(x, y, −z)·s`, quaternions `(−x, −y, z, w)`. The asymmetric projection maps
the left/down tangents to NDC −1 and the right/up tangents to +1; a symmetric 90°
FOV gives the desktop projection.

## Verification

- `xr.math_axes`, `xr.math_projection`, `xr.math_eye_pose` (headless).
- Regenerating the shaders leaves the eighteen existing artifacts byte-identical; the
  four line artifacts are new (SPIR-V and MSL).
- Device evidence: see the [first-light report](../implementation/reports/xr_first_light.md).

## Not included yet

Game rendering in XR, multiview, the UNORM render-target view for gamma-space
output, gameplay and menu actions, haptics, HUD/menu layers, refresh-rate and
performance settings, the data setup screen, PCVR presets.
