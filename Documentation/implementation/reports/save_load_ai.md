# AI core (save/load lot 11)

Date: 2026-10-21. Roadmap task 3.3 (M3), lot 11 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[implemented AI core](../../contracts/save_load_contracts.md#implemented-ai-core-lot-11); the
[source ledger](../sources/save_load_ai_sources.json) lists the references consulted. AI_Mgr's
original block with its script and path lists, CultistAI, CivilianAI, Fanatic, CCorpse and every
AI_Mgr-only subclass now save and load; their 18 classes are Original with payload version 1. A
save of a real game is still refused by name, because every world except `drop` also holds
classes of lots 9, 10 or 12. M2 closure and M2-GPU-001 are unchanged.

## What the original does

The [character audit](../save_load_audit/characters.md) (§1–§8, §18, §22, §23) gives the
blocks. Each NPC writes its class block, then AI_Mgr's 48 fields, then CBaseCharacter's empty
block and the aggregates (CMovement, CInventoryMgr, CDestructable). `MID_LOADOBJECT` runs the
class and AI_Mgr Loads, then `InsertMe`, then the aggregates. On `PRECREATE_SAVEGAME` no
property is read; on `INITIALUPDATE_SAVEGAME` the subclass binds its statics and AI_Mgr
initializes the inventory and damage owners, then returns. Relative clocks are
`m_fTimeStart` and `m_fLoadTimeStart`; the wait deadline, Fanatic's fuse and the ledge clock are
raw. The original adds `GetTime()` back to the raw `m_fLastUpdate` and `m_fLastLedgeCheck`;
the second suppresses ledge checks for about the saved game time. The idle guard, the smell
cursor, burning sources, the attacker memory and the pending self-removal are lost.

## Delivered behavior

- **Bodies.** `AISave.cpp` holds AI_Mgr's Save/Load and the tail; CultistAI, CivilianAI and
  Fanatic override `SaveSubclass`/`LoadSubclass`; CCorpse dispatches its own Save/Load before
  its damage aggregate. AI_Mgr's `EngineMessageFn` keeps the original order, including
  `InsertMe` before the aggregates and an `AfterLoad` hook after them.
- **Restore branches.** `RuntimePrecreate` accepts `PRECREATE_SAVEGAME` without properties or
  random draws; `RestoreInitialize` binds the shared table (InitializeSharedTable, now shared
  with fresh initialization) and the inventory and damage owners, and requires the model. The
  SoulDrudge constructor's weapon draw runs at the start of a fresh precreate instead, so a
  restore leaves the generator untouched.
- **Port tail.** Attacker memory, smell cursor, sought pickup (LoadSave references), pending
  self-removal, target and tracked-position flags, the failed-plan latch, script and motion
  outcomes, and the runtime failure with its text.
- **Checks before change.** Every field is decoded and checked first; a refusal names the
  first bad field through `RejectPayload`. Counts stay aligned after an earlier refusal.
- **Damage block.** CDestructable carries an NPC's unvalidated authored destruction spawn.
- **Declarations.** `registration.cpp` declares the 18 classes Original, version 1.

## Findings

- **Ledge clock (decision 4).** Loading the saved deadline raw makes the restored throttle run
  from it; the layout test steps 0.1 s after a load and sees a real ledge query.
- **Attacker memory and pending removal (decision 4, found here).** A recoiling Cultist keeps
  its unconsumed memory and selects the attacker after the recoil exactly as the saved world
  does (150 identical steps). A Civilian saved between its corpse transfer and its removal
  removes itself instead of transferring a second corpse.
- **Unvalidated NPC spawns.** 14_SubwayC's Prophet (`LifeSeedPU`) and 16_Bridge's Fanatic
  (`BullettammoPU`) carry authored destruction requests that fail validation. AI precreate
  admits them (the death spawns nothing), but the damage block refused their save. Neither
  name exists in the original tree; `LifeSeedPU` is also absent from the retail `object.lto`
  (BLOOD2P2.REZ holds `AngerPU` once and no `LifeSeedPU`; the only occurrence is that property
  in BLOOD2.REZ).
- **Exit hints.** `FindVisible` never accepts an ExitHint: CanSee needs a ray hit on the
  target. A scared civilian's EscapeRun therefore falls through to EscapeHide, as in the
  original; the escape case saves that state.
- **Player continuation (lot 5, observed).** Three player rules made the saved and the restored
  worlds part, none of them AI state: the unsaved `m_fNextPingTime` (an original loss: the
  restored player pings the NPCs at once), the client attach resetting a moving player's
  acceleration, and the restored player's first update restarting its clip. Original
  `CPlayerObj::Load` also zeroes a knocked-back player's velocity. The AI tests carry the ping
  clock over, keep the target still and ignore those player fields; lot 14 should decide them.
- **Rewrites.** AI_Mgr's `m_fLastUpdate` (+`GetTime()`, benign) and CCorpse's constant flags.

## Tests

`save_ai_tests` (labels `headless;objects;save`; `private` adds the retail data). The private
cases mount the retail data with a synthetic arena DAT (a floor triangle, a start point, three
path points and two exit hints) and go through `GameSession::save_game`/`load_game` with the
local player. Each round trip saves, records the saved world's next images, loads the save,
checks the restored members, requires the re-save to be byte-identical apart from the listed
rewrites, and then requires every later save to match the saved world's (OBJS, LINK, ATCH and
the clock with its generator).

