# M2 continuation: keyboard fire and relative mouse

The reported missing fire/mouse bindings were confirmed in the application input
adapter. Keyboard F/G and captured mouse left/right buttons now feed the existing
original primary/alternate weapon path; relative mouse motion feeds client yaw/pitch.
No direct-shot shortcut, new physics integration or dependency is introduced.

F3 captures the mouse and hides inspection; Escape releases it and restores the
inspector in development builds. F1 also opens inspection and releases capture.
`--view-world` requests capture initially. Existing WASD/ZQSD, jump/crouch/run, arrows,
USE, F2 control enablement and native inspector collapse/resize remain available.
Uncaptured clicks belong to the UI. Focus/text/control gates and player replacement
clear or suppress input to avoid stale firing and accumulated camera jumps.

See [contracts](../../contracts/player_control_contracts.md),
[provenance](../sources/m2_player_control_sources.json) and
[evidence](evidence/m2-player-controls.json).

## Validation

Debug and inspector-free Release each pass **624/624 checks**: 580 headless plus
44 GPU, including 125 private-data checks (91 headless). Debug times are
80.48 s / 176.41 s; Release times
are 12.24 s / 110.41 s.
No GPU validation error/VUID was logged. The private input tests observe 12 keyboard
shots / 38 bullets remaining and 17 combined keyboard/mouse shots / 33 bullets,
with one successfully played original fire voice per shot.

Targeted Clang ASan/UBSan/leak verification covers **52 distinct tests**. The initial
parallel run passed 51 and timed out movement.private at its existing 60-second
limit. The isolated rerun passed with **unchanged code, assertions and timeout**.
The timing failure remains in the evidence; its cause is not established. No sanitizer
error or leak was observed. Both the initial run and rerun are retained.

Commands: `cmake --build build/debug -j3`, `cmake --build build/release -j2`,
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`, then
serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
The existing Clang `build/sanitize` builds player_controls_tests, movement_tests,
interaction_tests, locomotion_tests, weapon_tests and object_contract_tests, then
runs regex `^(player_controls|movement|interaction|locomotion|weapons|objects)\.`
at `-j2` with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
The isolated rerun selects `^movement.private$` with the same environment.

All five changed C++ files pass clang-format 23.1.3; 2,523 local Markdown links
resolve and all 10,923 reference files/six reference repositories remain unchanged.
Evidence includes command logs, selected test transcripts, source hashes and project
commit/patch. No commercial files or new dependencies are committed.

After the full matrix, a final inspector help-wrap and impact-availability text fix
was rebuilt and rechecked with `gpu.weapons` (1/1, 20.77 s). Its local screenshot
was visually reviewed: controls wrap inside the window, capture state is visible
and impact availability is accurate. The capture hash is recorded without importing
commercial pixels. These final text/layout changes are inspector-only.

The existing application weapon GPU scenario now fires through SDL key events,
including while inspection is hidden, and still requires original FIRE_KEY shots,
ammunition, fire/impact audio and unload/re-entry. The new real-window test checks
SDL capture/release, relative motion reaching the camera, mouse primary/alternate
fire, uncaptured UI clicks and focus recovery using actual SubwayA/player/weapon assets.
The private headless test uses the same control adapter and original weapon updates.
Synthetic checks cover key/mouse mapping, gates, bounds, repeat handling and resets.

No human playthrough or retail input-equivalence claim is made. Only this Linux
SDL/Vulkan configuration was exercised. Non-attacking NPCs and the missing train-window
material remain the next priorities. Required original services, safe NEW_LEVEL and
observable M2 acceptance remain incomplete.

The [interactive entry correction](m2_npc_entry.md) now holds application simulation until player entry, preventing the reproduced pre-player NPC suspension. Raw diagnostic stepping retains that boundary; broader combat and M2 remain incomplete.
