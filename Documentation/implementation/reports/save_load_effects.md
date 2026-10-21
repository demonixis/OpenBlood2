# Effect publishers and audio emitters (save/load lot 10)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 10 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[implemented effect publishers and audio emitters](../../contracts/save_load_contracts.md#implemented-effect-publishers-and-audio-emitters-lot-10);
the [source ledger](../sources/save_load_effects_sources.json) lists the references consulted.
The original Save/Load bodies of SoundFX, RandomSoundFX, the ClientLightFX family, the scene
explosion, particle stream, lightning and warp gate emitters and DetailSprite now run, and their
21 registered classes are Original with payload version 1. A save of a real game is still
refused by name in every world that still holds classes of lots 9 or 12; with lots 7 and 11,
06_airship1, 08_subwayb, credits and end now save and load whole, as `drop` already did. M2 closure
and M2-GPU-001 are unchanged.

## What the original does

The [world-object audit](../save_load_audit/world_objects.md) (§12–§18, §21, §22, §27) gives the
blocks. Each class writes only "the data that changes": the original engine record also kept
the object's persistent special-effect message (the light's SFX_LIGHT, the stream's
SFX_PARTICLESTREAM) and replayed it on a restore. Instant effect messages (an explosion's
components, a lightning bolt, the warp gate's three setup messages) were never kept. Sounds are
not saved; SoundFX's Load restarts the current state's sound. Four losses come from the original
code: RandomSoundFX is never saveable, a light that starts off is never saveable, SoundFX's
`PostPropRead` and Load clear its schedule (frozen ramps), and a restored warp gate never
publishes its effects again.

## Delivered behavior

- **Bodies.** The eight classes handle `MID_SAVEOBJECT`/`MID_LOADOBJECT` in the original place
  and write the original fields in the original order and widths. ClientLightFX's record ends
  with lot 8's `CDestructable` block.
- **Restore branches.** `PRECREATE_SAVEGAME` reads no property. ClientLightFX keeps its
  `PostPropRead` (flags `VISIBLE | GOTHRUWORLD`, next update 0.01 s, from the constructor's ON);
  the explosion, lightning and warp gate initial updates stay unconditional (0.1 s, 0.1 s, 1 s).
  `INITIALUPDATE_SAVEGAME` skips the other classes' initialization.
- **Checks before change.** Each Load decodes its whole block, checks it with the precreate
  bounds and the existing light, stream and warp gate validators, and only then assigns;
  refusals go through lot 8's `RejectPayload` and name the class.
- **Effect messages and republication.** ClientLightFX and the particle stream append the effect
  message they published, in its original order, to their block; Load publishes it again, once
  per owner. The stream keeps its published descriptor (the initial ON included), as the
  replayed message did. Light waveforms evaluate the absolute restored clock.
- **Departures (decision 4).** RandomSoundFX and initially-off lights are saved (lot 4 marks
  them); SoundFX keeps its saved next update and Load restarts the sound without rescheduling;
  a warp gate past its first update publishes its effects again from its Load.
- **Rollback.** `RestoreObjects`' rollback now discards the lights, streams and warp gates the
  removed records' Loads published, before the clock moves back (a stale stream would otherwise
  fail the next update with a clock that ran backwards).
- **Declarations.** `registration.cpp` declares the 21 classes Original, version 1.

## Findings

- **Light schedule.** The original restore resets every light's next update to 0.01 s, so the
  restored world's lights poll at a different phase. The polling only reschedules and checks the
  lifetime, so evaluated light state is unaffected; the schedule and flags are the listed
  rewrites. An initially-off light, saved only through the departure, follows the same path.
- **Invisible lights.** A light switched off keeps the colour its client last evaluated; a
  republished one evaluates it again at the restored time. Visible lights match exactly; the
  comparison covers what is lit.
- **Restarted sounds.** SoundFX restarts its sound from the start, as the original Load does, so
  a restored non-ambient one-shot plays again; the state machine itself follows the saved
  deadline.
- **Client state.** Republished lights, streams and warp gates start their client state again
  (particles, ramp clocks), as the original's replayed messages did. A stream that was turned on
  after an initially-off publication ramps up again.
- **Never-assigned fields.** The explosion's `m_fOffset`, lightning's `bTriggered` and the warp
  gate's `bInitiallyOn` keep their constructor values and must match on load. DetailSprite's
  uninitialized `m_bAlignedToWorld` is carried.
- **Tests that asserted refusal.** `lights.defaults`, `randomsound.failures`,
  `sceneexplosion.properties` and `soundfx.failures` checked that a bare restore creation was
  refused; they now check that it reads nothing and publishes or starts nothing before its Load.
  `sprites` keeps its refusal: without a saved filename the engine refuses the sprite object.

## Tests

`save_effects_tests` (labels `headless;objects;save`; `private` adds the commercial data). Each
round trip saves the world, restores it into a fresh session, checks the restored members, the
published effects and the restarted voices, requires the re-save to be byte-identical apart from
the listed rewrites, then requires every later save of the restored world, and a probe of the
evaluated effects, to match the saved world's step by step. Every step also checks that no light,
stream, warp gate or SoundFX voice exists twice for one owner.

| Case | Checks |
| --- | --- |
| `sounds` | SoundFX ambient ON (loop restarts), ramping up and down (ramp sound restarts, saved deadline kept, next state reached at the same step), non-ambient ON, OFF (silent); 50 identical steps |
| `random_sounds` | Random and sequential RandomSoundFX mid-interval and one switched off: saved waits, list, sequence cursor; the same plays, choices and offsets over 80 steps |
| `lights` | All 14 light classes mid-phase with their own frequencies and phases, an initially-off and a switched-off light: one republished effect each with the saved message, evaluated radius and colour identical for 60 steps, listed flag rewrite |
| `explosions` | A pending emitter keeps position, normal, type, damage and sound; a triggered one is removed by its next update and not replayed |
| `streams` | Streams on, off, initially on then stopped, initially off then started: one republished stream each, published ON and descriptor, saved ON in the user flags |
| `lightning` | Saved mid-bolt: the bolt is lost, endpoints, shape and settings kept |
| `warp_gates` | A gate ramping up publishes its effects again, once; a gate saved before its first update publishes at that update |
| `sprites` | An animated DetailSprite mid-frame and a filename-less copy: sprite track, scale, flags; identical frames for 60 steps |
| `refusals` | Ten corrupt fields across the eight classes and a truncated and an extended payload of each: refused by class name, every created record and every republished effect rolled back, the next step valid |

### Real worlds

`save_effects.private` enters every mounted world with a local player, runs 30 steps, lists the
classes still blocking a whole save, and restores an image of every Original object of the world
(lots 5, 8 and 10, the player included) through the real restore-mode world and candidate
transaction (`load_game`). It requires every light and stream to be published again once, every
warp gate past its first update too, every ambient ON SoundFX to hold one voice, and the world to
run 10 more steps with no unavailable service and no duplicate. A world with no blocking class
restores whole twice.

Result (macOS, Apple clang 21, on top of lots 6, 7, 11 and 13): 58 of 58 worlds entered; 53
restored all their Original objects and 5 restored whole, twice (06_airship1, 08_subwayb,
credits, drop and end). Before lots 7 and 11, 57 restored their Original objects and only `drop`
was whole. Over all worlds the lot covers 125 SoundFX, 54 RandomSoundFX, 72
lights, 43 scene explosions, 91 particle streams, 10 lightning emitters, 7 warp gates and 122
DetailSprites; all 7 warp gates (09_abandoned_buildings and 24b_arena) republished once. Every
other world still holds classes of lots 9 or 12 (pickups and bosses, the Thief family, TheHand
and the Zealot).

| World | Lot-10 objects | Original records restored | Still blocking a whole save |
| --- | --- | --- | --- |
| 01_subwaya | CClientExplosionSFX x9, DetailSprite x12, RandomSoundFX x2, lights x12 | 193 (12 lights, 0 streams, 0 gates) | Gideon x1 |
| 02_cityhub | CClientParticleStreamSFX x8, DetailSprite x6, RandomSoundFX x1, SoundFX x5, lights x4 | 329 (4 lights, 8 streams, 0 gates) | AssaultRiflePU x1, BulletAmmoPU x6, FlareAmmoPU x2, FlashlightPU x1, Gideon x1, MedKitPU x1, ProximitiesPU x2, RemotesPU x2, SubMachineGunPU x1, WardPU x2 |
| 02_cityhubb | CClientParticleStreamSFX x7, DetailSprite x22, RandomSoundFX x1, SoundFX x3, lights x3 | 323 (3 lights, 7 streams, 0 gates) | BoneLeech x7, BulletAmmoPU x3, FlareAmmoPU x2, FlashlightPU x1, MegaHealthPU x1, ProximitiesPU x2, RemotesPU x2, SubMachineGunPU x1 |
| 02_cityhubc | CClientParticleStreamSFX x7, DetailSprite x22, RandomSoundFX x1, SoundFX x1, lights x3 | 296 (3 lights, 7 streams, 0 gates) | BatteryAmmoPU x2, BoneLeech x17, BugSprayPU x1, BulletAmmoPU x5, DieBugDieAmmoPU x1, FlareAmmoPU x1, FlashlightPU x1, HealthPU x1, MegaHealthPU x1, ProximitiesPU x2, RemotesPU x2, ShellAmmoPU x2, SubMachineGunPU x1 |
| 03_museum | CClientParticleStreamSFX x6, RandomSoundFX x1, SoundFX x3, lights x2 | 435 (2 lights, 6 streams, 0 gates) | Gideon x1, InvulnerabilityPU x1, NecroWardPU x1, ProximitiesPU x3, ShotgunPU x1 |
| 04_steamtunnels | CClientParticleStreamSFX x20, DetailSprite x8, SoundFX x1, lights x1 | 313 (1 lights, 20 streams, 0 gates) | FlareAmmoPU x1, HealthPU x6, MedKitPU x1, NecroWardPU x1, NightGogglesPU x1, RemotesPU x6 |
| 05_cdm | CClientParticleStreamSFX x2, DetailSprite x1, RandomSoundFX x2, SoundFX x2 | 416 (0 lights, 2 streams, 0 gates) | BerettaPU x1, BulletAmmoPU x2, HealthPU x1, KeyPickup x3, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, StealthPU x1, WardPU x1 |
| 05_cdmb | SoundFX x3 | 61 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x1, SniperRiflePU x1 |
| 06_airship1 | DetailSprite x7 | whole, twice | none |
| 06_airship2 | DetailSprite x1, SoundFX x1 | 220 (0 lights, 0 streams, 0 gates) | AngerPU x1, AssaultRiflePU x1, BugSprayPU x1, FlareGunPU x1, KeyPickup x1, MedKitPU x1, SniperRiflePU x1 |
| 07_cathedral | CClientExplosionSFX x3, DetailSprite x13, RandomSoundFX x3, lights x6 | 180 (6 lights, 0 streams, 0 gates) | AngerPU x1, BatteryAmmoPU x3, BoneLeech x2, BulletAmmoPU x1, FlareAmmoPU x1, KeyPickup x2, MegaHealthPU x2, NecroWardPU x2, NightGogglesPU x1, ShellAmmoPU x1, VoodooDollPU x1 |
| 08_subwayb | CClientExplosionSFX x9, RandomSoundFX x2, lights x12 | whole, twice | none |
| 09_abandoned_buildings | CClientParticleStreamSFX x10, CClientWarpGateSFX x1, RandomSoundFX x15, SoundFX x2, lights x6 | 512 (6 lights, 10 streams, 1 gates) | BMGAmmoPU x1, BulletAmmoPU x5, DieBugDieAmmoPU x1, FlareAmmoPU x1, FlashlightPU x2, Gideon x1, HowitzerPU x1, MedKitPU x1, MegaHealthPU x2, NecroWardPU x1, NightGogglesPU x1, RemotesPU x2, ShellAmmoPU x3, ShotgunPU x1, Thief x7, TimeBombPU x5, ZealotAI x3 |
| 10_sewagetreatmentplant | CClientParticleStreamSFX x1 | 63 (0 lights, 1 streams, 0 gates) | AssaultRiflePU x1, BMGAmmoPU x1, BulletAmmoPU x1, FlashlightPU x1, MedKitPU x1, ShellAmmoPU x1, Thief x12 |
| 11_slaughter_house | CClientParticleStreamSFX x1, RandomSoundFX x1, SoundFX x1 | 463 (0 lights, 1 streams, 0 gates) | BMGAmmoPU x2, BoneLeech x9, DeathRayPU x1, FlareAmmoPU x2, FlareGunPU x1, HealthPU x2, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x2, RemotesPU x5, TheEyePU x1, Thief x5, TimeBombPU x2, ZealotAI x2 |
| 12_train_station | CClientExplosionSFX x5, CClientParticleStreamSFX x7, RandomSoundFX x7, SoundFX x2, lights x9 | 395 (9 lights, 7 streams, 0 gates) | AngerPU x1, BatteryAmmoPU x1, BinocularsPU x1, BoneLeech x11, BulletAmmoPU x1, DieBugDieAmmoPU x1, FlareAmmoPU x1, FlashlightPU x1, HowitzerAmmoPU x2, InvulnerabilityPU x1, MedKitPU x1, ProximitiesPU x2, RemotesPU x2, ShellAmmoPU x1, SniperRiflePU x1, Thief x5, VoodooDollPU x1, ZealotAI x8 |
| 12_train_stationb | CClientParticleStreamSFX x1, SoundFX x1 | 347 (0 lights, 1 streams, 0 gates) | BMGAmmoPU x1, BoneLeech x10, RemotesPU x2, ShellAmmoPU x2, TimeBombPU x4, ZealotAI x2 |
| 13_downtown | CClientParticleStreamSFX x1, DetailSprite x3, RandomSoundFX x5, lights x1 | 235 (1 lights, 1 streams, 0 gates) | BMGAmmoPU x2, BatteryAmmoPU x1, BoneLeech x2, BugSprayPU x1, BulletAmmoPU x5, DieBugDieAmmoPU x3, FlareAmmoPU x2, FlashlightPU x1, HowitzerAmmoPU x3, NapalmCannonPU x1, NightGogglesPU x1, ProximitiesPU x2, ShellAmmoPU x5, ShotgunPU x1, SniperRiflePU x1, TheHandAI x3, TimeBombPU x2, ZealotAI x8 |
| 14_subwayc | CClientParticleStreamSFX x6, RandomSoundFX x2, lights x12 | 322 (12 lights, 6 streams, 0 gates) | AssaultRiflePU x1, BatteryAmmoPU x1, DieBugDieAmmoPU x2, FlareAmmoPU x1, FuelAmmoPU x1, NecroWardPU x1, RemotesPU x2, ShellAmmoPU x1, Thief x2 |
| 15_crypt | RandomSoundFX x1 | 117 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x4, BatteryAmmoPU x3, BulletAmmoPU x8, DieBugDieAmmoPU x1, FlareAmmoPU x3, FlashlightPU x1, FuelAmmoPU x3, HowitzerAmmoPU x3, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, NightGogglesPU x1, ShellAmmoPU x4, StealthPU x1, TheHandAI x5, Thief x7, VoodooDollPU x1 |
| 16_bridge | CClientParticleStreamSFX x3, DetailSprite x2, SoundFX x1 | 230 (0 lights, 3 streams, 0 gates) | AssaultRiflePU x1, BatteryAmmoPU x2, BoneLeech x2, BulletAmmoPU x2, DieBugDieAmmoPU x5, FuelAmmoPU x1, HealthPU x1, HowitzerAmmoPU x2, MedKitPU x1, ProximitiesPU x3, RemotesPU x4, ShellAmmoPU x1, ShotgunPU x1, SniperRiflePU x1, Thief x4, WardPU x2 |
| 17_entrance | DetailSprite x8, SoundFX x10 | 339 (0 lights, 0 streams, 0 gates) | BulletAmmoPU x2, HealthPU x2, KeyPickup x1, MiniGunPU x1, ShellAmmoPU x3, WardPU x1 |
| 18_temple | none | 127 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x2, BulletAmmoPU x4, DieBugDieAmmoPU x2, DrudgePriest x1, FlareAmmoPU x3, FlashlightPU x1, FuelAmmoPU x1, HealthPU x1, HowitzerAmmoPU x1, LifeLeechPU x1, MegaHealthPU x2, NecroWardPU x1, ShellAmmoPU x3, Thief x17 |
| 19_lobby | SoundFX x8 | 384 (0 lights, 0 streams, 0 gates) | DieBugDieAmmoPU x6, DrudgePriest x1, FlareAmmoPU x2, FuelAmmoPU x1, KeyPickup x1, MegaHealthPU x1, StealthPU x1 |
| 19_lobbyb | none | 94 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x1, BatteryAmmoPU x1, DieBugDieAmmoPU x1, FlareAmmoPU x1, FuelAmmoPU x1, HowitzerAmmoPU x1, MegaHealthPU x1 |
| 19_lobbyc | SoundFX x1 | 79 (0 lights, 0 streams, 0 gates) | AngerPU x1, BatteryAmmoPU x2, BulletAmmoPU x2, FuelAmmoPU x2, HealthPU x2, MegaHealthPU x1, NecroWardPU x1, WardPU x2, ZealotAI x1 |
| 20_powerstation | CClientLightningSFX x1, SoundFX x12 | 127 (0 lights, 0 streams, 0 gates) | BatteryAmmoPU x2, BulletAmmoPU x2, HealthPU x1, HowitzerAmmoPU x2, MedKitPU x1, SubMachineGunPU x1, TheHandAI x2, WardPU x1 |
| 21_laboratory | CClientLightningSFX x1, RandomSoundFX x5, SoundFX x6 | 267 (0 lights, 0 streams, 0 gates) | BatteryAmmoPU x4, BoneLeech x6, DieBugDieAmmoPU x4, FlareAmmoPU x1, HealthPU x2, KeyPickup x4, MegaHealthPU x1, SingularityPU x1, TheHandAI x2, WardPU x3 |
| 22_office | DetailSprite x2 | 478 (0 lights, 0 streams, 0 gates) | BatteryAmmoPU x2, BoneLeech x3, FuelAmmoPU x1, HealthPU x1, HowitzerAmmoPU x1, KeyPickup x1, MedKitPU x1, MegaHealthPU x3, NecroWardPU x1 |
| 23_rooftop | CClientParticleStreamSFX x4 | 76 (0 lights, 4 streams, 0 gates) | AngerPU x2, BMGAmmoPU x2, BatteryAmmoPU x14, BulletAmmoPU x9, DieBugDieAmmoPU x2, EnhancedGideon x1, FlareAmmoPU x2, FuelAmmoPU x4, HealthPU x9, HowitzerAmmoPU x4, LifeLeechPU x1, MegaHealthPU x2, ShellAmmoPU x5 |
| 24_ancientcity | DetailSprite x2 | 175 (0 lights, 0 streams, 0 gates) | BatteryAmmoPU x1, BulletAmmoPU x1, DrudgePriest x3, EnhancedGideon x1, FlareAmmoPU x2, FuelAmmoPU x3, HealthPU x5, HowitzerAmmoPU x1, InvulnerabilityPU x1, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x1, RemotesPU x1, ShellAmmoPU x1, UndeadGideon x1, WardPU x1 |
| 24b_arena | CClientExplosionSFX x3, CClientLightningSFX x3, CClientWarpGateSFX x6 | 76 (0 lights, 0 streams, 6 gates) | AngerPU x1, BatteryAmmoPU x2, BulletAmmoPU x7, DieBugDieAmmoPU x2, FuelAmmoPU x4, HealthPU x8, HowitzerAmmoPU x2, InvulnerabilityPU x1, MegaHealthPU x1, ProximitiesPU x2, RemotesPU x2, ShellAmmoPU x2, StealthPU x1, VoodooDollPU x1 |
| 25_ancientone | CClientExplosionSFX x14, CClientLightningSFX x5 | 98 (0 lights, 0 streams, 0 gates) | AncientOne x2, AncientOneTentacle x16, AngerPU x1, AssaultRiflePU x1, HealthPU x3, InvulnerabilityPU x1, LifeLeechPU x1, MegaHealthPU x1, NecroWardPU x1 |
| credits | DetailSprite x2, lights x1 | whole, twice | none |
| drop | none | whole, twice | none |
| end | RandomSoundFX x1 | whole, twice | none |
| multi/bb_area15 | none | 28 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x1, BatteryAmmoPU x2, BulletAmmoPU x7, DieBugDieAmmoPU x2, FlareAmmoPU x1, FuelAmmoPU x2, HealthPU x10, HowitzerAmmoPU x2, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x3, RemotesPU x2, ShellAmmoPU x4, StealthPU x1, WardPU x1 |
| multi/bb_bloodarena | none | 52 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x2, BatteryAmmoPU x4, BinocularsPU x1, BulletAmmoPU x12, DieBugDieAmmoPU x4, FlareAmmoPU x2, FuelAmmoPU x4, HealthPU x7, HowitzerAmmoPU x4, InvulnerabilityPU x1, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x8, RemotesPU x2, ShellAmmoPU x6, StealthPU x1, TheEyePU x1, WardPU x4 |
| multi/bb_bloodkeep | DetailSprite x10, SoundFX x4 | 35 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x1, BatteryAmmoPU x2, BulletAmmoPU x6, DieBugDieAmmoPU x2, FlareAmmoPU x3, FuelAmmoPU x2, HealthPU x4, HowitzerAmmoPU x2, MedKitPU x1, MegaHealthPU x1, ProximitiesPU x3, RemotesPU x2, ShellAmmoPU x6, TheEyePU x1, WardPU x1 |
| multi/bb_bodies | none | 21 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x1, BatteryAmmoPU x3, BulletAmmoPU x4, DieBugDieAmmoPU x2, FlareAmmoPU x3, FuelAmmoPU x3, HealthPU x4, HowitzerAmmoPU x2, InvulnerabilityPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x3, ShellAmmoPU x6, WardPU x1 |
| multi/bb_bridge | CClientParticleStreamSFX x2, DetailSprite x1, SoundFX x3 | 114 (0 lights, 2 streams, 0 gates) | AngerPU x1, BMGAmmoPU x4, BatteryAmmoPU x8, BulletAmmoPU x13, DieBugDieAmmoPU x4, FlareAmmoPU x7, FuelAmmoPU x7, HowitzerAmmoPU x4, MegaHealthPU x1, ProximitiesPU x13, RemotesPU x12, ShellAmmoPU x7, WardPU x1 |
| multi/bb_brownstar | RandomSoundFX x1, SoundFX x3 | 44 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x4, BatteryAmmoPU x5, BulletAmmoPU x9, DieBugDieAmmoPU x5, FlareAmmoPU x10, FuelAmmoPU x5, HealthPU x8, HowitzerAmmoPU x5, InvulnerabilityPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x10, ShellAmmoPU x9, TheEyePU x1, WardPU x4 |
| multi/bb_cabana | none | 15 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x1, BatteryAmmoPU x4, BulletAmmoPU x6, DieBugDieAmmoPU x4, FlareAmmoPU x2, FuelAmmoPU x2, HealthPU x4, HowitzerAmmoPU x2, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x3, RemotesPU x3, RevenantPU x1, ShellAmmoPU x4, StealthPU x1, WardPU x2 |
| multi/bb_chosenarena | none | 24 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x6, BatteryAmmoPU x6, BulletAmmoPU x10, DieBugDieAmmoPU x5, FlareAmmoPU x5, FuelAmmoPU x3, HealthPU x3, HowitzerAmmoPU x3, InvulnerabilityPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x12, RemotesPU x12, ShellAmmoPU x10 |
| multi/bb_condemned | CClientParticleStreamSFX x3, RandomSoundFX x1, SoundFX x4 | 72 (0 lights, 3 streams, 0 gates) | AngerPU x1, BMGAmmoPU x6, BatteryAmmoPU x8, BulletAmmoPU x10, DieBugDieAmmoPU x7, FlareAmmoPU x8, FuelAmmoPU x5, HealthPU x16, HowitzerAmmoPU x6, InvulnerabilityPU x1, MedKitPU x1, MegaHealthPU x1, NecroWardPU x2, ProximitiesPU x9, ShellAmmoPU x10, StealthPU x1, TheEyePU x1, WardPU x4 |
| multi/bb_crypt | RandomSoundFX x1, SoundFX x5 | 65 (0 lights, 0 streams, 0 gates) | BatteryAmmoPU x4, BulletAmmoPU x5, FlareAmmoPU x1, FuelAmmoPU x4, HealthPU x2, HowitzerAmmoPU x3, InvulnerabilityPU x1, MegaHealthPU x1, ProximitiesPU x6, ShellAmmoPU x5, StealthPU x1, WardPU x2 |
| multi/bb_deathcanyon | none | 5 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x1, BatteryAmmoPU x3, BulletAmmoPU x4, DieBugDieAmmoPU x2, FlareAmmoPU x2, FuelAmmoPU x2, HowitzerAmmoPU x3, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x3, ShellAmmoPU x4 |
| multi/bb_drain | none | 7 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x1, BatteryAmmoPU x3, BulletAmmoPU x6, DieBugDieAmmoPU x2, FlareAmmoPU x2, FuelAmmoPU x1, HealthPU x4, HowitzerAmmoPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x3, ShellAmmoPU x4, StealthPU x2, WardPU x1 |
| multi/bb_dustbowl | SoundFX x4 | 20 (0 lights, 0 streams, 0 gates) | BatteryAmmoPU x12, BulletAmmoPU x9, DieBugDieAmmoPU x4, FlareAmmoPU x5, FuelAmmoPU x7, HealthPU x4, HowitzerAmmoPU x4, MegaHealthPU x1, ProximitiesPU x13, ShellAmmoPU x7 |
| multi/bb_fields | none | 53 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x2, BatteryAmmoPU x4, BulletAmmoPU x6, DieBugDieAmmoPU x2, FlareAmmoPU x3, FuelAmmoPU x4, HowitzerAmmoPU x2, ProximitiesPU x5, RemotesPU x5, ShellAmmoPU x6 |
| multi/bb_jgrave | RandomSoundFX x1 | 74 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x3, BatteryAmmoPU x4, BulletAmmoPU x6, DieBugDieAmmoPU x2, FlareAmmoPU x4, FuelAmmoPU x4, HealthPU x4, HowitzerAmmoPU x4, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, NightGogglesPU x1, ProximitiesPU x2, RemotesPU x6, ShellAmmoPU x6, TheEyePU x1, TimeBombPU x8, WardPU x3 |
| multi/bb_midgard | none | 29 (0 lights, 0 streams, 0 gates) | BMGAmmoPU x1, BatteryAmmoPU x2, BulletAmmoPU x6, DieBugDieAmmoPU x2, FlareAmmoPU x5, FuelAmmoPU x3, HealthPU x10, HowitzerAmmoPU x2, MedKitPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x4, ShellAmmoPU x4 |
| multi/bb_power | SoundFX x39 | 89 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x1, BatteryAmmoPU x4, BulletAmmoPU x4, DieBugDieAmmoPU x3, FlareAmmoPU x4, FuelAmmoPU x1, HealthPU x4, HowitzerAmmoPU x1, MegaHealthPU x1, NecroWardPU x1, ProximitiesPU x9, RemotesPU x3, ShellAmmoPU x7 |
| multi/bb_rooftop | CClientParticleStreamSFX x2 | 44 (0 lights, 2 streams, 0 gates) | AngerPU x1, BatteryAmmoPU x3, BulletAmmoPU x8, DieBugDieAmmoPU x4, FlareAmmoPU x8, FuelAmmoPU x4, HealthPU x6, HowitzerAmmoPU x4, InvulnerabilityPU x1, MegaHealthPU x1, ProximitiesPU x5, RemotesPU x3, ShellAmmoPU x5, StealthPU x1 |
| multi/ctf_cabana | none | 26 (0 lights, 0 streams, 0 gates) | AngerPU x2, BMGAmmoPU x2, BatteryAmmoPU x8, BulletAmmoPU x12, DieBugDieAmmoPU x8, FlareAmmoPU x4, FuelAmmoPU x4, HealthPU x8, HowitzerAmmoPU x4, MegaHealthPU x2, NecroWardPU x2, ProximitiesPU x6, RemotesPU x6, RevenantPU x2, ShellAmmoPU x8, StealthPU x2, WardPU x4 |
| multi/ctf_deadcities | none | 30 (0 lights, 0 streams, 0 gates) | AngerPU x2, BMGAmmoPU x4, BatteryAmmoPU x6, BulletAmmoPU x8, DieBugDieAmmoPU x2, FlareAmmoPU x2, FuelAmmoPU x4, HealthPU x30, HowitzerAmmoPU x2, MedKitPU x2, MegaHealthPU x2, NecroWardPU x2, NightGogglesPU x2, ProximitiesPU x6, RemotesPU x6, ShellAmmoPU x8, TheEyePU x2, WardPU x10 |
| multi/ctf_dmx | none | 20 (0 lights, 0 streams, 0 gates) | AngerPU x1, BMGAmmoPU x2, BatteryAmmoPU x8, BulletAmmoPU x10, DieBugDieAmmoPU x4, FlareAmmoPU x4, FuelAmmoPU x4, HealthPU x1, HowitzerAmmoPU x8, MegaHealthPU x3, ProximitiesPU x3, RemotesPU x3, ShellAmmoPU x7, TimeBombPU x4, WardPU x1 |
| multi/ctf_jam | none | 11 (0 lights, 0 streams, 0 gates) | AngerPU x2, BMGAmmoPU x16, BatteryAmmoPU x8, BinocularsPU x2, BulletAmmoPU x18, DieBugDieAmmoPU x8, FlareAmmoPU x4, FuelAmmoPU x12, HealthPU x36, HowitzerAmmoPU x12, InvulnerabilityPU x1, MedKitPU x2, MegaHealthPU x2, NecroWardPU x2, NightGogglesPU x2, ProximitiesPU x14, RemotesPU x14, ShellAmmoPU x18, TheEyePU x4, TimeBombPU x8, WardPU x8 |

## Verification

macOS 27.2, Apple clang 21. The full runs below were made on `cfe2bf58` (lot 5). After the rebase
onto lots 6, 7, 11 and 13 (`67c97f58`) the dev headless suite again fails only the 7 known tests,
and every `save_*` test passes in dev, private runs included (completed at 01:31 on
2026-10-21). The retail data was then deleted by an unrelated scratch-directory accident, so the
Release and ASan/UBSan runs on this final tree are headless only (`-LE private`: 959 of 959 pass
in both, no sanitizer report); their private
`save_*` runs passed on the lot-7/13 rebase (`fed1c924`) and are **pending a re-run** on this
tree once the data is restored.
- **Combined lots.** Whole-world comparisons now hold every lot's classes, so the save tests share
  one list of listed rewrites (`tests/save_rewrites.h`, lots 5, 7, 8, 10 and 11). With lots 7, 10
  and 11 together, five worlds save whole: `save_session.private` accepts a whole save of
  08_SubwayB, `save_native.private` compares a re-save through that list, and `save_movers.private`
  and `save_ai.worlds` count a whole restore as restoring their objects. `save_session.declarations`
  now only requires some Unsupported classes to remain (98 after lots 5–11 and 13) and uses a
  synthetic Unsupported class for its refusal.

- **Dev** (`dev` preset) headless: every test passes except the 7 known private-data content
  failures (`doors.catalog`, `pickups.catalog`, `ai_recovery.catalog`, `ai_escape.catalog`,
  `assault.ai`, `mad_scientist.wounds`, `lightning.catalog`). `save_effects.private` passes
  (58 s).
- **Inspector-free Release** (`release` preset) headless: 1160 of 1168 pass; the same 7 failures
  and a `locomotion.cityhub_roam` timeout at a machine load average near 160, which passes when
  run alone. `save_effects.private` passes.
- **Clang ASan/UBSan** (`BLOOD2_SANITIZERS=ON`, inspector off; LeakSanitizer is unavailable with
  Apple clang, so `detect_leaks=0`): no sanitizer report in any test. Every save test passes,
  including `save_effects.private` (1,373 s), `save_movers.private` and `save_session.private`;
  besides 6 of the 7 known failures (`doors.catalog` timed out instead), 21 private or long tests
  exceeded their fixed wall limits at a load average of 80–90. Ten pass when rerun three at a time; the other
  eleven pass when run alone without a wall limit (`locomotion.cityhub_roam` in 475 s), except
  `doors.catalog`, the known content failure.
- Four existing tests were updated (see Findings); `save_session.declarations` lists the 21 new
  Original classes and no longer expects them in the refusal.

## Re-run on the restored data

Re-run on 2026-10-21 (save/load lot 14), after the retail data was restored, on `019123b0` plus
the two music commits, macOS, Apple clang 21. Dev and inspector-free Release, headless with private
data: every save test passes; the only failures are the 7 known private-data content checks. Clang
ASan/UBSan (`detect_leaks=0`) ran the private boss and enemy suites (gideon_*, enhanced_gideon,
undead_gideon, ancient_one, naga, leech, later_levels, campaign_expansion) and every private save
test of lots 5 to 13: all pass, with no sanitizer report. `save_effects.private` passes in every build (831 s under ASan) after a test fix:
it now accepts lot 13's CTF refusal on `multi/ctf_cabana`, as the other audits do.