| Case | Checks |
| --- | --- |
| `layout` | A synthetic AI_Mgr (labelled; diagnostic actors run no production initialization): every AI_Mgr field and tail entry by name, LoadSave indices for a listed target and NULL for always-load objects (a PathPoint, a SmellHint), the loop script at its FOLLOWPATH command with an unknown command kept as DONE, the reversed PathMgr order, a restore into a fresh session, the target link back from LINK, the attacker memory, the `m_fLastUpdate` rewrite exactly and an unthrottled ledge query 0.1 s later. |
| `refusals` | 17 corrupt fields (states, booleans, the fixed jump/start-fire flags, an enemy attachment, script movement, the path ownership flag, command type, script index, a nonfinite stimulus, a negative distance, a zero scale, the tail outcome, a smell cursor that is no SmellHint, a self target) each refuse the restore by name and leave no AI. |
| `classes` | All 17 classes idle out of sight: same state, weapon, table and movement cache, 90 identical steps. |
| `states` | A Cultist attacking the player (keeps the target and fires again, 90 steps); a Cultist mid FOLLOWPATH (90); a recoiling Cultist with its attacker memory (150); a scared Civilian hiding (90); a lit Fanatic sprinting, its smoke source recreated and republished (60); a burning Civilian corpse (fire, smoke and loop lost, block restored); a Civilian awaiting removal after its corpse transfer (30, one corpse). |
| `worlds` | Every mounted world entered with a player for 30 steps; the classes still blocking a whole save; every world with lot-11 NPCs restores all its supported objects through `load_game` and runs 30 steps with no NPC failure; a world with no blocking class saves, loads and saves the same bytes. |

