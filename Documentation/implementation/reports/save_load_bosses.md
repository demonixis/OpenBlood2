# Bosses and special enemies (save/load lot 12)

Date: 2026-10-21. Roadmap task 3.3 (M3), lot 12 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[implemented bosses and special enemies](../../contracts/save_load_contracts.md#implemented-bosses-and-special-enemies-lot-12);
the [source ledger](../sources/save_load_bosses_sources.json) lists the references consulted. Gideon,
EnhancedGideon, UndeadGideon, AncientOne, AncientOneTentacle, Naga, ZealotAI, BoneLeech, Thief,
TheHandAI and DrudgePriest now save and load on top of lot 11's AI_Mgr block; their 11 classes are
Original with payload version 1. With lot 11, every AI_Mgr class the worlds place is supported. M2
closure and M2-GPU-001 are unchanged.

## What the original does

The [character audit](../save_load_audit/characters.md) (§9–§17, §23) gives the blocks. The bosses
save their health reference and raw absolute clocks; the boss bars, Gideon's fire node and
EnhancedGideon's no-fire flag are not saved and come back from the constructor or the initial update,
which runs on every path. BoneLeech, Thief and TheHandAI save their own jump flag and two raw clocks;
on `MID_LOADOBJECT` they re-point AI_Mgr's `m_hEnemyAttach` to the single player, restart
`STATE_EnemyAttach` and replay the attach handshake. Naga writes its whomp count as a DWord and reads
it as a Float, and saves its spike as a plain object although the spike is an always-load Model; its
Load then orphans the spike its initial update created. Zealot saves its staff-start flag without the
staff object.

## Delivered behavior

- **Bodies.** `BossSave.cpp` holds the eleven blocks over lot 11's `SaveSubclass`/`LoadSubclass`
  hooks; BoneLeech also overrides `AfterLoad` and the enemy-attachment slot, and Thief and TheHandAI
  inherit its block. Naga's initial update clears SHADOW and SOLID for a fresh object only; Zealot's
  fire command clears a restored staff start that has no staff.
- **Checks before change.** Every field is checked first (finite clocks, nonnegative health, a
  positive tentacle scale, flag bytes, the constant `m_fLastFire` and `m_fLastIdle`, an attached
  object that is the player, a tracked object that is not the creature itself).
- **Declarations.** The 11 classes are Original, version 1; the inventory validator admits Gideon's
  strength 10.

## Findings

- **Shield clock.** Gideon's shield needs armor at most 25 and a last shield older than 60 s. The
  game clock starts at 0, so no shield is granted in the first minute; the test sets the last shield
  to −61 as a labelled fixture.
- **Attach replay.** A restored creature restarts STATE_EnemyAttach as the original does, so its
  record differs right after a load (Metacmd 1, its last state) and the client acknowledges again;
  the creature feeds again within a few updates.
- **Idle guard.** With the AncientOne case, a tentacle that drops its target diverges after 36
  steps: the original never saves `m_fWaitForIdleTime`/`m_bHasTargeted`, so the restored tentacle
  goes Idle where the saved one keeps searching (lot 11's kept loss). The test compares 30 steps and
  checks the loss.
- **Projectiles.** Gideon's shots are lot 9 classes, still Unsupported: the tests save on frames
  without a shot in flight, and the continuation stops at the first unsaveable frame (73 steps in
  the teleport case).
- **Naga spike.** The departure keeps the spike the restored initial update creates and attaches;
  the original orphaned it (attached, never hidden, shown or removed again).

## Tests

`save_ai_tests` gains the eleven classes in `classes` (idle, 90 identical steps each) and the private
`bosses` case. The shared `save_ai_fixture.h` lists the NPC classes and applies the listed payload
rewrites, also for `save_world_logic`'s real-world restores.

| Case | Checks |
| --- | --- |
| `classes` | Gideon, EnhancedGideon, UndeadGideon, AncientOne, AncientOneTentacle, Naga, ZealotAI, BoneLeech, Thief, TheHandAI and DrudgePriest idle: same state, weapon, table and movement cache, 90 identical steps. |
| `bosses` | Gideon attacking the player with its shield (target, shield clock, health reference, fire node, 90 steps); Gideon mid-teleport (73 steps, until its next shot); a BoneLeech feeding on the player (attached, tracked and linked again, the handshake replayed with the restored client, feeding resumes); the Ancient One with two placed and one spawned tentacle (all restored, 30 steps, then the idle-guard loss); a Naga with a whomp count (reloads 0, spike kept and attached). |
| `worlds` | Every NPC world restores all supported objects, the bosses included, and its NPCs run on. |

Results on macOS (Apple clang 21), on top of lot 11. Dev and inspector-free Release, headless and
private: 1293 of 1300 tests pass; the seven failures are the known private-data content checks
(doors, pickups, ai_recovery, ai_escape and lightning catalogs, assault.ai, mad_scientist.wounds).
All save tests pass. Clang ASan/UBSan (`detect_leaks=0`): no sanitizer report; the headless suite
(950 tests, the object contract suite included) passes, and so do the private save_ai cases.

The retail data used by the private tests was erased at 01:32 on 2026-10-21 by a manual run of
another lot (since guarded on main). The Dev and Release runs above finished before it; the ASan
private runs of the boss and enemy suites (gideon_*, enhanced_gideon, undead_gideon, ancient_one,
naga, leech, later_levels, campaign_expansion) and of save_world_logic/save_native did not, and
must be re-run once the data is restored.

## Real worlds

58 of 58 worlds enter; 35 hold NPCs (1129 in all) and all of them restore every
supported object through `load_game` with every NPC running. No AI_Mgr class blocks a world any
more: the remaining blockers are the pickups and projectiles of lot 9 and the effect publishers and
emitters of lot 10. Only `drop` restores whole.

This sweep ran before lot 10 landed. Lot 10 now supports the classes of the last column, so the
worlds with no lot-9 class (01_subwaya, 06_airship1, 08_subwayb, credits, end) are expected to
restore whole with lots 10 and 12 together; that is not verified, because the combined sweep could
not run without the retail data. It must be re-run once the data is restored.

| World | NPCs | Restored records / NPCs | Lot 9 classes | Lot 10 classes |
| --- | --- | --- | --- | --- |
| 01_subwaya | 16 | 159 / 16 | — | 4 |
| 02_cityhub | 27 | 306 / 27 | 9 | 6 |
| 02_cityhubb | 27 | 294 / 27 | 7 | 6 |
| 02_cityhubc | 38 | 279 / 38 | 12 | 6 |
| 03_museum | 39 | 424 / 39 | 4 | 4 |
| 04_steamtunnels | 52 | 283 / 52 | 6 | 4 |
| 05_cdm | 56 | 409 / 56 | 9 | 4 |
| 05_cdmb | 5 | 58 / 5 | 2 | 1 |
| 06_airship1 | 28 | 135 / 28 | — | 1 |
| 06_airship2 | 34 | 218 / 34 | 7 | 2 |
| 07_cathedral | 17 | 157 / 17 | 10 | 4 |
| 08_subwayb | 21 | 153 / 21 | — | 3 |
| 09_abandoned_buildings | 62 | 489 / 62 | 14 | 7 |
| 10_sewagetreatmentplant | 36 | 74 / 36 | 6 | 1 |
| 11_slaughter_house | 68 | 476 / 68 | 12 | 3 |
| 12_train_station | 66 | 389 / 66 | 15 | 5 |
| 12_train_stationb | 44 | 357 / 44 | 4 | 2 |
| 13_downtown | 46 | 238 / 46 | 15 | 4 |
| 14_subwayc | 29 | 304 / 29 | 8 | 3 |
| 15_crypt | 26 | 128 / 26 | 15 | 1 |
| 16_bridge | 35 | 230 / 35 | 14 | 3 |
| 17_entrance | 25 | 321 / 25 | 6 | 2 |
| 18_temple | 39 | 145 / 39 | 12 | — |
| 19_lobby | 55 | 377 / 55 | 6 | 1 |
| 19_lobbyb | 19 | 94 / 19 | 7 | — |
| 19_lobbyc | 10 | 79 / 10 | 8 | 1 |
| 20_powerstation | 25 | 116 / 25 | 7 | 2 |
| 21_laboratory | 49 | 263 / 49 | 8 | 3 |
| 22_office | 38 | 479 / 38 | 8 | 1 |
| 23_rooftop | 3 | 73 / 3 | 12 | 1 |
| 24_ancientcity | 62 | 178 / 62 | 14 | 1 |
| 24b_arena | 5 | 64 / 5 | 14 | 3 |
| 25_ancientone | 22 | 97 / 22 | 7 | 2 |
| credits | 1 | 12 / 1 | — | 2 |
| drop | 0 | — | none | none |
| end | 4 | 13 / 4 | — | 1 |
| multi/bb_area15 | 0 | — | 16 | — |
| multi/bb_bloodarena | 0 | — | 20 | — |
| multi/bb_bloodkeep | 0 | — | 16 | 2 |
| multi/bb_bodies | 0 | — | 15 | — |
| multi/bb_bridge | 0 | — | 13 | 3 |
| multi/bb_brownstar | 0 | — | 15 | 2 |
| multi/bb_cabana | 0 | — | 17 | — |
| multi/bb_chosenarena | 0 | — | 15 | — |
| multi/bb_condemned | 0 | — | 18 | 3 |
| multi/bb_crypt | 0 | — | 12 | 2 |
| multi/bb_deathcanyon | 0 | — | 11 | — |
| multi/bb_drain | 0 | — | 14 | — |
| multi/bb_dustbowl | 0 | — | 10 | 1 |
| multi/bb_fields | 0 | — | 10 | — |
| multi/bb_jgrave | 0 | — | 19 | 1 |
| multi/bb_midgard | 0 | — | 13 | — |
| multi/bb_power | 0 | — | 14 | 1 |
| multi/bb_rooftop | 0 | — | 14 | 1 |
| multi/ctf_cabana | 0 | — | 17 | — |
| multi/ctf_deadcities | 0 | — | 18 | — |
| multi/ctf_dmx | 0 | — | 15 | — |
| multi/ctf_jam | 0 | — | 21 | — |

## Remaining

Lots 9 and 10, then the certification of lot 14 (whole worlds, the SubwayA route, Restart, GPU
readback). The player-continuation observations of lot 11's evidence still apply.

## Re-run on the restored data

Re-run on 2026-10-21 (save/load lot 14), after the retail data was restored, on `019123b0` plus
the two music commits, macOS, Apple clang 21. Dev and inspector-free Release, headless with private
data: every save test passes; the only failures are the 7 known private-data content checks. Clang
ASan/UBSan (`detect_leaks=0`) ran the private boss and enemy suites (gideon_*, enhanced_gideon,
undead_gideon, ancient_one, naga, leech, later_levels, campaign_expansion) and every private save
test of lots 5 to 13: all pass, with no sanitizer report. `save_ai.worlds` (536 s under ASan) restores 54 of 58 worlds whole, including
01_subwaya, 06_airship1, 08_subwayb, credits and end, which this report expected but could not
verify; the 4 CTF maps are refused by lot 13. The whole-world certification of lot 14 then saves,
loads and runs on all 36 single-player worlds ([evidence](save_load_certification.md)).
