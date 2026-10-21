# Whole-world certification (save/load lot 14)

Date: 2026-10-21. Roadmap task 3.3 (M3), lot 14 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[certified whole worlds](../../contracts/save_load_contracts.md#certified-whole-worlds-lot-14); the
[source ledger](../sources/save_load_certification_sources.json) lists the references consulted.

Every single-player world now saves and loads whole and runs on afterwards. Roadmap task 3.3 is
done. M2 closure and M2-GPU-001 are unchanged.

## What was certified

| Check | Test | Result |
| --- | --- | --- |
| Every single-player world, at entry and after combat: slot file, load, census, identical re-save, 30 s | `save_audit.worlds` (difficulty 1; also run at 0 and 2) | 36 / 36 at each difficulty |
| Restart after a load returns to the level entry, repeatably | `save_audit.restart` (SubwayA) | pass |
| The scripted SubwayA route saves mid-level, loads and still reaches CityHub | `train_session.private_save` | pass |
| GPU readback after a load (Apple M5, Metal) | `gpu.train_session_save`, `gpu.save_native` | pass |
| The private suites of lots 7 to 13 on the restored retail data | `save_*.private`, `save_ai.*` | pass (dev, Release, ASan/UBSan) |

The 36 worlds are every mounted world outside `multi/`. The 22 multiplayer maps are out of scope
(D12). The other audits still enter them: the 18 Bloodbath maps save whole, and lot 13 refuses the
4 CTF maps by name.

### The audit cycle

Each world starts as a New Game. The test sets the player's original god-mode flag, a saved
CDestructable field, so a hostile world cannot end the run early. Two cycles run, each through
`SaveSlots` slot 0 in a scratch directory:

1. **Entry.** The level-entry snapshot is taken first, as the application takes it.
2. **Combat.** The three nearest live NPCs that are not bosses take player-attributed damage, two
   lethal and one wounding. The world then runs three seconds for corpses, weapon drops, gibs and
   effects.

Each cycle does the following:

1. Save, read the file back and decode it.
2. Load through the candidate transaction.
3. Compare the live census of saved classes before and after.
4. Save again. OBJS, LINK and ATCH must equal the file's apart from the listed rewrites
   (`tests/save_rewrites.h`).
5. Run 600 steps of 50 ms. Every step must succeed, with no NPC runtime failure and no
   unavailable service.

| World | Records | Bytes | World | Records | Bytes |
| --- | --- | --- | --- | --- | --- |
| 01_subwaya | 194 | 185,484 | 14_subwayc | 335 | 327,250 |
| 02_cityhub | 348 | 387,936 | 15_crypt | 165 | 172,278 |
| 02_cityhubb | 344 | 375,248 | 16_bridge | 266 | 288,972 |
| 02_cityhubc | 335 | 368,898 | 17_entrance | 351 | 357,384 |
| 03_museum | 444 | 478,303 | 18_temple | 167 | 195,029 |
| 04_steamtunnels | 329 | 359,266 | 19_lobby | 400 | 446,464 |
| 05_cdm | 431 | 482,128 | 19_lobbyb | 101 | 121,642 |
| 05_cdmb | 67 | 71,077 | 19_lobbyc | 96 | 105,204 |
| 06_airship1 | 143 | 157,884 | 20_powerstation | 141 | 149,387 |
| 06_airship2 | 229 | 261,607 | 21_laboratory | 295 | 325,100 |
| 07_cathedral | 197 | 212,636 | 22_office | 494 | 529,738 |
| 08_subwayb | 176 | 178,280 | 23_rooftop | 133 | 119,256 |
| 09_abandoned_buildings | 551 | 584,222 | 24_ancientcity | 203 | 256,974 |
| 10_sewagetreatmentplant | 81 | 108,810 | 24b_arena | 112 | 106,417 |
| 11_slaughter_house | 500 | 555,241 | 25_ancientone | 125 | 144,089 |
| 12_train_station | 439 | 489,948 | credits | 15 | 19,140 |
| 12_train_stationb | 370 | 401,151 | drop | 4 | 8,438 |
| 13_downtown | 279 | 289,984 | end | 14 | 20,500 |

Records and bytes are the largest of the two cycles at difficulty 1; each slot file also embeds the
level-entry snapshot as AUTO. The audit takes about 95 s per difficulty in the dev build.

Always-load objects come back in their authored state (rule 2), so their census differences are
logged, not compared. After combat, consumed Keys reappear (up to 57 in 09_abandoned_buildings), and
SkyPointers that removed themselves after binding return until their first update. One SparksObj
returns in 20_powerstation. The runtime SmellHint trail is lost.

### Restart and the mid-route save

- **Restart** (`save_audit.restart`, SubwayA). The level-entry snapshot is taken at 0 s and slot 1
  at 10 s, carrying the snapshot as AUTO. Loading slot 1 and then Restart, twice, puts the player
  back at the entry position, and a save matches the entry image.
- **Mid-route save** (`train_session.private_save`). The 60 Hz route saves at t = 14.1 s, at the
  train's midpoint (92,386 bytes, 195 records). The container bytes are encoded, decoded and loaded.
  The route then continues on the restored session: 6 Cultists killed in all (4 before the save),
  the relay Trigger2, the black fade and the NEW_LEVEL into CityHub at `01_egressa`, with health
  175 and 31 bullets kept.
  - `gpu.train_session_save` captures the restored game right after the load, and CityHub after the
    transfer.
  - The other route rates (30, 60 and 120 Hz) still pass.

## Findings

- **Lot 6: the level-entry snapshot right after a New Game** (fixed). In 07_cathedral,
  initialization leaves one deferred removal, so `save_game` refused the snapshot and Restart would
  have been empty. `capture_level_entry` now finishes that frame first, as the next frame boundary
  would; `save_slots.restart` checks it.
- **Music routing gaps ended two worlds** (fixed in two separate commits before this lot). Both
  happened with or without a save:
  - 11_slaughter_house and 25_ancientone failed at step 81 (4 s), on a key outside every region of
    Track1's program 13 (key 48; regions 0–11, 12–23 and 60–127).
  - 25_ancientone failed again at step 872 (44 s), on bank 0 program 1, which its bank (programs
    2–16) lacks.

  Each failed note failed the simulation step, and the application exits on that. A received note
  in either case now sounds nothing and is counted, following the DLS rule for an uncovered key and
  a replacement policy for a missing patch
  ([routing contract](../../contracts/music_routing_contracts.md)).
- **save_effects.private and CTF** (test fixed). The test did not accept lot 13's CTF refusal on
  `multi/ctf_cabana`, as the other audits already did.
- **Held weapons of dormant NPCs** (original loss, kept). After the mid-route load, a Cultist that
  was deactivated at the save was killed before it woke: it dropped no gun, and its inventory
  reported "No live current held weapon". The original creates the held model only in
  `CInventoryMgr`'s MID_UPDATE (InventoryMgr.cpp:2701-2711), and an inactive object receives none.
  The original `DropCurrentWeapon` then returns `CHWEAP_NOTAVAIL` silently (InventoryMgr.cpp:1139-1141),
  exactly like the port. The route test accepts this refusal for restored NPCs only.
- **Per-session counters.** Message, fire-key and music-message counters start again with the
  restored session. The route sums them across the load; the restored music level (3) and its loop
  are checked directly.

## Verification

macOS 27.2 (arm64), Apple clang 21, on `019123b0` (lots 1–13) plus the two music commits.

- **Dev** (`dev` preset, `BLOOD2_GPU_TESTS=ON`). Headless with private data: every test passes
  except the 7 known private-data content checks (`doors.catalog`, `pickups.catalog`,
  `ai_recovery.catalog`, `ai_escape.catalog`, `assault.ai`, `mad_scientist.wounds`,
  `lightning.catalog`). Every `save_*` test passes, as do `train_session.*`, the 163 music and audio
  tests, `gpu.train_session_save`, `gpu.train_session` and `gpu.save_native`.
  - The GPU label also fails `gpu.viewport` (a hard-coded content count in the viewport smoke) and
    `gpu.entry_wait` (it expects the pre-menu launch output). Both fail identically on `019123b0`,
    without this lot's changes.
- **Inspector-free Release** (`release` preset). Headless with private data: 1315 of 1322 pass, the
  same 7 known failures. `save_audit.worlds` takes 184 s.
- **Clang ASan/UBSan** (`BLOOD2_SANITIZERS=ON`, inspector off, `detect_leaks=0`, since
  LeakSanitizer is unsupported with Apple clang). No sanitizer report anywhere.
  - `save_audit.worlds` certifies 36 of 36 worlds at difficulty 1.
  - Every private save test of lots 5 to 13 passes: `save_native.private` 1,014 s,
    `save_movers.private` 884 s, `save_effects.private` 831 s, `save_world_logic.private` 635 s,
    `save_pickups.private` 556 s, `save_ai.worlds` 536 s.
  - The private boss and enemy suites lots 11 and 12 left pending pass: gideon_*, enhanced_gideon,
    undead_gideon, ancient_one, naga, leech, later_levels, campaign_expansion.
  - The music, slot and route tests pass: `dls_sampler.*`, `dls_timing.*`, `ima_transport.*`,
    `save_slots.*`, `save_audit.restart`, `train_session.private_save`.
