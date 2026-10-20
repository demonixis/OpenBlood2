# World logic (save/load lot 7)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 7 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules
are recorded in the contract's
[implemented world logic](../../contracts/save_load_contracts.md#implemented-world-logic-lot-7);
the [source ledger](../sources/save_load_world_logic_sources.json) lists the references
consulted. The original Save/Load bodies of the triggers, Spawner, KeyFramer with its KeyData
blocks and CameraObj now run, and their 10 classes are Original with payload version 1.
Self-triggered doors, which lot 8 left refused, save with their Trigger. A save of a real game
is still refused by name because every world except `drop` also holds classes of other lots.
M2 closure and M2-GPU-001 are unchanged.

## What the original does

The [world-object audit](../save_load_audit/world_objects.md) (§1–§3, §7, §20, §27) gives the
field order of each block. A subclass writes its block before calling Trigger's. On
`PRECREATE_SAVEGAME` the classes read no property; CameraObj's `PostPropRead` still forces its
flags and a 0.001 s first update. On `INITIALUPDATE_SAVEGAME` they skip their initialization,
except that CameraObj joins the camera list and Spawner reschedules from its constructor
defaults. State machines resume from the engine's saved next-update delay. Voices are not
saved: CinematicTrigger replays the cue that was playing and resends its message, KeyFramer's
key sound is lost. KeyFramer does not save its object list: the next update rebuilds it by
name. CameraObj's Load ends with `SetActive`, which recomputes the active camera and restarts
a timed camera's duration; the engine replays its saved camera effect message.

## Delivered behavior

- **Bodies.** `Trigger`, `ToggleTrigger`, `ExitTrigger`, `ObjectivesTrigger`,
  `ConversationTrigger`, `CinematicTrigger`, `Spawner`, `KeyFramer` (with the KeyData blocks)
  and `CameraObj` handle `MID_SAVEOBJECT`/`MID_LOADOBJECT` in the original place, on the port's
  typed members. BigTrigger has no block.
- **Restore branches.** The `PRECREATE_SAVEGAME` refusals are gone. ToggleTrigger no longer
  reads its properties for a savegame. Initial update is skipped for a savegame, as in the
  original.
- **Checks before change.** Each Load decodes the whole block, checks it against the class's
  property bounds (finite and bounded times, delays, radii and velocities, booleans 0 or 1,
  the ENDWORLD exit type, the objectives resource range, at most eight conversation messages
  with the cursor inside them, at most ten cinematic cues, camera type 0–2), and only then
  assigns. KeyFramer's key types must match their strings, its float-encoded indices must be
  integral and inside the key list, and its state must be one the port's sequencing reaches.
  Refusals go through `RejectPayload`.
- **KeyFramer.** A restored controller keeps its keys and never collects the reloaded
  always-load Keys again. Its target list is rebuilt by name at the next update; targets take
  the saved offsets in list order, and until then a save writes the loaded offsets unchanged.
  The private `position1`/`position2` members now hold signed key indices (−1 for NULL), and a
  new `cursor` flag records the original `m_pCurKey` being set.
- **CameraObj.** Load publishes the camera effect again, then calls `SetActive`. The
  publication helper is shared with InitialUpdate.
- **Door.** Load checks that a saved trigger reference is a Trigger and gives it back its
  owning door.
- **Declarations.** `registration.cpp` declares the 10 classes Original, version 1.

## Findings

- **Tunnel lights.** SubwayA, SubwayB and SubwayC move twelve GlowingLights (`sublight`) on a
  two-key looping path. The original writes one offset per key, so after a restore ten lights
  would read uninitialized offsets and jump. This is a player-visible loss in the sense of
  decision 4, so a port tail carries the offsets past the key count; it is recorded as a
  departure. No other world has more targets than keys.
- **Lookup order.** `FindNamedObjects` lists objects newest first. A restored world creates its
  saved records in creation order, so the rebuilt target list has the same order as the
  first-update list, and the saved offsets apply to the same objects.
- **Reset times.** ReadProp checks only that `ResetTime` is finite, and a 16_bridge trigger
  exceeds the 10⁶ bound a first Load draft applied; Load now checks finiteness only, like
  ReadProp.
- **Cinematic restart.** A cue playing at the save restarts at the restored update, resends
  its message and plays from the start, so the restored and saved worlds diverge from that
  point. This is the original behavior and has its own test.
- **Timed cameras.** Load restarts a timed active camera's full ActiveTime; this is listed as a
  re-save rewrite, with the forced flags and schedule.
- **Spawner.** The original Spawner's unconditional initial update stopped a running spawner
  after every restore. The port keeps the restored schedule; this is the decision-4 departure
  the plan names.
- **Objective sound.** `m_hstrSound` is not saved; a restored ObjectivesTrigger has no sound,
  as in the original. The port does not play it yet either.

## Tests

`save_world_logic_tests` (labels `headless;objects;save`; `private` adds the commercial data).
Every round trip saves the world, restores it into a fresh session, checks the restored
members, requires the re-save to be byte-identical apart from the listed rewrites, and then
requires every later save of the restored world to match the saved world's.

| Case | Checks |
| --- | --- |
| `triggers` | Every trigger class with nondefault properties: armed, waiting on two delayed sends (one sent, one pending), locked, a toggle on and resetting, a conversation mid-sequence; dims, sound, radius, slots, delays, activation filters, key name, counts, toggle off messages, exit world/start/type, objectives, conversation waves and cursor, saved touch flags, no objective sound; 60 identical steps each, with the pending send delivered |
| `doors` | A locked self-triggered door: the restored Trigger's owning door, slot, lock, key name and dims, the door-to-trigger link; a relay opens the door after the load; removing the door removes its trigger; 60 identical steps |
| `spawners` | A running spawner keeps spawning (three pickups in 1.2 s, count from 19 to 16) and a stopped one stays unscheduled; properties; 60 identical steps |
| `keyframers` | A path mid-segment, a looping path, one saved before its first update (collects the reloaded keys after the load) and one with five targets on three keys (every target keeps its offset); keys, sound, message and print values, key names, no restarted voice, rebuilt targets, consumed keys left inert; 80 identical steps |
| `cameras` | An active listener camera tracking a saved target and an idle one: active camera, client camera view and settings, link and interlink, list count; 60 identical steps. A timed camera with 1 s left restarts its full 2 s and then deactivates |
| `cinematics` | A trigger between cues resumes its delay (40 identical steps); one saved mid-cue restarts the cue and resends its message |
| `refusals` | Corrupt booleans, conversation count and camera type, a truncated and a lengthened block each fail the restore by class name with every record rolled back and no string, message or camera effect left; more than 255 keys refuses the save |
| `private` | All 58 worlds: lot-7 objects present after entry and the classes still blocking a whole save; every world restores an image of all its Original objects (lots 5, 7 and 8) through `load_game`, saves the same bytes again and runs 30 steps with no unavailable service; `drop` restores whole |

Updated tests: `save_session.declarations` and `marking` expect the 10 new Original classes;
`save_objects.order` uses an Unsupported pickup instead of Trigger; `save_movers.refusals` no
longer expects a self-triggered door to be refused; `exits.defaults`, `cameras.defaults`,
`keyframes.defaults`, `cinematics.failures` and `spawner.properties` expect a restore creation
to be accepted and left to Load.

## Real worlds

All 58 worlds enter; all 58 restore their Original objects through the real restore-mode world
and candidate transaction (57 from an image of those objects, `drop` whole). The lot-7 objects
present after entry: 943 Trigger, 231 Spawner, 95 KeyFramer, 90 BigTrigger, 81
CinematicTrigger, 62 CameraObj, 54 ObjectivesTrigger, 40 ExitTrigger, 8 ConversationTrigger and
4 ToggleTrigger. No world besides `drop` restores whole yet. The classes still blocking each
world after entry with a player:

| World | Lot-7 objects | Blocking classes |
| --- | --- | --- |
| 01_subwaya | CameraObj x2, CinematicTrigger x4, ExitTrigger x1, KeyFramer x4, ObjectivesTrigger x1, Trigger x15 | 7: CClientExplosionSFX, CivilianAI, CultistAI, DetailSprite, Gideon, GlowingLight, RandomSoundFX |
| 02_cityhub | BigTrigger x3, CameraObj x2, CinematicTrigger x8, ConversationTrigger x1, ExitTrigger x3, KeyFramer x2, ObjectivesTrigger x8, Trigger x31 | 18: AssaultRiflePU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, Gideon, MedKitPU, ProximitiesPU, RandomSoundFX, RemotesPU, SoundFX, SubMachineGunPU, WardPU |
| 02_cityhubb | BigTrigger x3, CameraObj x2, CinematicTrigger x1, ConversationTrigger x1, ExitTrigger x3, KeyFramer x2, ObjectivesTrigger x1, Spawner x2, Trigger x29 | 18: BoneLeech, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, MegaHealthPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShikariAI, SoulDrudge, SoundFX, SubMachineGunPU |
| 02_cityhubc | BigTrigger x3, CameraObj x1, ConversationTrigger x1, ExitTrigger x1, KeyFramer x1, ObjectivesTrigger x1, Spawner x15, Trigger x19 | 23: BatteryAmmoPU, BoneLeech, BugSprayPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, HealthPU, MegaHealthPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, SoundFX, SubMachineGunPU |
| 03_museum | BigTrigger x7, CameraObj x4, CinematicTrigger x11, ExitTrigger x1, KeyFramer x9, ObjectivesTrigger x2, Spawner x4, Trigger x37 | 13: CClientParticleStreamSFX, CultistAI, Fanatic, Gideon, GlowingLight, InvulnerabilityPU, MadScientistAI, NecroWardPU, Prophet, ProximitiesPU, RandomSoundFX, ShotgunPU, SoundFX |
| 04_steamtunnels | BigTrigger x25, ExitTrigger x1, ObjectivesTrigger x1, Trigger x19 | 15: CClientParticleStreamSFX, CivilianAI, DetailSprite, DrudgeLord, Fanatic, FlareAmmoPU, FlickerLight, HealthPU, MedKitPU, NecroWardPU, NightGogglesPU, RemotesPU, ShikariAI, SoulDrudge, SoundFX |
| 05_cdm | CameraObj x1, CinematicTrigger x7, ExitTrigger x1, KeyFramer x2, ObjectivesTrigger x6, Spawner x20, Trigger x87 | 17: BerettaPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, DetailSprite, Fanatic, HealthPU, KeyPickup, MedKitPU, MegaHealthPU, NecroWardPU, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX, StealthPU, WardPU |
| 05_cdmb | CameraObj x2, CinematicTrigger x1, ExitTrigger x1, KeyFramer x6, ObjectivesTrigger x1, Spawner x2, Trigger x12 | 5: BMGAmmoPU, Fanatic, Prophet, SniperRiflePU, SoundFX |
| 06_airship1 | BigTrigger x23, ExitTrigger x2, ObjectivesTrigger x1, Spawner x1, Trigger x3 | 3: CultistAI, DetailSprite, Fanatic |
| 06_airship2 | BigTrigger x10, ExitTrigger x1, ObjectivesTrigger x2, Trigger x27 | 12: AngerPU, AssaultRiflePU, BugSprayPU, CultistAI, DetailSprite, Fanatic, FlareGunPU, KeyPickup, MedKitPU, Prophet, SniperRiflePU, SoundFX |
| 07_cathedral | CameraObj x2, CinematicTrigger x3, ExitTrigger x1, KeyFramer x2, ObjectivesTrigger x1, Spawner x22, Trigger x25 | 19: AngerPU, BatteryAmmoPU, BoneLeech, BulletAmmoPU, CClientExplosionSFX, ClientLightFX, DetailSprite, DrudgeLord, Fanatic, FlareAmmoPU, KeyPickup, MegaHealthPU, NecroWardPU, NightGogglesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, SoulDrudge, VoodooDollPU |
| 08_subwayb | CinematicTrigger x2, ConversationTrigger x1, ExitTrigger x1, KeyFramer x2, ObjectivesTrigger x1, Trigger x12 | 5: CClientExplosionSFX, CivilianAI, Fanatic, GlowingLight, RandomSoundFX |
| 09_abandoned_buildings | CameraObj x11, CinematicTrigger x8, ExitTrigger x1, KeyFramer x14, ObjectivesTrigger x2, Spawner x31, Trigger x51 | 31: BMGAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, CClientWarpGateSFX, CivilianAI, DieBugDieAmmoPU, DrudgeLord, Fanatic, FlareAmmoPU, FlashlightPU, Flicker3WaveLightFX, Gideon, GlowingLight, HowitzerPU, MadScientistAI, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, Prophet, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, ShotgunPU, SoulDrudge, SoundFX, StrobeWaveLightFX, Thief, TimeBombPU, ZealotAI |
| 10_sewagetreatmentplant | ExitTrigger x1, ObjectivesTrigger x1, Trigger x10 | 12: AssaultRiflePU, BMGAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DrudgeLord, Fanatic, FlashlightPU, MedKitPU, ShellAmmoPU, ShikariAI, SoulDrudge, Thief |
| 11_slaughter_house | CinematicTrigger x6, ConversationTrigger x2, ExitTrigger x1, KeyFramer x3, ObjectivesTrigger x3, Spawner x22, Trigger x57 | 24: BMGAmmoPU, BoneLeech, CClientParticleStreamSFX, CivilianAI, CultistAI, DeathRayPU, DrudgeLord, Fanatic, FlareAmmoPU, FlareGunPU, HealthPU, MedKitPU, MegaHealthPU, NecroWardPU, Prophet, ProximitiesPU, RandomSoundFX, RemotesPU, SoulDrudge, SoundFX, TheEyePU, Thief, TimeBombPU, ZealotAI |
| 12_train_station | CinematicTrigger x5, ExitTrigger x1, KeyFramer x1, ObjectivesTrigger x3, Spawner x8, Trigger x68 | 29: AngerPU, BatteryAmmoPU, BinocularsPU, BoneLeech, BulletAmmoPU, CClientExplosionSFX, CClientParticleStreamSFX, CivilianAI, ClientLightFX, CultistAI, DieBugDieAmmoPU, DrudgeLord, Fanatic, FlareAmmoPU, FlashlightPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SniperRiflePU, SoulDrudge, SoundFX, Thief, VoodooDollPU, ZealotAI |
| 12_train_stationb | CinematicTrigger x1, ExitTrigger x1, ObjectivesTrigger x1, Spawner x10, Trigger x29 | 10: BMGAmmoPU, BoneLeech, CClientParticleStreamSFX, Fanatic, Prophet, RemotesPU, ShellAmmoPU, SoundFX, TimeBombPU, ZealotAI |
| 13_downtown | ExitTrigger x1, KeyFramer x12, ObjectivesTrigger x2, Spawner x5, Trigger x25 | 26: BMGAmmoPU, BatteryAmmoPU, BoneLeech, BugSprayPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, ClientLightFX, DetailSprite, DieBugDieAmmoPU, DrudgeLord, FlareAmmoPU, FlashlightPU, HowitzerAmmoPU, NapalmCannonPU, NightGogglesPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, ShotgunPU, SniperRiflePU, SoulDrudge, TheHandAI, TimeBombPU, ZealotAI |
| 14_subwayc | ExitTrigger x1, KeyFramer x1, ObjectivesTrigger x1, Spawner x2, Trigger x20 | 18: AssaultRiflePU, BatteryAmmoPU, CClientParticleStreamSFX, CivilianAI, DieBugDieAmmoPU, DrudgeLord, Fanatic, FlareAmmoPU, FuelAmmoPU, GlowingLight, NecroWardPU, Prophet, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, Thief |
| 15_crypt | CameraObj x1, CinematicTrigger x2, ExitTrigger x1, KeyFramer x1, ObjectivesTrigger x1, Spawner x8, Trigger x29 | 22: BMGAmmoPU, BatteryAmmoPU, Behemoth, BulletAmmoPU, DieBugDieAmmoPU, DrudgeLord, FlareAmmoPU, FlashlightPU, FuelAmmoPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, SoulDrudge, StealthPU, TheHandAI, Thief, VoodooDollPU |
| 16_bridge | BigTrigger x2, CameraObj x1, CinematicTrigger x2, ConversationTrigger x1, ExitTrigger x1, KeyFramer x1, ObjectivesTrigger x2, Spawner x4, ToggleTrigger x1, Trigger x23 | 24: AssaultRiflePU, BatteryAmmoPU, BoneLeech, BulletAmmoPU, CClientParticleStreamSFX, CultistAI, DetailSprite, DieBugDieAmmoPU, Fanatic, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, Prophet, ProximitiesPU, RemotesPU, ShellAmmoPU, ShikariAI, ShotgunPU, SniperRiflePU, SoulDrudge, SoundFX, Thief, WardPU |
| 17_entrance | BigTrigger x2, CameraObj x1, CinematicTrigger x3, ConversationTrigger x1, ExitTrigger x1, ObjectivesTrigger x1, Spawner x21, ToggleTrigger x3, Trigger x46 | 10: BulletAmmoPU, DetailSprite, Fanatic, HealthPU, KeyPickup, MiniGunPU, Prophet, ShellAmmoPU, SoundFX, WardPU |
| 18_temple | CameraObj x2, CinematicTrigger x4, ExitTrigger x1, KeyFramer x5, ObjectivesTrigger x1, Spawner x9, Trigger x16 | 18: BMGAmmoPU, BulletAmmoPU, CultistAI, DieBugDieAmmoPU, DrudgeLord, DrudgePriest, FlareAmmoPU, FlashlightPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, LifeLeechPU, MegaHealthPU, NecroWardPU, ShellAmmoPU, ShikariAI, SoulDrudge, Thief |
| 19_lobby | BigTrigger x2, CinematicTrigger x3, ExitTrigger x1, ObjectivesTrigger x2, Spawner x9, Trigger x39 | 13: Behemoth, CultistAI, DeathShroud, DieBugDieAmmoPU, DrudgePriest, Fanatic, FlareAmmoPU, FuelAmmoPU, KeyPickup, MegaHealthPU, Prophet, SoundFX, StealthPU |
| 19_lobbyb | ExitTrigger x2, ObjectivesTrigger x1, Spawner x3, Trigger x11 | 10: BMGAmmoPU, BatteryAmmoPU, DieBugDieAmmoPU, Fanatic, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, Prophet, ShikariAI |
| 19_lobbyc | ExitTrigger x2, ObjectivesTrigger x1, Spawner x4, Trigger x9 | 14: AngerPU, BatteryAmmoPU, Behemoth, BulletAmmoPU, DrudgeLord, Fanatic, FuelAmmoPU, HealthPU, MegaHealthPU, NecroWardPU, ShikariAI, SoundFX, WardPU, ZealotAI |
| 20_powerstation | ExitTrigger x1, ObjectivesTrigger x1, Spawner x3, Trigger x16 | 14: BatteryAmmoPU, BulletAmmoPU, CClientLightningSFX, CultistAI, Fanatic, HealthPU, HowitzerAmmoPU, MedKitPU, Prophet, ShikariAI, SoundFX, SubMachineGunPU, TheHandAI, WardPU |
| 21_laboratory | CameraObj x1, CinematicTrigger x1, ExitTrigger x1, ObjectivesTrigger x1, Spawner x1, Trigger x47 | 20: BatteryAmmoPU, BoneLeech, CClientLightningSFX, CivilianAI, CultistAI, DieBugDieAmmoPU, Fanatic, FlareAmmoPU, HealthPU, KeyPickup, MadScientistAI, MegaHealthPU, Prophet, RandomSoundFX, ShikariAI, SingularityPU, SoulDrudge, SoundFX, TheHandAI, WardPU |
| 22_office | ExitTrigger x1, ObjectivesTrigger x1, Spawner x9, Trigger x44 | 15: BatteryAmmoPU, BoneLeech, CultistAI, DeathShroud, DetailSprite, Fanatic, FuelAmmoPU, HealthPU, HowitzerAmmoPU, KeyPickup, MedKitPU, MegaHealthPU, NecroWardPU, Prophet, ShikariAI |
| 23_rooftop | CameraObj x1, CinematicTrigger x3, ExitTrigger x1, KeyFramer x1, Spawner x2, Trigger x3 | 16: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, CalebAI, DieBugDieAmmoPU, EnhancedGideon, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, LifeLeechPU, MegaHealthPU, OpheliaAI, ShellAmmoPU |
| 24_ancientcity | BigTrigger x2, CameraObj x8, CinematicTrigger x2, ExitTrigger x1, KeyFramer x8, ObjectivesTrigger x1, Spawner x4, Trigger x14 | 24: BatteryAmmoPU, Behemoth, BulletAmmoPU, DeathShroud, DetailSprite, DrudgeLord, DrudgePriest, EnhancedGideon, Fanatic, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, UndeadGideon, WardPU |
| 24b_arena | CameraObj x10, CinematicTrigger x1, ExitTrigger x1, KeyFramer x10, ObjectivesTrigger x1, Spawner x6, Trigger x7 | 22: AngerPU, BatteryAmmoPU, BulletAmmoPU, CClientExplosionSFX, CClientLightningSFX, CClientWarpGateSFX, DieBugDieAmmoPU, DrudgeLord, FuelAmmoPU, GabriellaREV, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, IshmaelREV, MegaHealthPU, OpheliaREV, ProximitiesPU, RemotesPU, ShellAmmoPU, SoulDrudge, StealthPU, VoodooDollPU |
| 25_ancientone | CameraObj x8, CinematicTrigger x3, ExitTrigger x1, KeyFramer x6, ObjectivesTrigger x1, Spawner x4, Trigger x7 | 15: AncientOne, AncientOneTentacle, AngerPU, AssaultRiflePU, CClientExplosionSFX, CClientLightningSFX, CalebAI, GabriellaAI, HealthPU, InvulnerabilityPU, IshmaelAI, LifeLeechPU, MegaHealthPU, NecroWardPU, OpheliaAI |
| credits | CameraObj x1, KeyFramer x1, Trigger x1 | 3: CalebAI, DetailSprite, FlickerLight |
| drop | — | none (restored whole) |
| end | CameraObj x1, KeyFramer x1, Trigger x1 | 5: CalebAI, GabriellaAI, IshmaelAI, OpheliaAI, RandomSoundFX |
| multi/bb_area15 | Trigger x5 | 16: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_bloodarena | Trigger x5 | 20: AngerPU, BMGAmmoPU, BatteryAmmoPU, BinocularsPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, TheEyePU, WardPU |
| multi/bb_bloodkeep | BigTrigger x4 | 18: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX, TheEyePU, WardPU |
| multi/bb_bodies | Trigger x1 | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, WardPU |
| multi/bb_bridge | Trigger x6 | 16: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX, WardPU |
| multi/bb_brownstar | Trigger x6 | 17: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, TheEyePU, WardPU |
| multi/bb_cabana | Trigger x2 | 17: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, RevenantPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_chosenarena | Trigger x1 | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU |
| multi/bb_condemned | Trigger x5 | 21: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, StealthPU, TheEyePU, WardPU |
| multi/bb_crypt | Trigger x5 | 14: BatteryAmmoPU, BulletAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, StealthPU, WardPU |
| multi/bb_deathcanyon | — | 11: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU |
| multi/bb_drain | — | 14: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_dustbowl | — | 11: BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, ShellAmmoPU, SoundFX |
| multi/bb_fields | — | 10: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, ProximitiesPU, RemotesPU, ShellAmmoPU |
| multi/bb_jgrave | Trigger x7 | 20: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, TheEyePU, TimeBombPU, WardPU |
| multi/bb_midgard | BigTrigger x4, Trigger x3 | 13: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU |
| multi/bb_power | — | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX |
| multi/bb_rooftop | Trigger x1 | 15: AngerPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU |
| multi/ctf_cabana | Trigger x4 | 17: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, RevenantPU, ShellAmmoPU, StealthPU, WardPU |
| multi/ctf_deadcities | Trigger x3 | 18: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RemotesPU, ShellAmmoPU, TheEyePU, WardPU |
| multi/ctf_dmx | — | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, TimeBombPU, WardPU |
| multi/ctf_jam | — | 21: AngerPU, BMGAmmoPU, BatteryAmmoPU, BinocularsPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RemotesPU, ShellAmmoPU, TheEyePU, TimeBombPU, WardPU |

## Execution evidence

macOS (Apple clang 21), branch on `cfe2bf58`:

- **Dev** (`dev` preset, private data configured): the full headless label runs 1,277 tests
  (eight new `save_world_logic.*`, including the private case); 1,269 pass. The failures are
  the seven pre-existing private-data content checks (doors.catalog, pickups.catalog,
  ai_recovery.catalog, ai_escape.catalog, assault.ai, mad_scientist.wounds,
  lightning.catalog) and `locomotion.cityhub_roam`, which exceeded its 120 s wall limit under
  the machine's load and passes alone (75 s at load average 60). `save_world_logic.private`
  takes 112 s.
- **Release** (inspector-free `release` preset): builds; its headless run gives the same
  1,269 of 1,277 with the same eight; `cityhub_roam` again passes alone.
- **Clang ASan/UBSan** (`testing.md` recipe, `ASAN_OPTIONS=detect_leaks=0` because Apple
  clang has no LeakSanitizer), over the save, object-contract, trigger, keyframe, camera,
  cinematic, exit, spawner, door, conversation and objectives suites (136 tests): no test
  output holds a sanitizer report. Every headless `save_world_logic`, `save_movers` and
  `save_session` case passes, and `save_world_logic.private` passes in 1,683 s (limit
  1,800 s). At a load average of 60 to 90 from other sessions, eight tests exceeded fixed wall
  limits; run serially, all pass except the known `doors.catalog` content failure.
  `objects.message_churn` (5.5 s CPU) and `cameras.catalog` (25 s CPU) still exceed their
  10 s and 60 s wall limits at that load but pass when run directly.

- **After rebasing onto lot 6** (`2872e92d`): the dev headless run gives 1,282 of 1,289, the
  seven pre-existing private-data failures only.

No GPU, interactive or menu validation is specific to this lot.

## For later lots

- Lot 10: SubwayA/B/C's `sublight` KeyFramer controls twelve GlowingLights; once they are
  Original, the KeyFramer tail keeps their offsets across a load.
- Lot 11/12: KeyFramer and CameraObj targets that are NPCs read back as NULL or are missing
  from the rebuilt list until their classes are Original.
- Lot 14: the whole-world audit can rely on every trigger, spawner, keyframer and camera.

## Limits

- A real game is still refused: every world except `drop` holds classes of lots 9 to 13.
- KeyFramer key sounds, CinematicTrigger voices and the objective sound are lost as in the
  original; a playing cue restarts and resends its message.
- A timed active camera restarts its full duration, as in the original.
- Restored always-load targets of a finished path snap back to their authored pose, as in
  the original.
