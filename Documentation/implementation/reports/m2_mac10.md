# M2: Original Ingram Mac-10 and dual-hand animation fallback

Project commit: `f6df7536b451e8e2b50b26d0452a9bda766fccb5`.

The original Mac-10 now uses the existing vector, ammo, damage, hearing and fire-audio
services. SubMachineGunPU retains its authored properties and 20 bullets. Players
can collect two guns, restore their inventory and use either hand. NPCs parse the
original SUBMA prefix, fire through their existing AI update and drop a real pickup
through the independently owned hand model. No reference or commercial asset changed.

[Contracts](../../contracts/mac10_contracts.md),
[source ledger](../sources/m2_mac10_sources.json) and
[retained evidence](evidence/m2-mac10.json) define the supported boundary.

The actual left Mac-10 ABC contains static_model, draw and fire only. Integration
initially exposed a missing original CViewWeapon::SetupViewModel rule: when alt_fire
is absent, the entire alternate start/fire/end sequence uses its primary counterpart.
The port now restores that rule, as well as the source idle-to-static_model fallback.
This is an original behavior import, not an invented animation or asset edit.

In each of two actual SubwayA load/entry cycles, after the original intro expires,
four 1.8-second held-input windows produce **23 right primary, 7 right alternate,
23 left primary and 23 left alternate shots**. Each matches the delivered FIRE_KEY
count and bullet consumption; releasing input ends firing. The differing left/right
alternate cadence follows the original models/fallback. The source .1/.5-second
reload fields are state-entry gates, not a new per-shot limiter. These are repeatable
port observations, not original-executable timing measurements.

A separately labelled synthetic arena uses actual Cultist/player/model/audio assets,
original NPC initialization and autonomous AI_Update. The Cultist chooses the Mac-10,
fires 37 shots over ten seconds and reduces player health to zero without forcing a
target or invoking the fire function from the test. This validates the selected
weapon path; it does not establish full campaign encounter parity.

Across 57 readable worlds, all four authored SubMachineGunPU entries create with
real models and the original weapon ID, resource ID and 20-bullet value. The full
supported-pickup catalog now creates 1,263 instances with no missing model; the
same 176 historical TypeAction schemas remain rejected. The original catalog test
first failed on its old 1,259 total; its new assertions check all four Mac-10 entries
and retain every previous failure expectation.

CityHub's authored SubMachineGunPU now creates with its real model. Counts are
**419 created / 31 transferred sounds / 541 editor-only / 31 unsupported / four
failed**. Structural preparation becomes **454 checked / 31 unknown / zero invalid**;
the isolated destination rehearsal retains the original source world/player. The
four pre-existing initialization failures remain explicit. SubwayA entry accounting
is unchanged. No automatic NEW_LEVEL transfer is enabled.

The Weapons inspector exposes loop/reload values, actual clip/fallback, original
paths, shot/key/ammo/audio state, and an explicit diagnostic grant (repeat for the
second gun). Existing pickups, attachments, NPCs and spawn history retain hidden,
removed and failed objects. A GPU probe renders both actual textured view models,
reads non-background pixels on both sides, and verifies cleanup/reload independently
of any inspection UI. Its captures remain private in ignored build directories.

Chrome/environment mapping, multiplayer damage boost, recoil, muzzle flashes,
casing ejection and stock-fold SOUND_KEY playback remain unavailable. Actual fire
WAVs are played through the existing service; model-key stock sounds are indexed
only. Player-initiated drops and other unsupported weapon classes remain unavailable.
Required remaining services and safe NEW_LEVEL keep **M2 incomplete**.

## Validation

Linux validation completed:

- GCC Debug and inspector-free Release: **679/679 headless tests each**, including
  109 private-data cases (105.81s / 20.73s).
- **6/6 focused Vulkan GPU tests each**, all using private assets
  (59.29s / 27.78s): original weapons,
  Mac-10 pixels/lifetime, model presentation, objectives/destination rehearsal,
  player restoration and KeepAlive. The Mac-10 produces 25,818 left / 25,776 right
  non-background pixels on each of two loads; the Debug capture was visually checked.
  The existing weapon smoke also exercises hidden inspection and reload. This is
  focused GPU coverage, not a rerun of every unrelated GPU test.
- **114 distinct Clang ASan/UBSan cases pass**, with leak detection, including
  18 private-data cases: 113 passed in the initial 304.95s suite;
  the timed-out case passes its isolated rerun in 100.59s.
  Neither retained run contains an ASan/UBSan diagnostic; exercised GPU logs
  contain no Vulkan validation diagnostic.
- 25 changed C++ files pass formatting; 2,895 local Markdown links resolve.
  All 10,923 inventoried reference files match their hashes and all six reference
  repositories remain clean. All Mac-10 WeaponData tokens match the original
  after excluding comments/whitespace. Dependency pins are unchanged.

The initial ASan/UBSan pass completed 113 cases but timed out the new two-cycle
Mac-10 private scenario at 90 seconds. Only its sanitizer timeout is raised to
240 seconds, following the existing private weapon-test policy. Every frame,
mode, reload and lifetime assertion is retained; the isolated rerun is recorded
separately from the original suite log.

Commands run from `Blood2/` (external data and GPU testing remain configured in
ignored local presets/cache):

```sh
cmake --build build/debug -j4
cmake --build build/release -j4
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
ctest --test-dir build/debug -R '^gpu\.(mac10|weapons|model_private|objectives|player_restore|player_keepalive)$' --output-on-failure
ctest --test-dir build/release -R '^gpu\.(mac10|weapons|model_private|objectives|player_restore|player_keepalive)$' --output-on-failure
cmake --build build/sanitize --target mac10_tests weapon_tests weapon_drop_tests inventory_state_tests ai_fire_tests ai_runtime_tests object_contract_tests services_tests world_tests weapon_audio_tests weapon_impact_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize -j2 -R '^(objects|services|world|weapons|weapon_drops|inventory_state|ai_fire|ai_runtime|weapon_audio|weapon_impacts|mac10)\.' --output-on-failure
cmake --build build/sanitize --target mac10_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize -R '^mac10.private$' --output-on-failure
```

No Windows/macOS/mobile/XR execution or full-level gameplay/retail comparison was
performed. M3–M8 remain later milestones; this increment does not close M2.