Results on macOS (Apple clang 21), rebased on lots 6, 7 and 13 (save_world_logic.private now
applies lot 11's listed rewrite to the NPCs it restores). Dev and inspector-free Release, headless and
private: 1279 of 1286 tests pass; the seven failures are the known private-data content checks
(doors, pickups, ai_recovery, ai_escape and lightning catalogs, assault.ai, mad_scientist.wounds).
All save tests pass. Clang ASan/UBSan (`detect_leaks=0`, unsupported by Apple clang): no sanitizer
report; all save tests and the object contract suite pass; apart from the known failures, only
`locomotion.cityhub_roam` exceeds its fixed limit under ASan (as before), and thirteen tests that
timed out while six sessions built in parallel pass when rerun.

## Real worlds

58 of 58 worlds enter; 35 hold lot-11 NPCs and all 35 restore their supported objects (the
records of lots 5, 8 and 11) with every NPC running. No lot-11 class blocks a world any more;
only `drop` restores whole. Measured on top of lots 6, 7 and 13: the columns give the number of
blocking classes per pending lot (lot 9 pickups and projectiles, lot 10 effect publishers and
emitters) and the lot-12 classes by name.

| World | Lot-11 NPCs | Restored records / NPCs | Lot 9 | Lot 10 | Lot 12 |
| --- | --- | --- | --- | --- | --- |
| 01_subwaya | 15 | 158 / 15 | — | 4 | Gideon |
| 02_cityhub | 26 | 305 / 26 | 9 | 6 | Gideon |
| 02_cityhubb | 20 | 287 / 20 | 7 | 6 | BoneLeech |
| 02_cityhubc | 21 | 262 / 21 | 12 | 6 | BoneLeech |
| 03_museum | 38 | 423 / 38 | 4 | 4 | Gideon |
| 04_steamtunnels | 52 | 283 / 52 | 6 | 4 | — |
| 05_cdm | 56 | 409 / 56 | 9 | 4 | — |
| 05_cdmb | 5 | 58 / 5 | 2 | 1 | — |
| 06_airship1 | 28 | 135 / 28 | — | 1 | — |
| 06_airship2 | 34 | 218 / 34 | 7 | 2 | — |
| 07_cathedral | 15 | 155 / 15 | 10 | 4 | BoneLeech |
| 08_subwayb | 21 | 153 / 21 | — | 3 | — |
| 09_abandoned_buildings | 51 | 478 / 51 | 14 | 7 | Gideon, Thief, ZealotAI |
| 10_sewagetreatmentplant | 24 | 62 / 24 | 6 | 1 | Thief |
| 11_slaughter_house | 52 | 460 / 52 | 12 | 3 | BoneLeech, Thief, ZealotAI |
| 12_train_station | 42 | 365 / 42 | 15 | 5 | BoneLeech, Thief, ZealotAI |
| 12_train_stationb | 32 | 345 / 32 | 4 | 2 | BoneLeech, ZealotAI |
| 13_downtown | 33 | 225 / 33 | 15 | 4 | BoneLeech, TheHandAI, ZealotAI |
| 14_subwayc | 27 | 302 / 27 | 8 | 3 | Thief |
| 15_crypt | 14 | 116 / 14 | 15 | 1 | TheHandAI, Thief |
| 16_bridge | 29 | 224 / 29 | 14 | 3 | BoneLeech, Thief |
| 17_entrance | 25 | 321 / 25 | 6 | 2 | — |
| 18_temple | 21 | 127 / 21 | 12 | — | DrudgePriest, Thief |
| 19_lobby | 54 | 376 / 54 | 6 | 1 | DrudgePriest |
| 19_lobbyb | 19 | 94 / 19 | 7 | — | — |
| 19_lobbyc | 9 | 78 / 9 | 8 | 1 | ZealotAI |
| 20_powerstation | 23 | 114 / 23 | 7 | 2 | TheHandAI |
| 21_laboratory | 41 | 255 / 41 | 8 | 3 | BoneLeech, TheHandAI |
| 22_office | 35 | 476 / 35 | 8 | 1 | BoneLeech |
| 23_rooftop | 2 | 72 / 2 | 12 | 1 | EnhancedGideon |
| 24_ancientcity | 57 | 173 / 57 | 14 | 1 | DrudgePriest, EnhancedGideon, UndeadGideon |
| 24b_arena | 5 | 64 / 5 | 14 | 3 | — |
| 25_ancientone | 4 | 79 / 4 | 7 | 2 | AncientOne, AncientOneTentacle |
| credits | 1 | 12 / 1 | — | 2 | — |
| drop | 0 | — | none | none | none |
| end | 4 | 13 / 4 | — | 1 | — |
| multi/bb_area15 | 0 | — | 16 | — | — |
| multi/bb_bloodarena | 0 | — | 20 | — | — |
| multi/bb_bloodkeep | 0 | — | 16 | 2 | — |
| multi/bb_bodies | 0 | — | 15 | — | — |
| multi/bb_bridge | 0 | — | 13 | 3 | — |
| multi/bb_brownstar | 0 | — | 15 | 2 | — |
| multi/bb_cabana | 0 | — | 17 | — | — |
| multi/bb_chosenarena | 0 | — | 15 | — | — |
| multi/bb_condemned | 0 | — | 18 | 3 | — |
| multi/bb_crypt | 0 | — | 12 | 2 | — |
| multi/bb_deathcanyon | 0 | — | 11 | — | — |
| multi/bb_drain | 0 | — | 14 | — | — |
| multi/bb_dustbowl | 0 | — | 10 | 1 | — |
| multi/bb_fields | 0 | — | 10 | — | — |
| multi/bb_jgrave | 0 | — | 19 | 1 | — |
| multi/bb_midgard | 0 | — | 13 | — | — |
| multi/bb_power | 0 | — | 14 | 1 | — |
| multi/bb_rooftop | 0 | — | 14 | 1 | — |
| multi/ctf_cabana | 0 | — | 17 | — | — |
| multi/ctf_deadcities | 0 | — | 18 | — | — |
| multi/ctf_dmx | 0 | — | 15 | — | — |
| multi/ctf_jam | 0 | — | 21 | — | — |

## Remaining

Lot 12 (bosses and special enemies, including BoneLeech's attachment and the Thief/TheHand
re-pointing of `m_hEnemyAttach`), then lots 9 and 10 and the certification of lot 14.
