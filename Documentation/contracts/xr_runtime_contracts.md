# OpenXR runtime, stereo world, controllers, HUD and menus (M8 lots 8.1–8.3, 8.5–8.7, 8.9)

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

## Stereo world (lot 8.3)

- `blood2 --xr` (XR builds) and the Quest APK launched with game data enter the world
  directly (`--world`, `Demo_01` for the demo) through the ordinary start point, with no
  menu, HUD or controller input yet. Simulation and game audio run only while FOCUSED;
  the step is the time between the last two predicted display times (the display period
  on the first frame), capped at 100 ms like `app_frame_delta`.
- **Data folders.** The headset reads `/sdcard/openblood2/game` (retail, `BLOOD2.REZ`)
  before `/sdcard/openblood2/demo` (`B2DEMO.REZ`); file names match case-insensitively.
  Neither opens the first light. The Quest flavor declares `MANAGE_EXTERNAL_STORAGE`
  (Android 11+ hides files that other apps or adb placed there); `Blood2Activity`
  opens the system "all files access" page when it is missing. A demo/full choice in
  the menu comes later.
- **Camera.** The game camera (`client_shell.view`) contributes its position and yaw only;
  the tracked head adds orientation (roll included) and an offset from a reference taken
  at the first rendered frame, after a recentre and after NEW_LEVEL. The horizontal lean
  is clamped to 0.5 m. World scale: the player's standing eye height above its feet
  (`dims.y + eye_level`) maps to 1.6 m — 48.12 units/m for Caleb — with 45 as fallback.
- **Presenter.** `camera_override` and `weapon_camera` give `update()` the head camera
  (sprites face it; the sky, explosion sorting, shadow casters and view weapons follow
  it). `record_eyes()` records one scene pass per eye into the layers of the XR
  swapchain, sharing the frame's uploads and shadow tiles, with a two-layer depth array,
  no letterbox and `view_scale` ignored. `WorldRecordTarget::eye` carries each eye's
  asymmetric projection to every draw (sky and close draws included) and per-eye culling
  (`ViewFrustum::asymmetric`). Close draws keep the reserved depth slice.
- **Shaders.** `world.vert` and `skinned.vert` add `z·right.w` and `z·up.w`; both are zero
  on desktop and for shadow tiles, so desktop frames do not change.
- **Gamma.** The renderer's output is gamma-encoded. The sRGB swapchain is created with
  `XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT` and rendered through UNORM views, so the
  compositor decodes it once and blending stays in gamma space as on desktop. The
  format-list extension is chained when offered (the Quest 3 does not offer it). A
  refused mutable swapchain falls back to plain sRGB with a warning. This needs the local
  SDL patch `cmake/PatchSdlXrSwapchain.cmake` (hash-guarded; SDL otherwise overwrites the
  explicit runtime format with the view format).

## Controllers (lot 8.5)

- **Actions.** One `gameplay` action set holds semantic actions (runtime-rebindable,
  localized names) and the grip/aim poses, attached once. Suggested bindings:
  `oculus/touch_controller` (every Quest; the Quest 3 Touch Plus is served through it) and
  `khr/simple_controller` (fire, use, menu). States are read only while FOCUSED.
- **Default layout (right-handed, chosen by the user after the second worn test).**

  | Control | Play | Menu |
  | --- | --- | --- |
  | Left stick | Move, head-relative (full deflection runs) | Navigate |
  | Left stick click | Jump | — |
  | Left trigger | Alternate fire | Back |
  | Left grip | Detonate | — |
  | X / Y | Previous / next item (inventory cursor) | — |
  | Left menu button | Menu | Back / close |
  | Right stick left / right | Snap or smooth turn (Options > VR) | — |
  | Right stick up / down | Next / previous weapon past 0.85 (dominant axis) | — |
  | Right stick click | Crouch (held) | — |
  | Right trigger | Primary fire | Click on the panel (laser), else accept |
  | Right grip | Use the current item past 0.85 | — |
  | A / B | Jump / use (doors, switches) | Accept / back |

  The run toggle of the first layout is gone: only full deflection runs.
- **Bridge.** `xr_apply_controls` (`src/app/xr_player.cpp`) writes the original client inputs:
  `movement_input` (jump from A or the left stick click), `weapon_input` (trigger hysteresis
  0.55/0.35, all hands), `use_down`, `detonate_down`, `prev_item_requested`,
  `next_item_requested` and `use_item_requested` on press edges (the grip with hysteresis
  0.85/0.6), and `GameSession::cycle_weapon` once per stick deflection (0.85 to fire, 0.3 to
  re-arm).
  Outside focus, before calibration or outside the world, every input is neutral. When play
  resumes (menu closed), buttons and triggers still held stay neutral until released, so the
  press that closed the menu neither fires nor jumps; the sticks pass through.
