# OpenXR plan: Meta Quest first, PCVR on Linux last (M8)

Drafted 2026-10-21 at `c09a9d1f`. Lots 8.1 and 8.2 have their first light on a
Quest 3 ([report](reports/xr_first_light.md)), lot 8.3 renders SubwayA in stereo at
72 Hz ([report](reports/xr_stereo_world.md)), lot 8.5 adds the controllers
([report](reports/xr_controls.md)), lot 8.6 the HUD and menus ([report](reports/xr_menus_hud.md)) and
lot 8.7 the menu laser with the first playtest's fixes ([report](reports/xr_laser_playtest.md))
and lot 8.9 the view weapons on the controllers with arm IK ([report](reports/xr_arms.md));
see the [contract](../contracts/xr_runtime_contracts.md). Lot 8.6 composites the menu panel as an
OpenXR quad like the HUD, rather than drawing it in the eye buffer: it is sharper and shares
one canvas; the 8.7 laser draws its cursor into the canvas and its beam up to the panel.
The rest is not implemented. Since then the data location is the shared
`/sdcard/openblood2/{game,demo}` (user decision), and the Quest 3 offers no swapchain
format list, so gamma-space output uses the core mutable-format bit. It turns
roadmap M8 into ordered lots and fixes the design choices the user set:

- Meta Quest is the priority; PCVR is tested last, on Linux.
- Multiview stereo rendering wherever the platform allows it, for performance.
- Controllers are drawn as wireframe cubes for now.
- Menus and other UI sit 0.75 m in front of the view; the HUD is locked to the view.
- The first-person arms follow the controllers through IK on their own rig.
- Controller bindings are as good as the hardware allows.
- Menus are driven by a selection laser.
- A setup screen tells the user where to put the game data and offers to download the demo.

Sizes use the M9 scale: S ≈ 1 lot, M ≈ 2–3, L ≈ 4–5, where a lot is one
continuation-sized change.

## Starting point

