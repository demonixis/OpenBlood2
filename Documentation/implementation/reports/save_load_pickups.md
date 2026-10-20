# Pickups and projectiles (save/load lot 9)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 9 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[implemented pickups and projectiles](../../contracts/save_load_contracts.md#implemented-pickups-and-projectiles-lot-9);
the [source ledger](../sources/save_load_pickups_sources.json) lists the references consulted.
The original Save/Load bodies of PickupObject, KeyPickup, CProjectile and SeeingEye now run, and
their 85 registered classes (50 pickup classes, KeyPickup, 33 projectile classes and SeeingEye)
are Original with payload version 1. A real game is still refused by name in every campaign
world (see [real worlds](#real-worlds)). M2 closure and M2-GPU-001 are unchanged.

## What the original does

The [world-object audit](../save_load_audit/world_objects.md) (§25, §26, §27) and the
[character audit](../save_load_audit/characters.md) (§4.3, §20) give the blocks.

- **PickupObject** writes 15 fields (first update, type, value, respawn time, rotate flag, yaw,
  relative last time, a copy of the create flags, bounce acceleration, bouncing, rest position
  and four HSTRINGs). Every subclass relies on it; KeyPickup writes its own five fields first.
  `PostPropRead` runs on every precreate, a restore included: it ORs `TOUCH_NOTIFY | VISIBLE |
  GRAVITY` into the saved flags and sets the model from the class's file. Initial update is
  skipped, except AmmoPickup's list insertion and the multiplayer branches, which run always.
- **CProjectile** writes 26 fields and never its shooter ("Can't tell"). No GameProjectiles
  subclass has a Save/Load; many shadow `m_fStartTime` with a constructor value, so their scale
  delay, burst timer or attract clock restarts or expires after a load, and their bounce
  counters, tracking state, hit objects and sound handles are lost. The trail is an
  engine-saved special-effect message.
- **SeeingEye** derives from BaseClass in the original and writes only its owner, stuck object
  (both LoadSave references) and dropped flag; its Load re-links and re-attaches. CInvTheEye
  saves `m_hSeeingObject` and calls `SeeingEye::Init(owner, NULL)` on load, so the eye's client
  is lost.
- **Remote bombs.** The player's list of charges is unsaved and the charge loses its shooter: a
  restored charge can never be detonated.

## Delivered behavior

- **Bodies.** `PickupSave.cpp` (PickupObject, KeyPickup), `ProjectileSave.cpp` (CProjectile and
  the subclass mappings), `SeeingEye.cpp` (its block). Dispatch is in the original place;
  projectiles write their block before the CDestructable aggregate.
- **Restore branches.** The `PRECREATE_SAVEGAME` refusals of PickupObject, ItemPickup, KeyPickup
  and CProjectile are gone. Pickup precreate keeps `PostPropRead` and reads no property; the
  projectile and eye restore branches skip `PostPropRead` and initial update.
- **Checks before change.** Each Load decodes the whole block, checks it (finite and bounded
  values, booleans 0 or 1, known damage types, the class's pickup type and object name, the
  values the port does not model) and only then assigns. Refusals go through `RejectPayload`.
- **Port-only state.** `configured` is restored true; the remaining port flags (`exploded`,
  `contacted`) start from the constructor except the Tesla ball's, derived from its armed flag.
- **Shadowed start time.** The subclasses' constructor-time `m_fStartTime` is the new
  `CProjectile::scale_start`, set in the constructor; the 0.01-second scale delays and the
  alternate flare's burst use it, so they restart after a load as in the original.
- **Trails.** The grenade, primary orb, life-leech primary, bug spray, vomit and skull trails are
  published again in Load.
- **Departure D15.** The CProjectile block ends with the shooter as a LoadSave reference. A
  remote charge not yet signalled is listed again on its player (`RestoreRemoteBomb`, list only);
  the player-charge link comes back with the other saved interlinks. A thrown eye's removal turns
  the view off on the owner's current client.
- **Inventory.** CInventoryMgr's Load binds a thrown eye (it must be a SeeingEye) and sets its
  owner, replacing lot 5's refusal.
- **Declarations.** `registration.cpp` declares the 85 classes Original, version 1.

## Findings

- **Interlink order.** A Load that creates an interlink places it before the LINK section's, so
  a re-save would reorder LINK. Loads therefore create none: projectile-shooter, player-charge
  and eye-owner/stuck links all come back through LINK, and a re-save keeps their order.
- **Attract clocks.** An armed Tesla ball or Singularity restored at a world clock past its
  attract time ends on its first update (constructor 0 shadow clock), as in the original; below
  that clock it lives on. The family test fires them after 6 seconds so the loss shows.
- **Flare fragments** read an uninitialized colour in the original; the port keeps the engine
  colour its last update set, so a fragment saved before its first update restores white.
- **Pickups never respawn in single player**: the original removes a collected pickup, so the
  only saved pickup states are present, bouncing (string spawn) and without model dimensions.
- **Worlds that became whole.** With pickups covered (and lot 7's triggers), `drop` and nine
  multiplayer maps save whole; the sibling private audits assumed none of their sky worlds
  would. The lot-7, lot-8 and lot-13 audits now check the sky binding through the whole
  restore, treat lot 13's CTF refusal as a blocker, accept lot 9's pickup flag rewrite and lot
  5's player rewrites after a second load.

## Tests

`save_pickups_tests` (labels `headless;objects;save`; `private` adds the commercial data). Every
case saves through `GameSession::save_game`, loads through `load_game` (restore-mode world,
candidate, restored client) and requires the re-save of the lot-9 records to be byte-identical
apart from the listed rewrite.

| Case | Checks |
| --- | --- |
| `pickups` | An authored ammo box on the ground (back on the AI ammo list), a string-spawned bouncing medkit (acceleration, rest position), a weapon drop without ammunition and a pickup trigger, a rotating powerup and an authored key (names, icons, uses, model strings, flags); re-save identity; the restored player then collects the ammunition |
| `rewrites` | A pickup whose flags lost `TOUCH_NOTIFY` and `GRAVITY` regains them through `PostPropRead`; that is the only difference on re-save |
| `bombs` | A proximity bomb stuck and armed, a time bomb counting down, a remote charge stuck and one in flight (both listed), a grenade in flight: armed flags, spin, shooter, constructor-reset fields, the player's list rebuilt in the saved order, both interlinks; re-save identity including LINK; the time bomb explodes 5 s after it stuck on the restored clock, the player detonates both restored charges, the proximity bomb detects the player |
| `eye` | A thrown eye dropped on the floor while the player looks through it: the inventory item binds the restored eye (inactive), owner and link; re-save identity including LINK; removing the eye after looking through it again turns the view off and consumes the item |
| `family` | 29 projectile classes in flight (20 after two updates, nine one-update projectiles right before the save) and a coloured flare fragment: shooter and configuration restored, re-save identity, and after 20 frames the same projectiles fly and fail as in the saved world, except the Tesla ball and Singularity, which end at once (the documented loss) |
| `refusals` | A corrupt boolean, a foreign pickup type, a nonzero respawn time, an unknown damage type, a nonzero `m_bRemoveFromWorld` and a truncated projectile block each refuse the load by class name with the source session kept |
| `private` | All 58 worlds: entry with a player, 30 frames, the blocking classes, then every Original object (player, lots 8 and 9) through `load_game`, saved again byte for byte apart from the player's listed rewrites, then 30 frames with no unavailable service; whole saves where nothing blocks |

Updated tests: `save_session.declarations` expects the 85 lot-9 classes; `save_objects.order`
names CBaseCharacter instead of HealthPU as its still-Unsupported class;
`save_world_logic.spawners` now saves the spawned pickups; the `save_movers`, `save_world_logic`
and `save_native` private audits handle the worlds that now save whole (see findings).

## Real worlds

On top of lots 6, 7 and 13, all 58 worlds enter with a player and restore every Original object
through the real restore-mode world and candidate transaction: 9,402 non-player records, each
saved again identically apart from the listed rewrites of lots 5, 7, 8 and 9 (and a one-ulp
relative clock, lot 8). Ten restore whole: `drop` and nine multiplayer maps
(`bb_area15`, `bb_bloodarena`, `bb_bodies`, `bb_cabana`, `bb_chosenarena`, `bb_deathcanyon`,
`bb_drain`, `bb_fields`, `bb_midgard`). Four CTF maps hold no unsupported class but are refused
by lot 13's CTF rule. No campaign world is whole yet. After entry no projectile is alive in any
world; the synthetic cases cover them.

Blocking classes after entry with a player (number of worlds): SoundFX 25, Fanatic 23,
RandomSoundFX 20, ShikariAI 19, CClientParticleStreamSFX 18, DetailSprite 17, SoulDrudge 17,
CultistAI 15, Prophet 14, CivilianAI 13, DrudgeLord 13, BoneLeech 10, Thief 8,
CClientExplosionSFX 6, ZealotAI 6, FlickerLight 5, GlowingLight 5; fewer than five worlds each:
AncientOne, AncientOneTentacle, Behemoth, CClientLightningSFX, CClientWarpGateSFX, CalebAI,
ClientLightFX, DeathShroud, DrudgePriest, EnhancedGideon, Flicker3WaveLightFX,
Flicker4WaveLightFX, GabriellaAI, GabriellaREV, Gideon, IshmaelAI, IshmaelREV, MadScientistAI,
OpheliaAI, OpheliaREV, StrobeWaveLightFX, TheHandAI, UndeadGideon. These belong to lots 10, 11
and 12.

| World | Pickups | Blocking classes |
| --- | --- | --- |
| 01_subwaya | 0 | CClientExplosionSFX, CivilianAI, CultistAI, DetailSprite, Gideon, GlowingLight, RandomSoundFX |
| 02_cityhub | 18 | CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, Flicker4WaveLightFX, FlickerLight, Gideon, RandomSoundFX, SoundFX |
| 02_cityhubb | 12 | BoneLeech, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, Flicker4WaveLightFX, FlickerLight, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX |
| 02_cityhubc | 20 | BoneLeech, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, Flicker4WaveLightFX, FlickerLight, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX |
| 03_museum | 6 | CClientParticleStreamSFX, CultistAI, Fanatic, Gideon, GlowingLight, MadScientistAI, Prophet, RandomSoundFX, SoundFX |
| 04_steamtunnels | 16 | CClientParticleStreamSFX, CivilianAI, DetailSprite, DrudgeLord, Fanatic, FlickerLight, ShikariAI, SoulDrudge, SoundFX |
| 05_cdm | 12 | CClientParticleStreamSFX, CivilianAI, DetailSprite, Fanatic, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX |
| 05_cdmb | 2 | Fanatic, Prophet, SoundFX |
| 06_airship1 | 0 | CultistAI, DetailSprite, Fanatic |
| 06_airship2 | 7 | CultistAI, DetailSprite, Fanatic, Prophet, SoundFX |
| 07_cathedral | 15 | BoneLeech, CClientExplosionSFX, ClientLightFX, DetailSprite, DrudgeLord, Fanatic, RandomSoundFX, ShikariAI, SoulDrudge |
| 08_subwayb | 0 | CClientExplosionSFX, CivilianAI, Fanatic, GlowingLight, RandomSoundFX |
| 09_abandoned_buildings | 27 | CClientParticleStreamSFX, CClientWarpGateSFX, CivilianAI, DrudgeLord, Fanatic, Flicker3WaveLightFX, Gideon, GlowingLight, MadScientistAI, Prophet, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX, StrobeWaveLightFX, Thief, ZealotAI |
| 10_sewagetreatmentplant | 6 | CClientParticleStreamSFX, DrudgeLord, Fanatic, ShikariAI, SoulDrudge, Thief |
| 11_slaughter_house | 21 | BoneLeech, CClientParticleStreamSFX, CivilianAI, CultistAI, DrudgeLord, Fanatic, Prophet, RandomSoundFX, SoulDrudge, SoundFX, Thief, ZealotAI |
| 12_train_station | 18 | BoneLeech, CClientExplosionSFX, CClientParticleStreamSFX, CivilianAI, ClientLightFX, CultistAI, DrudgeLord, Fanatic, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX, Thief, ZealotAI |
| 12_train_stationb | 9 | BoneLeech, CClientParticleStreamSFX, Fanatic, Prophet, SoundFX, ZealotAI |
| 13_downtown | 31 | BoneLeech, CClientParticleStreamSFX, CivilianAI, ClientLightFX, DetailSprite, DrudgeLord, RandomSoundFX, ShikariAI, SoulDrudge, TheHandAI, ZealotAI |
| 14_subwayc | 10 | CClientParticleStreamSFX, CivilianAI, DrudgeLord, Fanatic, GlowingLight, Prophet, RandomSoundFX, ShikariAI, SoulDrudge, Thief |
| 15_crypt | 36 | Behemoth, DrudgeLord, RandomSoundFX, ShikariAI, SoulDrudge, TheHandAI, Thief |
| 16_bridge | 27 | BoneLeech, CClientParticleStreamSFX, CultistAI, DetailSprite, Fanatic, Prophet, ShikariAI, SoulDrudge, SoundFX, Thief |
| 17_entrance | 10 | DetailSprite, Fanatic, Prophet, SoundFX |
| 18_temple | 22 | CultistAI, DrudgeLord, DrudgePriest, ShikariAI, SoulDrudge, Thief |
| 19_lobby | 12 | Behemoth, CultistAI, DeathShroud, DrudgePriest, Fanatic, Prophet, SoundFX |
| 19_lobbyb | 7 | Fanatic, Prophet, ShikariAI |
| 19_lobbyc | 13 | Behemoth, DrudgeLord, Fanatic, ShikariAI, SoundFX, ZealotAI |
| 20_powerstation | 10 | CClientLightningSFX, CultistAI, Fanatic, Prophet, ShikariAI, SoundFX, TheHandAI |
| 21_laboratory | 20 | BoneLeech, CClientLightningSFX, CivilianAI, CultistAI, Fanatic, MadScientistAI, Prophet, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX, TheHandAI |
| 22_office | 11 | BoneLeech, CultistAI, DeathShroud, DetailSprite, Fanatic, Prophet, ShikariAI |
| 23_rooftop | 56 | CClientParticleStreamSFX, CalebAI, EnhancedGideon, OpheliaAI |
| 24_ancientcity | 21 | Behemoth, DeathShroud, DetailSprite, DrudgeLord, DrudgePriest, EnhancedGideon, Fanatic, ShikariAI, SoulDrudge, UndeadGideon |
| 24b_arena | 36 | CClientExplosionSFX, CClientLightningSFX, CClientWarpGateSFX, DrudgeLord, GabriellaREV, IshmaelREV, OpheliaREV, SoulDrudge |
| 25_ancientone | 9 | AncientOne, AncientOneTentacle, CClientExplosionSFX, CClientLightningSFX, CalebAI, GabriellaAI, IshmaelAI, OpheliaAI |
| credits | 0 | CalebAI, DetailSprite, FlickerLight |
| drop | 0 | none (whole) |
| end | 0 | CalebAI, GabriellaAI, IshmaelAI, OpheliaAI, RandomSoundFX |
| multi/bb_area15 | 41 | none (whole) |
| multi/bb_bloodarena | 67 | none (whole) |
| multi/bb_bloodkeep | 38 | DetailSprite, SoundFX |
| multi/bb_bodies | 36 | none (whole) |
| multi/bb_bridge | 82 | CClientParticleStreamSFX, DetailSprite, SoundFX |
| multi/bb_brownstar | 78 | RandomSoundFX, SoundFX |
| multi/bb_cabana | 42 | none (whole) |
| multi/bb_chosenarena | 79 | none (whole) |
| multi/bb_condemned | 97 | CClientParticleStreamSFX, RandomSoundFX, SoundFX |
| multi/bb_crypt | 35 | RandomSoundFX, SoundFX |
| multi/bb_deathcanyon | 26 | none (whole) |
| multi/bb_drain | 32 | none (whole) |
| multi/bb_dustbowl | 66 | SoundFX |
| multi/bb_fields | 39 | none (whole) |
| multi/bb_jgrave | 62 | RandomSoundFX |
| multi/bb_midgard | 42 | none (whole) |
| multi/bb_power | 44 | SoundFX |
| multi/bb_rooftop | 54 | CClientParticleStreamSFX |
| multi/ctf_cabana | 84 | CTF refusal (lot 13) |
| multi/ctf_deadcities | 100 | CTF refusal (lot 13) |
| multi/ctf_dmx | 63 | CTF refusal (lot 13) |
| multi/ctf_jam | 193 | CTF refusal (lot 13) |

## Execution evidence

macOS (Apple clang 21). Measured on `fed1c924` (lots 6, 7 and 13), then rebased on lot 11
(`67c97f58`):

- **Dev** (`dev` preset, private data configured): the full headless label runs 1,301 tests
  (seven new `save_pickups.*`, including the private case); 1,294 pass. The seven failures are
  the pre-existing private-data content checks (doors.catalog, pickups.catalog,
  ai_recovery.catalog, ai_escape.catalog, assault.ai, mad_scientist.wounds,
  lightning.catalog), the same as before this lot. All save suites pass, including the five
  private audits (`save_session`, `save_movers`, `save_world_logic`, `save_native`,
  `save_pickups`; 27 s for the last).
- **Release** (inspector-free `release` preset): builds; its headless run gives the same 1,294
  of 1,301 with the same seven failures.
- **Clang ASan/UBSan** (`testing.md` recipe, `ASAN_OPTIONS=detect_leaks=0`: Apple clang has no
  LeakSanitizer). Before the rebase, the full headless label (1,276 tests) produced no
  sanitizer report; besides the seven failures above, 23 tests exceeded their fixed wall-clock
  limits at a load average of 80 to 110 (other sessions' builds). Run directly without the
  limit, the first seven of them pass (`objects.message_churn` 6 s, `locomotion.cityhub_roam`
  465 s); `doors.catalog` keeps its known content failure. Every save suite passed, including
  the three private audits of that base (`save_pickups.private` 1,719 s). After the rebase,
  the save, object-contract, pickup and projectile suites (184 tests) pass under the
  sanitizers apart from pickups.catalog and assault.ai, with no sanitizer report.
- **On lots 10 and 11.** The whole-world audits share every lot's listed rewrites in
  `tests/save_rewrites.h` (lot 10's header; lot 9 adds the pickup flag rule), and the
  `save_movers`, `save_world_logic` and `save_ai` audits treat lot 13's CTF refusal as a blocker,
  since lot 9 leaves the CTF maps without an unsupported class. With these, 86 of the 87 save tests passed, every private audit except
  `save_ai.worlds` included; that one then failed only on its world count, which the last
  change fixes.
- **Private checks must be re-run.** The private-data results above (all 58 worlds, the five
  private save audits, the 1,294 of 1,301 headless runs with private data) were measured before
  the retail data folder was erased (incident below). They must be re-run once the data is
  restored, `save_ai.worlds` first. After the deletion only the headless suites without private
  data were run (see the final runs below).
- **Incident.** While checking `save_ai.worlds` by hand, the private-data folder was passed as
  `save_ai_tests`' first path argument, its scratch root, which the test deletes: the retail
  data folder was erased. `save_pickups_tests` uses the scratch-root guard main added after
  the incident (`tests/scratch_dir.h`).
- **Final runs (lot 12 base, `dedb60d5`, no private data).** The headless label without private data (965 tests) passes in full in the Dev,
  inspector-free Release and ASan/UBSan builds, with no sanitizer report.

No GPU, interactive or menu validation is specific to this lot.

## For later lots

- Lot 6's slots and menus can now save ten real worlds whole.
- Lot 11: NPC shooters become saveable with their class; a projectile already keeps its shooter
  reference, and the Naga debris, Zealot and Behemoth shockwaves read it on their single update.
- Every lot: a Load should not create an interlink the LINK section restores, or a re-save
  reorders LINK.

## Limits

- Campaign worlds are still refused by name (lots 7, 10, 11, 12).
- The documented subclass losses are kept; the Tesla ball and Singularity end after a load.
- The trail of a life-leech primary is republished with a new random scale.
- The pickup display name is NULL: the port has no server `FormatString`.