- **Locomotion.** `xr::locomotion` maps the stick to the nearest of the original eight flag
  combinations and folds the residual angle into `CBloodClientShell::locomotion_yaw`, the only
  change to original code: `UpdateMovement` passes it to `CMoveMgr` instead of
  `rotation.yaw` when set (empty on desktop). Per-direction speeds, the diagonal quirk and the
  animations stay original; the reference is the head's yaw.
- **Turning and body yaw.** The tracking base yaw is the app's body yaw while the view is the
  player's eye; snap turns add the Options > VR step to it (15°, 30°, 45° by default, 60° or
  90°, one per deflection: 0.75 to fire, 0.3 to re-arm). Smooth turning adds
  `xr::stick_axis(x) × 120°/s` over the frame step: zero inside a 0.2 dead zone, then rescaled to reach full
  speed at full deflection. Server-driven rotations (start points, teleports, restores) are
  adopted as the difference from the last written aim.
- **Aim.** The right aim pose, placed in the world like the eyes (same lean shift), casts
  `IntersectSegment` (player excluded, 8192 units). `rotation` becomes the angles from the
  original fire origin (`camera_position`) to that point, pitch clamped to ±1.48, so shots,
  USE and every server reader of `m_fPitch/m_fYaw` go where the controller points, with no
  weapon-code change. Without a valid aim pose the head's forward is used.
- **Feedback.** A red laser from the right aim pose to the target and a white reticle sized
  with distance (Options > VR AIM LASER and CROSSHAIR turn each off), drawn by `LineRenderer`
  through `ScenePresenter::eye_overlay` in each eye's scene pass and depth range. The
  controllers themselves are not drawn: the view weapons, or the menu hands, stand at them.
  A 30 ms haptic pulse marks every shot a hand's weapon actually fires (`CViewWeapon::shots`).
- **View weapons per hand.** Each hand's view model follows its own controller (the left one
  for dual weapons); see Arms and view weapons below. An untracked controller falls back to
  the original head placement, enlarged about the eye by the same scale
  (`ViewWeaponCamera::scale`; 1 on desktop, where nothing changes).
- **Dual weapons aim per hand.** `CBloodClientShell::left_hand_forward` (XR adaptation, empty on
  desktop) gives the left-hand weapon the left controller's converged direction in
  `UpdateWeapons`; the left laser and reticle appear with a left-hand weapon.

## Arms and view weapons (lot 8.9)

- **Scale.** The original view models are about 1/8 scale: the Beretta's hand is 0.9 units long
  and the gun 1.3 (its muzzle flash, at 28 units, is ten times the geometry). XR draws them at
  `view_model_meters` = 0.17 m per model unit (`scale = 0.17 × units_per_meter`): hand ~15 cm,
  Beretta ~22 cm, forearm ~29 cm, upper arm ~24 cm. This replaces lot 8.7's life-size
  assumption, which left the arm tiny and behind the controller.
- **Rig.** `rig_view_arms` (`src/engine/view_arms.cpp`) finds every arm chain by the original
  naming: an upper arm `lu_arm*`, its child `ll_arm*`, that one's child `d_null*` (the wrist)
  and the hand mesh under the wrist. Numbers vary (`lu_arm2` is the right arm of the Beretta,
  `lu_arm3` the left of the Mac-10); the side comes from the upper arm's rest x. The rest pose
  is `static_model`, else `idle`, `fire`, then the first clip, at time zero.
- **Grip.** The controller holds, in order: the hand of the chain on the model's own side (the
  centre of the hand mesh's own rest vertices, not what it holds); for the eleven two-handed
  models whose trigger hand is not modelled (M16, sniper rifle, minigun, Tesla cannon, death
  ray, howitzer, napalm cannon, Singularity and the `lh_` M16, napalm and Tesla), an audited
  rest grip measured on their side views; else the bounds centre (no original model needs
  it). The model is turned to the aim pose (pitch, yaw and roll) and moved so the grip point
  sits on the grip pose; animations (draw, fire, recoil) play relative to it.
