# Menu laser and first playtest fixes on Meta Quest 3 (M8 lot 8.7)

2026-10-21, on top of `713ce7ae`. Contract: [OpenXR runtime](../../contracts/xr_runtime_contracts.md).
Plan: [OpenXR plan](../openxr_plan.md).

## User playtest (worn, before this change)

| Report | Change |
| --- | --- |
| The menu panel sits above eye level | Centre 10 cm below the eyes; placement waits for input focus and is redone after a recentre (the startup recentre moved the space under a panel placed earlier) |
| No selection laser in the menus | Lot 8.7: laser from the hand whose trigger was pressed last, pointer events on the canvas, cursor, hover pulse |
| Boxes, ray and reticle good; the arm is tiny | The life-size view model was placed with its camera eye on the controller (the gun ended up ~60 cm beyond the hand). The eye is now placed behind the grip by the weapon's hand point (flash offset, 60% of its depth) |
| The ray drives both arms with dual weapons | Each hand's view model follows its own controller; the left weapon fires along the left controller's converged direction (`left_hand_forward`); a left laser shows with a left-hand weapon |
| A jumps, unlike the gamepad; requested mapping A accept/jump, B back/use, X item+, Y use item, left click run, right click crouch | Applied (left grip: detonate; right grip unbound; flashlight through the inventory) |

Also: buttons and triggers held when a menu closes wait for release before acting in play.

## Results

| Check | Result |
| --- | --- |
| `xr.input_panel` (new), `xr.input_*`, `xr.math_*`, `xr.player_*` (left hand never redirects fire without a left weapon) | Pass (15 XR tests) |
| Quest build | No warnings in the changed files |
| Quest 3, not worn (controllers asleep) | Main menu panel, keyboard navigation to a game, HUD without the centre crosshair, view weapon on the head fallback (1 close model), 72 FPS |
| Desktop GPU suite | 126/127; `gpu.entry_wait` fails as before the XR work |
| Desktop headless suite | 1346/1353; the same 7 private-data catalogue failures as at `c09a9d1f` |

## Not verified

With the headset worn: panel height, the laser and cursor, clicks and sliders, the new
mapping, the weapon's size and grip placement, dual weapons per hand, the hover pulse.
