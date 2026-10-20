# Reported level loading — 2026-10-20

The report from `bbc7ef4-dirty` predates several merged campaign continuations.
The initial audit on merged baseline `598d294` reproduced the remaining failures
using the external retail 2.1 installation and `--start-game`, difficulty 1.
This checks admission, player entry, one 1/60-second update and admission again.

Passed: 01_subwaya, 02_cityhubc, 03_museum, 05_cdm, 05_cdmb,
10_sewagetreatmentplant, 14_subwayc, 15_crypt, 16_bridge, 17_entrance.

Still failed: 09_abandoned_buildings, 11_slaughter_house, 12_train_station,
12_train_stationb, 13_downtown, 18_temple. Slaughter House passes entry but
CivilianAI0 requests an absent animation on the first update; earlier entry-only
success does not certify this stricter check.

## First correction

- Eight Train Station Sound entries author InnerRadius > OuterRadius. Retain
  the original values and apply the later LithTech `CSoundInstance` inner-first
  attenuation branch. This yields full gain through InnerRadius, then silence,
  without division by zero. Both radii remain finite and bounded by one million.
  Reference: `sources/lithtech/runtime/sound/src/soundinstance.cpp:1195–1209`.
- Account for the exact missing Sound references `""`,
  `sounds\ambient\sewerdrip1.wav` (streamed),
  `sounds\ambients\amb1.wav` and `sounds\ambients\neonloop1.wav` using the
  existing class/property/path/failure-category omission mechanism. No aliases,
  replacement audio or broad suppression of resource failures are introduced.
- Temple's DieBugDieAmmoPU0 is collected by an AI during its initial ground
  placement. The acknowledgement removes it before the loader examines it.
  Previously the loader treated it as a creation failure and reused an unrelated
  old sprite diagnostic. `CollectedPickup` records this verified original
  lifecycle separately, without an object handle or a false failure.

`reported_level_tests` exercises the actual `start_new_game` transaction, all
three difficulties, tiny initial frame deltas, movement, three seconds of
simulation, a final startup/runtime recheck and cleanup. The final recheck is
deliberate: `step()` can succeed while an NPC suspends with a durable failure.
The collection case checks Temple's consumed source row and the AI acknowledgement.
Sound tests check both sides of the inverted-radius boundary and preserve refusal
of unbounded radii, absent resources and over-budget voices.

M2 closure and reservation M2-GPU-001 are unchanged. Startup tests do not certify
full traversal, all combat/script branches or multiplayer support.

## Verification of the first correction

- Dev build; `ctest --test-dir build/dev -R '^(sound|randomsound|reported_levels)\.'`
  passes all 20 tests ([log](evidence/reported-level-dev-tests.log)).
- Inspector-free Release application and test build; `sound.*` and
  `reported_levels.*` pass all 11 tests
  ([log](evidence/reported-level-release-tests.log)).
- Clang ASan/UBSan, leak detection and halt-on-error enabled: the same 11 tests
  pass ([log](evidence/reported-level-sanitize-tests.log)).
- Real-display Release on NVIDIA RTX 3060 Laptop GPU:
  `--view-world --frames 3` for both
  [12_train_stationb](evidence/reported-level-train-stationb-gpu.log) and
  [13_downtown](evidence/reported-level-downtown-gpu.log) exits 0. Audio uses SDL's
  dummy driver; this is rendering/lifetime coverage, not an audible assessment.
- The [initial wider regression run](evidence/reported-level-initial-regressions.log)
  passed object lifetime, stream and most world tests. Two old sound expectations
  were updated for the eight now-playable inverted-radius loops and pass above.
  `world.destination_private` remains a failure: it expects 39,745 CityHub
  collision triangles but measures 39,724. Collision/preparation code is unchanged
  by this correction; the broader suite is **not** claimed green.
- `git diff --check` and clang-format checks pass.

The reusable audit command is:

```sh
./build/dev/tests/reported_level_tests audit "/path/to/Blood II"
```

It enumerates mounted `worlds/*.dat`, reports rejected source rows, exercises all
three difficulties when possible, and returns nonzero if any world fails.