| Area | Current state | Consequence for VR |
| --- | --- | --- |
| SDL | The pinned SDL (`6defbc0`, 3.5.0) builds with `SDL_GPU_OPENXR=ON`. `SDL_openxr.h` creates the session and swapchains, and swapchain images come back as `SDL_GPUTexture`s. `SDL_PROP_GPU_DEVICE_CREATE_XR_*` creates the instance (Android loader init and `XR_KHR_android_create_instance` included). SDL `dlopen`s the Khronos `libopenxr_loader`. | No raw Vulkan interop is needed. |
| Multiview | `SDL_gpu.h` has no view mask. The Vulkan backend builds framebuffers with `layers = 1` and render passes without `VkRenderPassMultiviewCreateInfo`. | Multiview needs a local SDL patch (lot 8.4). Per-eye passes work as they are, through the colour target `layer_or_depth_plane`. |
| Device | `SDL_CreateGPUDevice` is called with no properties ([runtime.cpp:23](../../src/engine/runtime.cpp)). The frame (acquire, record, ImGui, submit) is inline in `run_frontend` ([frontend.cpp:716-777](../../src/app/frontend.cpp)). | XR moves to `SDL_CreateGPUDeviceWithProperties`, and the frame loop gains an XR branch. |
| Projection | `world_view_uniform` pushes per draw a 128-byte camera basis plus a symmetric projection, 90° horizontal, near 1 / far 65536 ([world_view.cpp:648-663](../../src/engine/world_view.cpp)). Culling planes are fixed at 45° ([world_renderer.cpp:23-41](../../src/engine/world_renderer.cpp)). Every renderer uses one viewport. | XR needs asymmetric per-eye FOVs, wider culling and a second view. |
| Close draws | View weapons and `FLAG_REALLYCLOSE` draw in a reserved [0, 1/256] depth slice ([scene_presenter.h:57](../../src/app/scene_presenter.h)). | In VR the weapon sits in world space at the controller, so it needs real depth. |
| Overlays | The menu is a CPU-composed 640×480 canvas uploaded to a texture ([menu_renderer.cpp](../../src/app/menu_renderer.cpp)). The HUD, crosshair, fades and modals are ImGui draw lists in window pixels, drawn into the swapchain. | They must render offscreen and reach a panel or a compositor layer. |
| Camera and aim | `client_shell.view` (`CameraView`) feeds the presenter. `shell.rotation` drives movement yaw, fire direction (origin at the eye), USE, the view-weapon pose, the listener and server readers of `m_fPitch/m_fYaw` (flashlight, TheEye, Orb, DeathRay, Voodoo). | Head, aim and locomotion directions must be separated. |
| Input | The [action layer](../contracts/input_action_contracts.md) has Hold/Latch/Analog/Press/Command/Interface kinds. Touch controls inject a virtual gamepad (`touch_gamepad_id`). The contract lists VR controllers as remaining. | XR plugs into the action layer and adds poses. |
| Menus | `Menu::pointer(x, y, press, release, motion)` already hit-tests items and sliders in 640×480 canvas coordinates ([MainMenus.cpp:504-556](../../src/game/original/MainMenus.cpp)). | A laser only has to produce canvas coordinates. |
| Data | Data comes from `blood2.local.ini`, `BLOOD2_DATA_DIR` or `--data-dir`. Missing data exits with code 2, and Android has no provisioning. The launcher downloads the [qualified demo](../contracts/launcher_contracts.md#qualified-demo-delivery): Archive.org ZIP, SHA-256, bounded miniz extraction. | Quest needs an in-headset setup screen that reuses the launcher's qualification. |
| Quest build | The `quest` preset builds a 2D panel APK: no `com.oculus.intent.category.VR`, `headtracking required=false`, no OpenXR loader. | The flavor becomes immersive. |
| View weapons | `*_pv.abc` / `lh_*_pv.abc` carry the hands and arms, reskinned per character. Node transforms are composed on the CPU before the GPU palette push ([model_view.cpp:418-458](../../src/engine/model_view.cpp)). No PV node names are documented. | IK can edit poses before the push, but the PV rigs must be audited first. |

## Design decisions

### Runtime and frame

- **New engine module `src/engine/xr/`.** `XrRuntime` owns the instance (through SDL
  device properties), the session, reference spaces, swapchains, the event poll and
  the session state machine. It has no game knowledge. App glue lives in
  `src/app/xr_*`. A CMake option `BLOOD2_XR` (ON for the Quest preset, OFF elsewhere)
  compiles it. Desktop builds without it keep their behaviour and frames.
- **One pure per-frame value, `XrFrameInput`.** It carries the predicted display time
  and period, the head pose, the per-eye poses and FOVs, the controller grip and aim
  poses, action states and the session state. The backend fills it from OpenXR;
  tests fill it synthetically. Presenter, input bridge, laser and IK read only this
  value.
- **The frame order** is:
  1. `xrWaitFrame` and the event poll
  2. `xrSyncActions`
  3. `XrFrameInput`
  4. the input bridge
  5. the simulation step: delta from the predicted display period, still capped at 100 ms by `app_frame_delta`
  6. `presenter.update`
  7. acquire the swapchain images
  8. record: shadow atlas, offscreen overlays, scene, laser and cubes
  9. submit, then `xrEndFrame`

  `xrWaitFrame` replaces VSYNC pacing.
- **Reference spaces.** Use `LOCAL_FLOOR` (core in OpenXR 1.1), falling back to
  `STAGE`, then `LOCAL` plus a calibrated height. Recentering follows
  `XrEventDataReferenceSpaceChangePending`.
- **Session states.** Render only in VISIBLE/FOCUSED. Anything below FOCUSED takes
  the existing focus-loss path: open the pause menu, reset input, pause game audio,
  hide the controllers (Quest VRC). STOPPING ends the session. EXITING and
  LOSS_PENDING quit cleanly. Android background/foreground events are handled
  explicitly; none are today, and SDL's block-on-pause must not starve the XR event
  loop.
- **Units and axes.** OpenXR is right-handed in metres with −Z forward. The engine is
  left-handed with +Z forward, in world units. Convert once, with tested helpers, as
  a Z flip plus `units_per_meter`. Lot 8.0 derives the default from the player's
  standing `eye_level` against a 1.6 m nominal eye height. A user height
  calibration adjusts only the head offset, never the collision body.
- **Colour encoding.** The renderer writes gamma-encoded values into a UNORM
  swapchain on desktop. OpenXR treats UNORM swapchains as linear, so the image would
  look washed out. XR swapchains are therefore created `_SRGB` and written through a
  UNORM render-target view (`XR_KHR_vulkan_swapchain_format_list` and a mutable
  format), part of the local SDL patch. Blending then stays in gamma space exactly as
  on desktop. The fallback is a measured final conversion pass.

### Stereo rendering

- **`StereoView`.** It carries two eyes (pose, asymmetric `XrFovf`) plus a head
  centre. `CameraView` stays the gameplay camera. The XR eye pose is composed over
  it:

  ```
  eye = view.position + R(body_yaw) · (head_offset + eye_offset) · units_per_meter
  ```

  The orientation is `body_yaw ∘ HMD orientation`, roll included. The original
  camera never had roll.
- **Projection.** The vertex uniform gains x/y offset terms for asymmetric frusta.
  With zero offsets the desktop arithmetic is unchanged, and the GPU readback tests
  keep proving byte-identical frames. Sky draws use the head centre for both eyes
  (no parallax at infinity). Near stays 1 unit (about 2–3 cm in the expected scale);
  the close near of 0.01 remains.
- **Shared per-frame work.**
  - Culling uses one union frustum built from the XR FOVs, replacing the fixed 45°
    planes in XR only.
  - Translucent sorting, sprite billboards and explosion-model sorting run once,
    against the head centre, so both eyes (and multiview) consume one draw stream.
  - The shadow atlas renders once per frame.
- **Stage A, per-eye passes.** One `array_size = 2` colour swapchain and a presenter
  depth array, recorded as two scene passes (layer 0 and layer 1). This works with
  today's SDL, including Metal in the desktop mock mode, and is the fallback when
  multiview is unavailable.
- **Stage B, multiview.** A local Vulkan-only SDL patch, applied like
  `PatchSdlProperties.cmake`, adds:
  - a graphics-pipeline view-mask property
  - a multiview render-pass begin entry point
  - view mask included in the render-pass and framebuffer cache keys
  - 2D-array attachment views
  - the Vulkan 1.1 `multiview` feature enabled on the device
  - the sRGB/UNORM view rule above

  Shader variants come from the same HLSL with a `MULTIVIEW` define and
  `SV_ViewID`, which DXC emits as SPIR-V `ViewIndex`:
  - both vertex shaders, with a 256-byte two-eye view block
  - the fragment shaders that read the eye position (`SurfaceFrameUniform`)

  They are SPIR-V only; there is no XR target on Metal. The existing eighteen desktop
  artifacts stay byte-identical and are still regenerated together, with the new
  variants added to the list.

  The patch stays local. SDL's contribution policy refuses AI-written changes, so it
  is never proposed upstream from this project's agents. Re-check it whenever SDL is
  updated, and drop it if upstream gains multiview.
- **Mode selection.** Multiview is the default wherever the device reports the
  feature. An option forces per-eye passes, for A/B measurement and as a fallback.
- **Close draws.** In XR the view weapon draws in world depth at the controller pose.
  The reserved slice stays for desktop and for any other `FLAG_REALLYCLOSE` object.
  The cinematic letterbox is not drawn in XR.
- **Audio.** The listener follows the head pose, not the aim.

### UI, HUD, laser and controllers

- **Offscreen overlays.** ImGui renders into an offscreen colour texture with a
  forced `DisplaySize`; HUD, crosshair and fades currently read the window viewport.
- **HUD, locked to the view.**
  - It is a VIEW-space `XrCompositionLayerQuad` 0.75 m in front of the eyes, built
    from its own XR swapchain with source-alpha blending.
  - The compositor keeps head-locked content stable under reprojection and sharper
    than eye-buffer text.
  - The canvas covers a configurable central field, about 50° wide by default, so the
    corner tabs of `CNewStatusBar` stay readable. `CNewStatusBar` lays out for the
    canvas size with its original rules, including the `< 512×384` variant.
  - Distance and size are VR options.
- **Menus, 0.75 m in front.**
  - The 640×480 menu texture is drawn as a world-locked panel, placed 0.75 m in front
    at the head yaw when the menu opens. Recentering is an option.
  - It is drawn in the eye buffer, not as a compositor layer, so the laser and cursor
    occlude correctly and every overlay uses one path.
  - A paused world stays visible, dimmed, behind the panel. With no world (main menu,
    setup screen) the backdrop is a dark void with a floor grid.
  - If text legibility is insufficient on device, a later lot can switch to a quad
    underlay with an alpha hole. The risk is translucent passes writing alpha.
- **Selection laser.**
  - It comes from the aim pose of the last-used hand, which is the dominant hand by
    default. It is intersected with the panel plane; the UV × 640×480 goes to
    `Menu::pointer`, with trigger edges as press and release.
  - Hover, activation and sliders reuse the existing pointer rules. A short haptic
    pulse marks a selection change.
  - ImGui panels (error modal, setup screen) receive the same point as ImGui mouse
    events.
  - Stick and button navigation keep working through the existing Menu* actions.
- **Controllers.** A new unlit line pipeline (position + colour, multiview-aware,
  SPIR-V and MSL) draws a wireframe box at each grip pose, sized like a Touch
  controller, plus a short aim tick. The laser uses the same pipeline. Cubes are
  hidden when a pose is invalid or input focus is lost.
- **Reticle.** The screen-centred original crosshair is hidden in XR. A small
  depth-ignoring reticle is drawn at the converged aim point (see below) with
  constant angular size.
- **Fades.** FADEIN/FADEOUT and death fades become
  `XR_KHR_composition_layer_color_scale_bias` on the projection layer when available.
  The fallback is a full-view quad in the eye buffer. The HUD layer fades with them.

### Input, aim and locomotion

- **OpenXR action sets.** `gameplay` and `menu` hold semantic actions with
  localized names. Suggested bindings are given for:
  - `/interaction_profiles/oculus/touch_controller` (every Quest)
  - `/interaction_profiles/meta/touch_controller_plus` (Quest 3/3S, when
    `XR_META_touch_controller_plus` is present)
  - `/interaction_profiles/khr/simple_controller` (fallback)

  The PCVR lot adds `valve/index_controller` and `htc/vive_controller`. Hand
  tracking is out of scope.
- **Bridge into the action layer.** An `XrInputBridge` feeds a fixed, injected XR
  device whose per-action states join the binding-based states before kind
  processing. The Press guard, the RunLatch, menu edges and focus resets are
  therefore shared. The Controls page does not rebind it in v1; runtime rebinding
  (SteamVR) applies instead.

  A virtual gamepad, as touch uses, was rejected: rebinding the physical gamepad
  would silently change the VR bindings, and Touch grips do not map onto bumpers.
- **Aim from the controller, outside original code where possible.**
  1. Cast a ray from the right controller's aim pose (muzzle) to the first world or
     object hit, up to the weapon range ceiling.
  2. Write `shell.rotation` as the eye→hit angles.
  3. Shots, USE (82-unit eye ray) and every server reader of `m_fPitch/m_fYaw` then
     land where the controller points, with no change to weapon code.

  A later option sends the muzzle as the `WeaponFire` origin, after an eye→muzzle
  clearance ray so a hand pushed through a wall cannot shoot through it. Desktop
  behaviour is unchanged.
- **Locomotion.** `CBloodClientShell::UpdateMovement` gains a separate locomotion yaw
  (one explicit, documented adaptation in `BloodShells.cpp`; desktop passes
  `rotation.yaw`).
  - The stick angle is mapped to the nearest of the original eight flag combinations,
    and the residual angle is folded into the yaw. The direction is exact, while
    per-direction speeds and animations stay original.
  - The reference is the head (default) or the left controller.
  - The run threshold is the existing stick rule plus the RunLatch.
- **Turning.** Snap turn by default (45°, options 30/45/60/90) or smooth turn. It
  rotates `body_yaw`. With aim already from the controller, this does not fight the
  aim.
- **Haptics.** Short pulses on fire, on menu hover changes and (optionally) on damage.
- **Left-handed option.** It mirrors the weapon hand and the bindings.

Default Touch bindings as first planned (superseded after the first playtest by the
user's layout in the [contract](../contracts/xr_runtime_contracts.md#controllers-lot-85)):

| Control | Gameplay | Menu |
| --- | --- | --- |
| Left stick | Move (analog, head-relative) | Navigate |
| Left stick click | RunLatch | — |
| Left trigger | Alternate fire | Accept (laser) |
| Left grip | Use item (inventory cursor) | — |
| X | Next item | — |
| Y | Detonate (remote bombs) | — |
| Left menu button | Menu / pause | Back / close |
| Right stick X | Snap or smooth turn | Navigate |
| Right stick Y | Next / previous weapon | Navigate |
| Right stick click | Flashlight | — |
| Right trigger | Primary fire | Accept (laser) |
| Right grip | Use (doors, switches) | — |
| A | Jump | Accept |
| B | Crouch (hold; toggle option) | Back |

Medkit, binoculars, night goggles, TheEye and the bombs stay reachable through the
inventory cursor, as on the console layout. Quick save and quick load stay in the
menu.

### Arms and view weapons

1. **Audit (lot 8.0).** On private data, dump the node hierarchy, bind poses and
   scale of every `*_pv.abc` and `lh_*_pv.abc`. Record per model: the arm chain
   (upper arm, forearm, hand), the weapon grip node, the muzzle node, the PV-to-world
   scale and whether the geometry is cut at the original screen edge.

   The result is a versioned `config/xr_arms.json` with bounded parsing, like the
   materials manifest. Models without a valid entry fall back to rigid placement,
   and inspection says so.
2. **Rigid placement.** Place the model root so the audited grip node matches the
   controller grip pose, with the per-model scale and grip offset. Weapon
   animations (fire, reload, recoil) play unchanged. Akimbo left-hand models follow
   the left controller.
3. **Two-bone IK.**
   - A virtual shoulder is derived from the head: a neck pivot below and behind the
     head, offset by shoulder width, with a body yaw that follows the head beyond a
     ±45° dead zone.
   - An analytic two-bone solve runs from shoulder to hand, with a downward/outward
     elbow pole and limited shoulder slide when the target is out of reach.
   - It runs after `sample_skin` and before the palette push. Descendants are
     recomposed from the edited local poses, which needs a pose-edit API around the
     file-local `local_pose`/`fill_pose` in `model.cpp`.
   - The animated hand-in-weapon offset is kept, so animations still read correctly.
4. **Cut geometry.** Arms modelled only for a 90° screen may end in visible stumps.
   Hide the beyond-forearm nodes with the existing per-instance node masks, or accept
   the rigid fallback for that model.

Two-handed foregrip aiming and motion-driven melee are out of scope for v1: the left
hand keeps its animation, and melee stays trigger-driven.

**As built (lot 8.9).** The audit found the chains follow one naming rule (`lu_arm*` →
`ll_arm*` → `d_null*` → hand mesh) and the models at about 1/8 scale, so there is no
`config/xr_arms.json`: chains are discovered by name, the scale is one constant
(0.17 m per model unit) and only the eleven two-handed models without a trigger hand need
data, an audited grip table in `src/engine/view_arms.cpp`. The solve edits the composed
model-space poses (rigid segment deltas), so no pose-edit API in `model.cpp` was needed.
Cut geometry is handled by masking what the original camera framed out (a widened view
cone) rather than per-model node lists. The muzzle-origin option is not built.

### Data setup screen and demo download

- **Location on Quest.**
  - `SDL_GetAndroidExternalStoragePath()` gives
    `/sdcard/Android/data/org.openblood2.game/files/`. Retail data goes in `Blood2/`
    and the demo in `Demo/`. No storage permission is needed.
  - The path is reachable over USB (MTP, after the headset's file-access prompt) and
    with `adb push`.
  - The edition is inferred from the REZ present: retail if `BLOOD2.REZ` is valid,
    demo if only `B2DEMO.REZ` is. When both are present a choice is shown and saved
    in preferences.
- **Startup without data.** Android no longer exits with code 2; it opens the setup
  screen. Desktop non-XR behaviour is unchanged.
- **The screen.**
  - It is an ImGui panel at 0.75 m, driven by the laser, using an embedded Droid
    Sans: game fonts are unavailable by definition. The launcher already ships the
    font and its Apache licence.
  - It shows the exact path, the required and optional files (`BLOOD2.REZ` required;
    `SOUND.REZ`, `BLOOD2P.REZ`, `BLOOD2P2.REZ` optional; the loose Music folder) with
    each one's detected state.
  - Its buttons are **Rescan**, **Download the demo (37.7 MB, archive.org)** and
    **Quit**.
- **Download.**
  - Split `src/launcher/demo_install.cpp` into a transport-independent library
    (`blood2_content`) shared with the launcher. It keeps the provider constants,
    staging, SHA-256 checks, bounded miniz extraction, the publish rename and every
    rule of the launcher contract.
  - On Android the transport is a small Java `HttpsURLConnection` helper: platform
    TLS, archive.org hosts only, at most five redirects and the same
    timeout/size/low-speed limits. It streams into staging with progress and atomic
    cancel. No curl/OpenSSL cross-build is needed. SHA-256 comes from the platform or
    a vetted single-file implementation, decided in the lot.
  - The download starts only from the button; `INTERNET` is the only new permission.
  - After a successful install the demo is validated and started.
- **PCVR.** The same screen shows the configured data state and points to the
  launcher, which already downloads the demo. The desktop game build does not gain
  curl.

## Lots

| # | Lot | Size | Depends on | Done when |
| --- | --- | --- | --- | --- |
| 8.0 | XR contract draft; PV rig audit and `xr_arms.json` schema; `units_per_meter`; Quest per-eye size and refresh baselines from the runtime | S | — | Contract and audit committed; scale measured on private data (Linux agent). |
| 8.1 | Build and packaging. Pinned OpenXR-SDK headers. `BLOOD2_XR`. Immersive Quest flavor: VR category, `headtracking required=true`, `INTERNET`, release-manifest rules. Pinned Khronos `openxr_loader_for_android` AAR (exact version, SHA-256 recorded in the packaging guide). Fixed Android data path with edition inference, so data can be pushed for early device runs. | S–M | 8.0 | Quest APK builds; desktop frames and tests unchanged; packaging guide updated. |
| 8.2 | XR runtime core: device with XR properties, session, spaces, swapchains, frame loop, events, session states, Android lifecycle, recenter, refresh rate, `XR_EXT_performance_settings`. First light: per-eye clear plus a head-locked test grid. | M | 8.1 | User confirms on Quest: stable tracking, pause and resume, system menu; logs clean. |
| 8.3 | Stereo presentation, stage A: `StereoView`, asymmetric projection, union culling, depth array, sky at infinity, shared sorting, world-depth view weapon, listener on head, sRGB/UNORM view. Desktop `--xr-mock` (side-by-side window or offscreen) and `--bench --xr-mock`. | M–L | 8.2 | Desktop GPU readback still byte-identical; mock readback tests; SubwayA renders correctly in the headset. |
| 8.4 | Multiview, stage B: local SDL patch, `MULTIVIEW` shader variants, runtime selection and fallback. Quest frame-time evidence (OVR Metrics or Perfetto) for both modes. | M | 8.3 | Identical images in both modes (mock readback on Linux Vulkan); measured CPU/GPU gain recorded. |
| 8.5 | Controllers and input: action sets, suggested bindings, `XrInputBridge`, wireframe cubes, aim convergence and reticle, locomotion yaw adaptation, snap/smooth turn, haptics, handedness. VR options page (turn, movement reference, handedness, HUD distance and size, seated height). | M | 8.3 | Headless tests: bindings, edges, residual-yaw mapping, aim convergence. Desktop unchanged. SubwayA playable on Quest with controllers. |
| 8.6 | VR overlays: offscreen ImGui, HUD quad layer, menu panel, colour-scale fades, credits, save notice, error modal. | M | 8.3 | Mock readback of panel/HUD layout; device legibility check by the user. |
| 8.7 | Selection laser: panel intersection to `Menu::pointer`, sliders, hover haptics, ImGui pointer, stick navigation kept. | S | 8.5, 8.6 | Headless UV→canvas→item tests; every menu page usable with the laser. |
| 8.8 | Setup screen and demo download: `blood2_content` split (launcher contract unchanged), Java transport, setup UI, edition choice, post-install validation. | M | 8.6, 8.7 | Launcher checks still pass; local-fixture tests (no network) for staging and verification; user confirms a real download on Quest. |
| 8.9 | View weapons and arm IK: rigid grip placement, akimbo, then two-bone IK with virtual shoulders and node masks for cut geometry; optional muzzle origin with clearance check. | L | 8.0, 8.5 | IK solver unit tests (reach, pole, clamp); every weapon checked in the headset; fallbacks inspectable. |
| 8.10 | Comfort and VR-specific gaps: head-in-wall fade, lean clamp and room-scale follow through one MoveMgr integration, optional physical crouch, cinematic look-around with fades on cuts, death orbit replaced by fade/static view, binocular and sniper zoom as a magnified inset (documented no-op until then), seated mode. | M | 8.5 | Behaviour tests for each rule; comfort pass by the user. |
| 8.11 | Quest performance: refresh rate (72/90 Hz), render scale, Quest detail defaults (per-vertex lighting, original renderer, model shadows off), 4× MSAA evaluation, optional `XR_FB_foveation` (needs a fragment-density-map addition to the SDL patch), thermal soak on SubwayA → CityHub. | M | 8.4 | Recorded frame-time and thermal evidence meeting the chosen refresh rate on the target headset. |
| 8.12 | PCVR on Linux: `BLOOD2_XR` desktop preset, system `libopenxr_loader.so.1`, Index/Vive profiles, multiview on NVIDIA (`VK_DRIVER_FILES`), desktop mirror of the left eye with the inspector still on the window, launcher "Play in VR" (`--xr`). Runtimes: SteamVR or Monado/WiVRn (a Quest can stream to Linux through WiVRn). | M | 8.4–8.9 | User confirms a session on Linux; desktop non-XR still unchanged. |
| 8.13 | Acceptance record: device and runtime matrix, open omissions, the user's decision. | S | all | Report committed for the user's decision. |

Total: about 27–39 lots. Order: 8.0 → 8.1 → 8.2 → 8.3 → 8.4 → 8.5 → 8.6 → 8.7 →
8.8 → 8.9 → 8.10 → 8.11 → 8.12 → 8.13. Performance measurement starts at 8.4;
8.11 is the final tuning. M7's remaining items (touch device test, menu auto-repeat,
Keyboard/Joystick pages) do not block this work.

## Verification policy

- Agents only build Quest APKs (AGENTS.md). Device runs, installs and Meta tools
  (logcat, screenshots, Perfetto traces) happen with the user's explicit approval,
  each time. No Meta XR Simulator, Android emulator or simulated OpenXR runtime
  without asking.
- Desktop non-XR builds keep byte-identical frames (`gpu.surface` and the existing
  readback tests), the eighteen shader artifacts and every existing test.
- Pure logic gets headless tests through `XrFrameInput`: axis conversion,
  projection, culling, bindings, locomotion mapping, aim, laser, IK and the setup
  state machine. Rendering gets mock-mode GPU readback.
- Quest and PCVR evidence (traces, frame times, device and OS versions) go into
  reports under `implementation/reports/`, with failures kept.

## Risks

| Risk | Mitigation |
| --- | --- |
| The Quest frame budget (13.9 ms at 72 Hz) with a single-threaded game loop | Multiview, shared culling and sorting, Quest detail defaults; measure CPU and GPU separately from 8.4 on. |
| The local SDL patch drifts with SDL updates | Keep it small, Vulkan-only and scripted; re-check on every SDL bump; per-eye fallback always available. |
| PV rigs lack upper arms, or have a miniature scale | The 8.0 audit decides; per-model scale, node masks and rigid fallback. |
| Converged aim misses near walls | The optional muzzle origin with clearance check (8.9). |
| Motion sickness from original speeds, impulses, cinematics and the death orbit | Snap turn, head-relative movement, fades and the 8.10 comfort rules. Original gameplay physics are not changed. |
| MTP access to `Android/data` varies with Horizon OS versions | `adb push` documented; the in-headset demo download needs no PC. |
| 640×480 menu art is soft at 0.75 m | Panel size option; quad underlay as a later option. |

## Defaults taken (the user may override)

- The `quest` flavor becomes immersive; the 2D panel APK is dropped.
- Quest 3/3S is the performance target and Quest 2 is best effort at reduced
  settings.
- Movement is head-relative and turning is a 45° snap; aim comes from the right
  controller.
- The demo source is the launcher's qualified Archive.org package.
