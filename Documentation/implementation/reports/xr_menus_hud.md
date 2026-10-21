# HUD and menus on Meta Quest 3 (M8 lot 8.6)

2026-10-21, on top of `e6ab5f29`. Contract: [OpenXR runtime](../../contracts/xr_runtime_contracts.md#hud-and-menus-lot-86).
Plan: [OpenXR plan](../openxr_plan.md).

## What changed

- `--xr` now runs the ordinary frontend in the OpenXR session through `XrFrontend`; the Quest
  APK opens the original main menu. The separate XR world loop was removed.
- ImGui renders a 640×480 canvas into an XR quad: a world-locked menu panel at 0.75 m, or the
  head-locked HUD at 0.75 m. The paused world stays behind the menu panel.
- Screen fades use the compositor's colour scale (fallback: scene light scale); the
  screen-centred crosshair is hidden in XR.
- Controllers drive the menus as a virtual gamepad; session focus loss opens the in-world menu.
- The frontend's SDL event handling became one `dispatch` function shared by real and
  synthetic events (desktop behaviour unchanged).

## Results

| Check | Result |
| --- | --- |
| Quest 3, launch without arguments | Main menu panel in both eyes (raw `screencap`), 72 FPS, 2 compositor layers |
| Menu navigation (Android Enter/Escape keys through SDL, controllers asleep) | Single Player, Caleb, difficulty, then `01_SubwayA` loaded; world-locked panel |
| In play | Head-locked HUD at the original proportions (health, armour, ammunition tabs); the canvas switches to HUD mode after the intro cinematic |
| In-game menu | Escape opens the panel over the paused, still-rendered world; Escape closes it |
| Compositor fade support | `XR_KHR_composition_layer_color_scale_bias` listed on Quest 3 |
| Desktop GPU suite (`ctest -L gpu`, includes the menu and demo smokes) | 126/127; `gpu.entry_wait` fails as before the XR work |
| Desktop headless suite | 1345/1352; the same 7 private-data catalogue failures as at `c09a9d1f` |
| XR headless tests | `xr.*` (12) and `platform.sdl_*_patch` pass |

Device screenshots were taken with the `screencap` method; they are not committed
(commercial art).

## Not verified

Controller navigation of the menus and fades on the headset (controllers asleep, no fade
triggered), the error modal and credits in XR, the crosshair removal on the device, the Quest
reporting its controllers as an Android gamepad as well ("Gamepad connected" at launch: check
for doubled menu input in a worn run).
