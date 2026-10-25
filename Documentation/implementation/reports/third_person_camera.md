# Desktop third-person camera — 2026-10-25

C now toggles an optional chase view in ordinary and diagnostic desktop play.
The Controls page exposes the action, the original body and held weapon models
are rendered, and the normal HUD/crosshair stays available. The selection is local
and resets on player/world entry or load. XR refuses it. See the
[camera contract](../../contracts/camera_contracts.md) for framing, clearance,
reticle convergence and special-camera priority.

`controls.cfg` now writes version 2 and accepts version 1. Migration adds C only
when an old custom binding does not own it; a version-2 file can deliberately
leave the action unassigned. Preferences and save formats are unchanged.

## Validation

Inspector-free Release built successfully on macOS (Apple toolchain). The linker
retains the existing duplicate-spdlog-library warning; no compile error occurred.
The focused CTest run passed **31/31**:

- **30 headless tests:** seven camera cases including the new third-person test,
  eleven input-action cases, four original-control cases, five XR player cases,
  HUD values/session and menu state.
- **One private-data Metal GPU test:** `gpu.third_person` enters CityHub with real
  resources, presses C through the action path, renders the scene and actual HUD,
  verifies visible player/held geometry and hidden first-person weapons, then
  toggles back without advancing simulation. The restored first-person pixels are
  byte-identical to the baseline. The captured chase frame was visually inspected.

The new camera case exercises repeat and suppression, unchanged eye/aim state,
near-target crosshair convergence, obstruction and very tight-space fallback,
look pitch, movement, real FIRE_KEY firing while the view model is hidden,
zoom, scripted/death/Eye/Orb priority, VR refusal and exit/re-entry reset.
Input tests cover migration with free/custom C, round trip and deliberate unbinding;
XR input explicitly clears a previously selected chase mode.

Evidence: [focused CTest output](evidence/third-person-release-tests.log).
Captures remain local build outputs in `build/release/tests/third-person-gpu/`
(`first-person.png`, `third-person.png`); no commercial image is committed.
No headset, simulator or emulator was run. This is not retail camera parity or
full campaign camera-clearance certification; M2/M3 acceptance is unchanged.