- **IK.** `solve_arms` (`src/engine/arm_ik.cpp`) runs in `ModelView::add_skinned` after
  `sample_skin` and before the palette push. Each visible chain is re-routed from a virtual
  shoulder to the wrist's animated position: the authored segment lengths, the elbow toward a
  pole (down, outward, back), the authored hinge mapped onto the new bend plane, and the
  shoulder sliding along the reach when the wrist is out of range or too close. The wrist and
  everything under it (hand, gun) keep their animated poses; other nodes under a segment move
  rigidly with it. Model-space composed poses are edited directly, so `model.cpp` is unchanged.
  An invalid chain draws nothing and reports `Invalid arm IK chain`; the CPU skinning path has
  no IK and reports `Arm IK requires GPU skinning` (the presenter always skins on the GPU).
- **Virtual shoulders.** `xr_shoulders` (`src/app/xr_player.cpp`): a neck pivot 10 cm below and
  9 cm behind the eyes in the head frame, shoulders ±17 cm across and 12 cm lower along a body
  yaw that trails the head beyond a 45° dead zone. The body yaw is kept relative to the
  tracking yaw, so snap turns carry it; looking straight up or down keeps it; calibration
  resets it.
- **Framed-out geometry.** The original camera sat at −offset (`m_vViewModelOffset`, mirrored
  for the left hand) with a 90° field of view, and some parts were only hidden by that framing.
  Anything beyond that view cone widened by 1.5 units (~25 cm) stays hidden through the
  per-instance node mask: another chain whose hand lies wholly outside, and any node outside
  the chains whose vertices all do. The held chain is always drawn. On retail data this hides
  the Mac-10's and knife's spare left arms, the Singularity's lowered support arm and the M16's
  grenade; stocks, grips and near-plane cuts stay.
- **Kept from the original.** Two-handed support hands keep their animated place on the gun
  (no foregrip tracking); secondary hands (Voodoo pin, BugSpray can, C4) stay where the
  animation puts them relative to the held hand. Upper arms without geometry (death ray,
  howitzer, napalm, Singularity, Tesla) leave the forearm starting at the elbow.

## Menu hands

- **What.** While the menu panel is shown, each tracked controller holds the current
  character's Beretta view model (`Beretta_pv.abc` right, `lh_Beretta_pv.abc` left, the
  character's `?_Beretta_pv_t.dtx` hands skin; Caleb before a game gives another) with its arm,
  through the same rig, placement, framed-out masks and IK as the view weapons. They play their
  idle clip (else `static_model`) on the frame clock. `CViewWeapon::Create` loads them; nothing
  else of the weapon runs.
- **Where.** `XrFrontend::build_hands` fills `ScenePresenter::hand_scene` and sets
  `draw_hands`; the presenter then records `hand_renderer` in the view-model depth slice in place
  of the paused world's frozen view weapons, or, with no world, `record_hand_eyes` clears each eye
  and draws the hands and the laser. The hands are fully bright (no lighting service) with the
  original shaders. A world revision clears their geometry and textures with the others. A load
  or placement failure is logged once and leaves the hands out; the menu still works.

## HUD and menus (lot 8.6)

- **The frontend in XR.** `--xr` runs the ordinary frontend (`run_frontend`) with an
  `XrFrontend` (`src/app/xr_frontend.cpp`): menus, saves, preferences, credits, the demo end
  and the simulation are the desktop ones. The Quest APK opens the main menu like a desktop
  launch; `--xr --view-world` enters the world directly. The standalone XR world loop is gone.
- **Device and window.** The OpenXR session owns the GPU device (`Runtime::set_device`); the SDL
  window still carries input and ImGui's platform backend but is never presented.
  `xrWaitFrame` paces the loop and the step is the time between the last two predicted display
  times, capped at 100 ms (a fixed display period ran slow frames in slow motion and left the
  audio short, heard as a repeating chunk);
  an idle session only polls SDL for quit.
