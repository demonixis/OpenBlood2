# Additional original control actions — 2026-10-25

The nine previously disabled Controls rows now execute gameplay behavior and can be
assigned through the existing keyboard/mouse/gamepad rebinder. Defaults stay unbound;
existing user bindings are retained. This supersedes the NOT IMPLEMENTED presentation
recorded in the earlier tester-feedback report.

See the [behavior and departures](../../contracts/input_action_contracts.md#additional-original-controls-2026-10-25)
and [source ledger](../sources/control_actions_sources.json). Mouse Look is enabled in the
retail Mouse menu. Preferences migrate transactionally to version 11. Message is a bounded
local solo echo, not network chat; no cheat parser or multiplayer support is added.

## Verification

- Release application and affected test targets build on macOS arm64. Inspector and frontend
  translation units also compile with `BLOOD2_INSPECTOR=1` (syntax check, not a second linked build).
- 66 targeted Release cases pass: `control_actions.*`, `input_actions.*`, `weapon_drops.*`,
  `movement.*`, `locomotion.*`, `cameras.*`, `menus.preferences/state/input`, `player_voice.*`.
- Four new cases cover actual movement, one-shot half-turn/cancellation, held mouse aim,
  persistent auto-run, crosshair cycle/zoom admission, text editing/suppression/bounds,
  left-first dual player drops with both view-model lifetimes, taunts and moveable-only Grab.
  The real rebinding test visits all nine formerly disabled rows and assigns them.
- 24 input/control/drop cases pass with AddressSanitizer and UndefinedBehaviorSanitizer,
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
  The initial leak-enabled run was rejected at startup: LeakSanitizer is unsupported on
  this macOS ARM platform. It is not counted as a passing leak check.
- The final 20-case input/preferences/control/GPU selection passes. GPU cases are
  `gpu.menus`, `gpu.menu_traversal`, `gpu.movement`, `gpu.cameras`. Menu traversal now opens
  the editor via a bound key, checks SDL text-input ownership, sends a text event, verifies
  the local echo and release, then executes RunLock and Crosshair through the frontend.
- Extended `player_voice.private` passes with the retail data: a typed Taunt request starts
  the original streamed taunt WAV, advances its audio cursor and refuses immediate repetition
  through the existing cooldown. This supplements the synthetic command-routing check.
- `cmake --build build/quest --target blood2_apk -j6` assembles the Quest APK. No headset,
  simulator or emulator was run. Windows and physical touch/controller testing remain unperformed.

Logs are retained under `build/release/tests/control-actions/` (ignored local output).
No commercial assets or save payloads change. This does not certify campaign traversal,
resolve the unreproduced Museum crash, or alter the M2/M3 acceptance boundaries.
