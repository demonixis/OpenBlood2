# M2 continuation: original vector impacts and material sounds

Beretta/knife now execute the original material-dependent vector continuation,
selected client impact WAV playback and impact hearing. Glass can transmit a bullet
to a second target without a second ammunition deduction. Brush/door FireThrough,
liquid branch order, melee exclusions and character hearing exclusions follow the
original code. Player and AI inspection exposes each ordered contact. **M2 stays open.**

See [contracts](../../contracts/weapon_impact_contracts.md),
[source ledger](../sources/m2_weapon_impact_sources.json) and
[evidence](evidence/m2-weapon-impacts.json).

The preserved source quirks include primary-world FIRETHROUGH stopping in the general
branch, knife ignoring destructible-brush FireThrough, and brush continuation using a
resolved material while audio uses the earlier raw surface byte. Default liquid
brushes do not invent a water material. Character initialization supplies the original
FLESH surface byte. The 1.25-normal sound offset, 500/50 radii, low priority and B2_LITE
impact1..3 selection are retained. These are real decoded spatial voices; impact
visuals/decals, reverb and priority arbitration remain unavailable.

Typed rays retain texture flags and use SOLID/RAYHIT objects, a volume filter for eye
alignment and an explicit interior-entry policy. The latter is documented replacement
physics informed by a later engine generation. Contacts advance one unit while keeping
the original endpoint. A bound of 64 contacts prevents an unbounded original loop;
exceeding it is an explicit partial-shot failure. Callback removal is deferred and
rechecked before damage. Detached impact voices survive removal and end on sample
completion or unload. No mutable original global or dependency is added.

Synthetic tests cover material selection/silent RNG, world glass and melee, primary
FIRETHROUGH, box/polygon brushes, brush/door differences, raw/resolved materials,
RAYHIT liquid/nonliquid volumes, eye filters, interior/invalid queries, callback order,
shooter/target removal, flesh/character hearing exclusion, voice lifetime, missing
resources and the exact 64/65-contact boundary. Private tests fire four directions
after the original SubwayA intro across three unload/re-entry cycles and supplement
those shots with explicitly diagnostic material-audio requests. Each private cycle
records six contacts and three impact voices from four shots, with no audio failures;
the diagnostic requests cover ten material families plus the building/stone alias.
These checks do not claim rendered impact visuals or retail behavioral comparison.

The GPU weapon scenario requires actual impact voices and no impact-audio failures
while the UI is hidden, alongside original FIRE_KEY activity and SDL sample submission.

## Validation

Debug passes **569/569 headless** (81.44 s) and
**41/41 GPU checks** (166.41 s). Inspector-free Release passes
**569/569 headless** (7.08 s) and **41/41 GPU checks**
(108.87 s): **610/610 per configuration**, including
121 private-data checks (89 headless). Targeted Clang
ASan/UBSan/leak coverage passes **105/105** after correcting the obsolete glass-hit
assertion and rerunning that case (initial 104/105, recheck 1/1; 251.78 s total).
Neither GPU matrix emits a Vulkan validation error or VUID.

Commands: `cmake --build build/debug -j3`, `cmake --build build/release -j2`,
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`, then
`ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers use
existing Clang `build/sanitize`, weapon_impact_tests, weapon_audio_tests, weapon_tests,
weapon_drop_tests, object_contract_tests, ai_trigger_tests, ai_fire_tests, ai_runtime_tests,
world_tests, player_restore_tests and player_keepalive_tests. Run with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` and CTest
regex `^(weapon_impacts|weapon_audio|weapons|weapon_drops|objects|ai_trigger|ai_fire|ai_runtime|world|player_restore|player_keepalive)\.` at `-j2`.

`--inspect-player` exits 0; `--start-game` retains exit 6. All 16 changed C++ files
pass clang-format 23.1.3; 2,482 local Markdown links resolve. All 10,923 reference files
and six reference repositories remain unchanged. Evidence includes logs, test selection,
source hashes, project patch/commit and a reviewed Debug inspector capture record.
Captures remain ignored. Audio proof uses decoded/mixed/submitted samples, not pixels.

Initial failures are retained: typed Damage needed namespace qualification because
an original volume class shares its name. Synthetic door/volume creation initially
supplied the brush-only HitPoints property; fixtures now respect the strict schemas
and existing movement/container contracts. Review also found the exact-64/miss boundary;
64/65 cases are explicitly tested. The old AI clearance assertion expected only the
glass hit; it now requires ordered glass and target damage with one ammunition
deduction. No source behavior or acceptance test was weakened.

SubwayA source accounting remains 203 created, four transferred sounds, 372 editor-only,
one unsupported Gideon, zero failures. Safe NEW_LEVEL and required remaining services
keep M2 incomplete; destination rehearsal does not authorize runtime promotion.
M3–M8 remain later milestones. External assets, references and pinned stack are preserved.