- **UI canvas.** ImGui renders a fixed 640×480 canvas (the original layout: menu art 1:1,
  `CNewStatusBar` at its original proportions) into one single-layer XR swapchain with the same
  gamma handling as the eyes, composited as an `XrCompositionLayerQuad` 0.70 m wide
  (about 50°) at 0.75 m:
  - menu, credits screen and demo end: a world-locked opaque panel placed when it opens,
    0.75 m ahead of the head, its centre 10 cm below eye level, facing the user; placement
    waits for input focus and is redone after a recentre (the first worn test found it too
    high: poses taken before the startup recentre). The paused world stays visible behind
    it (a dark clear with no world);
  - in play: the head-locked HUD in VIEW space, alpha-blended (premultiplied by ImGui's blend).
- **HUD content.** Status bar, credits roll text, save notice and error modal as on desktop.
  XR hides the screen-centred crosshair (the controller laser and reticle aim instead), the
  touch overlay and the inspector.
- **Fades.** FADEIN/FADEOUT and death fades scale the projection layer with
  `XR_KHR_composition_layer_color_scale_bias` (listed on Quest 3); without it,
  `ScenePresenter::scene_fade` scales the scene light. The canvas is not faded, as on desktop.
- **Menu input.** The controllers become a virtual gamepad (`xr_gamepad_id`) on the ordinary
  gamepad path: left stick navigates, A (or the right trigger off the panel) accepts, B or the
  left trigger goes back, the menu button opens and closes the menu (in play too). A button or
  trigger held across a change between menu and play waits for its release. Session focus replaces window focus: losing it enters as a window focus
  loss, which opens the in-world menu and pauses; SDL window focus events are ignored in XR.
- **Selection laser (lot 8.7).** The right hand points (the left trigger is back). Its aim ray
  meets the panel (`xr::panel_hit`) and becomes SDL mouse events in canvas pixels on the
  ordinary pointer path (`Menu::pointer`: hover selects, press and release on one item
  activate, sliders follow); a right trigger press on the panel is the left button and its
  release always follows, a press off the panel is an accept. ImGui gets the same
  events (the error modal's button). A red beam runs from the controller to the panel (the
  compositor draws the panel over its last part), a red-and-white cursor is drawn into the
  canvas and an 8 ms pulse marks each selection change. Sticks and buttons still navigate.
- **Options > VR.** In an XR session (`Menu::vr`) Options ends with a replacement `VR` row
  opening three replacement rows, labelled like the Display page's: `TURN [SNAP n]` /
  `TURN [SMOOTH]` (cycling 15, 30, 45, 60, 90, smooth), `AIM LASER [ON/OFF]` and
  `CROSSHAIR [ON/OFF]` (the 3D reticle). They persist in preferences version 4 (`vr_turn`
  in degrees, 0 smooth, `vr_laser`, `vr_crosshair`; versions 1–3 load the defaults: 45°, on,
  on) and reach `XrFrontend::set_options` every frame. Desktop menus have no VR row.
- **Diagnostics.** Every 360 frames the log records the canvas mode, whether the world was
  drawn, menu state, ImGui vertices, scene draw calls, controller tracking and close models. Meta's "metacam" screenshots omit
  quad layers; use the `screencap` method (both raw eye images) to see the canvas.

## Conversions

`xr_math.h` converts OpenXR (right-handed, −Z forward, metres) to the engine
(left-handed, +Z forward, units) by the reflection z → −z and a scale:
positions `(x, y, −z)·s`, quaternions `(−x, −y, z, w)`. The asymmetric projection maps
the left/down tangents to NDC −1 and the right/up tangents to +1; a symmetric 90°
FOV gives the desktop projection.

## Verification

- `xr.math_axes`, `xr.math_projection`, `xr.math_eye_pose`, `xr.math_stereo`,
  `xr.input_sectors`, `xr.input_edges`, `xr.input_aim`, `xr.input_hands` and
  `platform.sdl_xr_swapchain_patch` (headless); `xr.player_locomotion`, `xr.player_aim`,
  `xr.player_turning` (snap steps, smooth rate and dead zone, the 0.85 weapon threshold) and
  `xr.player_buttons` (stick jump, previous/next item, grip hysteresis) drive the bridge in a
  synthetic world; `menus.state` walks the VR page and `menus.preferences` version 4;
  `xr.player_shoulders` checks the virtual shoulders and their dead zone.
- `arm_ik.solve_reach`, `solve_pole`, `solve_clamp` and `solve_invalid` (lengths, rigid
  segments, hinge, pole side, shoulder slide, atomic refusal); `arm_ik.rig`, `placement`,
  `hiding` and `model_view` (chains, grips, anchor on the grip, framed-out masks, solved
  palette, CPU refusal) on a synthetic weapon; `arm_ik.catalog` (private) rigs, poses and
  solves all 26 installed view models at their definitions' offsets.
- Regenerating the shaders leaves the eighteen existing artifacts byte-identical; the
  four line artifacts are new (SPIR-V and MSL).
- Device evidence: the [first-light report](../implementation/reports/xr_first_light.md) and
  the [stereo world report](../implementation/reports/xr_stereo_world.md).

## Not included yet

Multiview, the remaining VR options (handedness, movement reference, smooth-turn speed,
HUD distance), the left-handed layout, weapon slots by number, two-handed foregrip tracking
and motion melee, refresh-rate and performance settings,
the data setup screen and demo download, PCVR presets and their profiles.
Shadow casters are still chosen with the 90° desktop frustum around the head, close
models are lit from the game eye, and translucent sorting runs per eye.
