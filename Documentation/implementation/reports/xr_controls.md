# Controllers on Meta Quest 3 (M8 lot 8.5)

2026-10-21, on top of `93d4e93e`. Contract: [OpenXR runtime](../../contracts/xr_runtime_contracts.md#controllers-lot-85).
Plan: [OpenXR plan](../openxr_plan.md).

## What changed

- OpenXR `gameplay` action set: move, turn/weapon, fire, alternate fire, use, use item, jump,
  crouch, next item, detonate, run, flashlight, menu, haptics and the hand poses, with Touch
  and simple-controller bindings.
- `xr_apply_controls` feeds the original client inputs: head-relative locomotion through the
  new `CBloodClientShell::locomotion_yaw` (one adaptation in `UpdateMovement`), 45° snap turns
  on an app-side body yaw, weapon cycling, item edges and converged controller aim.
- Wireframe controller boxes, an aim laser and a reticle are drawn in each eye's scene pass;
  shots fired give a haptic pulse on that hand; view weapons follow the right controller.
- The first-light scene now shares `LineRenderer` with the world overlay.

## Results

| Check | Result |
| --- | --- |
| `xr.math_*` (4), `xr.input_*` (4) | Pass |
| `xr.player_locomotion` | Forward walks along the head's forward; a head turned 90° left walks left; full deflection and the run latch run |
| `xr.player_aim` | Converged yaw within 0.01 rad of the body forward from an offset hand; pointing right gives +90°; pointing at the floor converges below; aiming never turns the body |
| `xr.player_turning` | One 45° snap per deflection, back the other way; a server-driven yaw change is adopted; the vertical axis cycles weapons without turning |
| `xr.player_buttons` | Trigger hysteresis, held use/jump/crouch, one next-item request per press |
| Desktop headless suite (`ctest -L headless`, macOS `dev`, retail data) | 1345/1352 pass. The 7 failures (`doors.catalog`, `pickups.catalog`, `ai_recovery.catalog`, `ai_escape.catalog`, `assault.ai`, `mad_scientist.wounds`, `lightning.catalog`: private-data catalogue expectations) fail identically at `c09a9d1f`, before the XR work |
| Quest build | No warnings in the changed files |
| Quest 3 smoke run (not worn) | SubwayA at 72.0 frames/s, every frame rendered, no errors; Touch profiles bound; inputs neutral while the controllers sleep |

## Not verified

Play with the headset worn: walking, snap turns, aiming and firing with the real
controllers, the laser and reticle readability, haptics. These need the user.
