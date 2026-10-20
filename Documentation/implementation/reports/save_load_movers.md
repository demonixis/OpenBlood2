# Movers and brushes (save/load lot 8)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 8 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules
are recorded in the contract's
[implemented movers and brushes](../../contracts/save_load_contracts.md#implemented-movers-and-brushes-lot-8);
the [source ledger](../sources/save_load_movers_sources.json) lists the references
consulted. The original Save/Load bodies of the doors, rotators, destructible brushes and
models, their debris, the volume brushes, TripLaser and Tchernotronic now run, and their 18
classes are Original with payload version 1. A save of a real game is still refused by name
because every world also holds classes of other lots. M2 closure and M2-GPU-001 are unchanged.

## What the original does

The [world-object audit](../save_load_audit/world_objects.md) (§4–§6, §9–§11, §23, §24) and the
[character audit](../save_load_audit/characters.md) (§2, §19) give the field order of each
block. A class writes its block before calling its parent; the aggregates follow in reverse
order of addition. On `PRECREATE_SAVEGAME` the classes read no property but keep their
post-processing (Door's and CDestructableBrush's `PostPropRead`, VolumeBrush's type, name and
constructor flags, TripLaser's model and skin); on `INITIALUPDATE_SAVEGAME` they skip their
initialization, and the aggregates bind their owner from the self reference in their Load.
The state machines resume from the engine's saved next-update delay. Sounds are not saved:
TripLaser restarts its loop in Load, the door and rotator busy loops and the model's sliding
loop are lost. VolumeBrush never restores `m_hPlayerClass`.

## Delivered behavior

- **Bodies.** `Door`, `RotatingDoor`, `Rotating`, `CDestructableBrush`, `CDebris`,
  `WorldModelDebris`, `CDestructableModel`, `VolumeBrush`, `TripLaser` and
  `Tchernotronic` handle `MID_SAVEOBJECT`/`MID_LOADOBJECT` in the original place. The
  `CDestructable` aggregate's byte block wraps the existing typed Save/Load used by NEW_LEVEL
  (`DestructableSave.cpp`); a spawn class absent from the original stays an accepted no-op
  on load, as at precreate.
- **Restore branches.** The `PRECREATE_SAVEGAME` refusals are gone; precreate keeps only the
  original post-processing, and initial update is skipped (`RotatingBrush`/`RotatingModel`
  still bind the aggregate, as the original does unconditionally).
- **Checks before change.** Each Load decodes the whole block, checks it (finite values,
  original ranges, booleans 0 or 1, RotatingDoor's open directions, the pose of a rotating
  world model) and only then assigns. A refusal goes through the new `RejectPayload` service,
  which fails the active payload with the class name; it is a reviewed pure service, so Save
  bodies may use it too.
- **Helpers.** `game/original/SaveFields.h`: NULL-for-empty strings, bounded string and
  boolean reads, and the inverse of the original relative float clocks.
- **Declarations.** `registration.cpp` declares the 18 classes Original, version 1; the build
  schema now holds them.

## Findings

- **Relative float clocks.** The original writes `t - GetTime()` in float. When that
  subtraction rounds, several absolute values give the same saved difference; the port
  restores the one nearest `difference + GetTime()`, so the re-save is byte-identical, but the
  absolute value can be one ulp away (seen on a spinning-up rotator's start time). Later saves
  of that field can then differ by one ulp; the round-trip tests accept exactly that.
- **Rotator pose.** The port's world-model rotation needs the current pose angles, which
  before the first move are the authored rotation while the source's angles start at zero.
  They are written after the original Rotating fields, and Load checks them against the
  restored rotation.
- **Container code.** The original precreate reapplies the class constructor's container
  code. The port's volume subclasses take their code from properties, so the saved code is
  kept and recorded as a departure; a generic VolumeBrush authored as water stays water.
- **Player class.** The port's engine marks the player in `ContainerPhysics`; it never had
  the original's `m_hPlayerClass` member, so the decision-4 fix is structural. The test
  checks a restored volume still leaves the player on the player branch.
- **Rewrites.** The only listed re-save rewrite is a started WorldModelDebris regaining
  `FULLPOSITIONRES | MODELGOURAUDSHADE` from its parent's precreate.
- **Self-triggered doors.** A door's spawned Trigger is saved by LoadSave reference, so a
  world with such a door is refused naming Trigger until lot 7.
- **Presentation preflight.** A world BSP whose object is absent after a restore counts as
  drawable. A partial image without a container's VolumeBrush therefore fails on a
  container-only texture (05_CDM's `default`); a real save includes the volume.

## Tests

`save_movers_tests` (labels `headless;objects;save`; `private` adds the commercial data).
Every round trip saves the world, restores it into a fresh session (the saved world first
records its images for the following steps, because only one session is bound at a time),
checks the restored members, requires the re-save to be byte-identical apart from the listed
rewrite, and then requires every later save of the restored world to match the saved world's.

| Case | Checks |
| --- | --- |
| `doors` | A Door closed, in its open delay, opening, open (waiting) and closing, with a RotatingDoor mid-swing: state, open time, started-moving, endpoints, sounds, portal, waveform, trigger flags, no restarted busy loop, the swing's angles and posed world-space bounds; a door blocked by a crate it cannot push into the floor; 60 identical steps each |
| `rotators` | A spinning RotatingBrush, one spinning up, one never moved (pose = authored rotation, angles zero) and a RotatingModel: state, angles, pose, clocks (same saved difference), spin time left, owner binding, busy-sound string; 150 identical steps through the end of the spin-up |
| `brushes` | A damaged brush with its debris type and death trigger, a dead brush whose removal is pending, two destroyed brushes that stay gone and world-model debris in flight; damage-list count; 120 identical steps while the dying brush goes and the debris lands and spins down |
| `models` | A damaged model (replacement file and dimensions) and a fading one with its authored yaw: health, files, dimensions, absolute fade clock, model body; 120 identical steps through the end of the fade |
| `volumes` | Water with a current holding a pushable model body, a hidden damage volume and a generic VolumeBrush authored as water: code kept, viscosity, fog, the recreated surface PolyGrid, the body found in the water by the rebuilt container index, and the player-class departure; 60 identical steps |
| `lasers` | A triggered TripLaser (loop requested again in Load), one flickering after TRIGGEROFF and a Tchernotronic counting down: flags, end points, damage clock, strings, removal clock; 40 identical steps until the removal |
| `sky` | A Door and a CDestructableBrush holding sky slots 3 and 4; the restore-mode world's always-load SkyPointers exist before the records, then bind the restored objects again: slots, INSKY flags, no sky failure; 5 identical steps |
| `refusals` | A self-triggered door is refused naming `Trigger (Unsupported)`; corrupt booleans, a corrupt RotatingDoor direction, a truncated and a lengthened Door block each fail the restore by class name with every record rolled back and no string or message left |
| `private` | All 58 worlds: classes still blocking a whole save after entry; for the 50 sky worlds, an image of every lot-8 object restored through `load_game` with every restored sky object back in its slot, the always-load pointers binding again, no unavailable service after 10 steps and a second save; a world with no blocking class restored whole twice |

Updated tests: `save_session.declarations` now expects the 18 Original classes and their
schema; `destructable_models.rejects` expects a restore creation to be accepted and left to
Load.

## Real worlds

All 58 worlds enter; all 50 sky worlds restore their lot-8 objects through the real
restore-mode world and candidate transaction. `drop` (no sky, three lot-8 objects and no
other saveable class after entry) restores whole twice. No sky world is restorable whole
yet; the classes still blocking each world after entry (without a player, which lot 5 adds):

| World | Sky pointers | Lot-8 records restored | Blocking classes |
| --- | --- | --- | --- |
| 01_subwaya | 0 | — | 13: CClientExplosionSFX, CameraObj, CinematicTrigger, CivilianAI, CultistAI, DetailSprite, ExitTrigger, Gideon, GlowingLight, KeyFramer, ObjectivesTrigger, RandomSoundFX, Trigger |
| 02_cityhub | 4 | 220 (4 in sky) | 26: AssaultRiflePU, BigTrigger, BulletAmmoPU, CClientParticleStreamSFX, CameraObj, CinematicTrigger, CivilianAI, ConversationTrigger, CultistAI, DetailSprite, ExitTrigger, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, Gideon, KeyFramer, MedKitPU, ObjectivesTrigger, ProximitiesPU, RandomSoundFX, RemotesPU, SoundFX, SubMachineGunPU, Trigger, WardPU |
| 02_cityhubb | 4 | 222 (4 in sky) | 27: BigTrigger, BoneLeech, BulletAmmoPU, CClientParticleStreamSFX, CameraObj, CinematicTrigger, CivilianAI, ConversationTrigger, CultistAI, DetailSprite, ExitTrigger, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, KeyFramer, MegaHealthPU, ObjectivesTrigger, ProximitiesPU, RandomSoundFX, RemotesPU, ShikariAI, SoulDrudge, SoundFX, Spawner, SubMachineGunPU, Trigger |
| 02_cityhubc | 4 | 198 (4 in sky) | 31: BatteryAmmoPU, BigTrigger, BoneLeech, BugSprayPU, BulletAmmoPU, CClientParticleStreamSFX, CameraObj, CivilianAI, ConversationTrigger, CultistAI, DetailSprite, DieBugDieAmmoPU, ExitTrigger, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, HealthPU, KeyFramer, MegaHealthPU, ObjectivesTrigger, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, SoundFX, Spawner, SubMachineGunPU, Trigger |
| 03_museum | 4 | 309 (4 in sky) | 21: BigTrigger, CClientParticleStreamSFX, CameraObj, CinematicTrigger, CultistAI, ExitTrigger, Fanatic, Gideon, GlowingLight, InvulnerabilityPU, KeyFramer, MadScientistAI, NecroWardPU, ObjectivesTrigger, Prophet, ProximitiesPU, RandomSoundFX, ShotgunPU, SoundFX, Spawner, Trigger |
| 04_steamtunnels | 0 | — | 19: BigTrigger, CClientParticleStreamSFX, CivilianAI, DetailSprite, DrudgeLord, ExitTrigger, Fanatic, FlareAmmoPU, FlickerLight, HealthPU, MedKitPU, NecroWardPU, NightGogglesPU, ObjectivesTrigger, RemotesPU, ShikariAI, SoulDrudge, SoundFX, Trigger |
| 05_cdm | 4 | 228 (4 in sky) | 24: BerettaPU, BulletAmmoPU, CClientParticleStreamSFX, CameraObj, CinematicTrigger, CivilianAI, DetailSprite, ExitTrigger, Fanatic, HealthPU, KeyFramer, KeyPickup, MedKitPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX, Spawner, StealthPU, Trigger, WardPU |
| 05_cdmb | 3 | 27 (3 in sky) | 12: BMGAmmoPU, CameraObj, CinematicTrigger, ExitTrigger, Fanatic, KeyFramer, ObjectivesTrigger, Prophet, SniperRiflePU, SoundFX, Spawner, Trigger |
| 06_airship1 | 3 | 76 (3 in sky) | 8: BigTrigger, CultistAI, DetailSprite, ExitTrigger, Fanatic, ObjectivesTrigger, Spawner, Trigger |
| 06_airship2 | 3 | 143 (3 in sky) | 16: AngerPU, AssaultRiflePU, BigTrigger, BugSprayPU, CultistAI, DetailSprite, ExitTrigger, Fanatic, FlareGunPU, KeyPickup, MedKitPU, ObjectivesTrigger, Prophet, SniperRiflePU, SoundFX, Trigger |
| 07_cathedral | 5 | 83 (5 in sky) | 27: AngerPU, BatteryAmmoPU, BoneLeech, BulletAmmoPU, CClientExplosionSFX, CameraObj, CinematicTrigger, ClientLightFX, DetailSprite, DrudgeLord, ExitTrigger, Fanatic, FlareAmmoPU, KeyFramer, KeyPickup, MegaHealthPU, Naga, NecroWardPU, NightGogglesPU, ObjectivesTrigger, RandomSoundFX, ShellAmmoPU, ShikariAI, SoulDrudge, Spawner, Trigger, VoodooDollPU |
| 08_subwayb | 0 | — | 11: CClientExplosionSFX, CinematicTrigger, CivilianAI, ConversationTrigger, ExitTrigger, Fanatic, GlowingLight, KeyFramer, ObjectivesTrigger, RandomSoundFX, Trigger |
| 09_abandoned_buildings | 5 | 309 (5 in sky) | 38: BMGAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, CClientWarpGateSFX, CameraObj, CinematicTrigger, CivilianAI, DieBugDieAmmoPU, DrudgeLord, ExitTrigger, Fanatic, FlareAmmoPU, FlashlightPU, Flicker3WaveLightFX, Gideon, GlowingLight, HowitzerPU, KeyFramer, MadScientistAI, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ObjectivesTrigger, Prophet, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, ShotgunPU, SoulDrudge, SoundFX, Spawner, StrobeWaveLightFX, Thief, TimeBombPU, Trigger, ZealotAI |
| 10_sewagetreatmentplant | 5 | 25 (5 in sky) | 15: AssaultRiflePU, BMGAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DrudgeLord, ExitTrigger, Fanatic, FlashlightPU, MedKitPU, ObjectivesTrigger, ShellAmmoPU, ShikariAI, SoulDrudge, Thief, Trigger |
| 11_slaughter_house | 3 | 313 (3 in sky) | 31: BMGAmmoPU, BoneLeech, CClientParticleStreamSFX, CinematicTrigger, CivilianAI, ConversationTrigger, CultistAI, DeathRayPU, DrudgeLord, ExitTrigger, Fanatic, FlareAmmoPU, FlareGunPU, HealthPU, KeyFramer, MedKitPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, Prophet, ProximitiesPU, RandomSoundFX, RemotesPU, SoulDrudge, SoundFX, Spawner, TheEyePU, Thief, TimeBombPU, Trigger, ZealotAI |
| 12_train_station | 4 | 236 (4 in sky) | 35: AngerPU, BatteryAmmoPU, BinocularsPU, BoneLeech, BulletAmmoPU, CClientExplosionSFX, CClientParticleStreamSFX, CinematicTrigger, CivilianAI, ClientLightFX, CultistAI, DieBugDieAmmoPU, DrudgeLord, ExitTrigger, Fanatic, FlareAmmoPU, FlashlightPU, HowitzerAmmoPU, InvulnerabilityPU, KeyFramer, MedKitPU, ObjectivesTrigger, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SniperRiflePU, SoulDrudge, SoundFX, Spawner, Thief, Trigger, VoodooDollPU, ZealotAI |
| 12_train_stationb | 4 | 270 (4 in sky) | 15: BMGAmmoPU, BoneLeech, CClientParticleStreamSFX, CinematicTrigger, ExitTrigger, Fanatic, ObjectivesTrigger, Prophet, RemotesPU, ShellAmmoPU, SoundFX, Spawner, TimeBombPU, Trigger, ZealotAI |
| 13_downtown | 10 | 146 (5 in sky) | 31: BMGAmmoPU, BatteryAmmoPU, BoneLeech, BugSprayPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, ClientLightFX, DetailSprite, DieBugDieAmmoPU, DrudgeLord, ExitTrigger, FlareAmmoPU, FlashlightPU, HowitzerAmmoPU, KeyFramer, NapalmCannonPU, NightGogglesPU, ObjectivesTrigger, ProximitiesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, ShotgunPU, SniperRiflePU, SoulDrudge, Spawner, TheHandAI, TimeBombPU, Trigger, ZealotAI |
| 14_subwayc | 0 | — | 23: AssaultRiflePU, BatteryAmmoPU, CClientParticleStreamSFX, CivilianAI, DieBugDieAmmoPU, DrudgeLord, ExitTrigger, Fanatic, FlareAmmoPU, FuelAmmoPU, GlowingLight, KeyFramer, NecroWardPU, ObjectivesTrigger, Prophet, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, Spawner, Thief, Trigger |
| 15_crypt | 4 | 58 (4 in sky) | 29: BMGAmmoPU, BatteryAmmoPU, Behemoth, BulletAmmoPU, CameraObj, CinematicTrigger, DieBugDieAmmoPU, DrudgeLord, ExitTrigger, FlareAmmoPU, FlashlightPU, FuelAmmoPU, HowitzerAmmoPU, KeyFramer, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ObjectivesTrigger, RandomSoundFX, ShellAmmoPU, ShikariAI, SoulDrudge, Spawner, StealthPU, TheHandAI, Thief, Trigger, VoodooDollPU |
| 16_bridge | 3 | 156 (3 in sky) | 35: AssaultRiflePU, BatteryAmmoPU, BigTrigger, BoneLeech, BulletAmmoPU, CClientExplosionSFX, CClientParticleStreamSFX, CameraObj, CinematicTrigger, ConversationTrigger, CultistAI, DetailSprite, DieBugDieAmmoPU, ExitTrigger, Fanatic, FuelAmmoPU, HealthPU, HowitzerAmmoPU, KeyFramer, MedKitPU, ObjectivesTrigger, Prophet, ProximitiesPU, RemotesPU, ShellAmmoPU, ShikariAI, ShotgunPU, SniperRiflePU, SoulDrudge, SoundFX, Spawner, Thief, ToggleTrigger, Trigger, WardPU |
| 17_entrance | 3 | 216 (3 in sky) | 19: BigTrigger, BulletAmmoPU, CameraObj, CinematicTrigger, ConversationTrigger, DetailSprite, ExitTrigger, Fanatic, HealthPU, KeyPickup, MiniGunPU, ObjectivesTrigger, Prophet, ShellAmmoPU, SoundFX, Spawner, ToggleTrigger, Trigger, WardPU |
| 18_temple | 4 | 67 (4 in sky) | 25: BMGAmmoPU, BulletAmmoPU, CameraObj, CinematicTrigger, CultistAI, DieBugDieAmmoPU, DrudgeLord, DrudgePriest, ExitTrigger, FlareAmmoPU, FlashlightPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, KeyFramer, LifeLeechPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, ShellAmmoPU, ShikariAI, SoulDrudge, Spawner, Thief, Trigger |
| 19_lobby | 3 | 265 (3 in sky) | 19: Behemoth, BigTrigger, CinematicTrigger, CultistAI, DeathShroud, DieBugDieAmmoPU, DrudgePriest, ExitTrigger, Fanatic, FlareAmmoPU, FuelAmmoPU, KeyPickup, MegaHealthPU, ObjectivesTrigger, Prophet, SoundFX, Spawner, StealthPU, Trigger |
| 19_lobbyb | 3 | 57 (3 in sky) | 14: BMGAmmoPU, BatteryAmmoPU, DieBugDieAmmoPU, ExitTrigger, Fanatic, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, ObjectivesTrigger, Prophet, ShikariAI, Spawner, Trigger |
| 19_lobbyc | 3 | 52 (3 in sky) | 18: AngerPU, BatteryAmmoPU, Behemoth, BulletAmmoPU, DrudgeLord, ExitTrigger, Fanatic, FuelAmmoPU, HealthPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, ShikariAI, SoundFX, Spawner, Trigger, WardPU, ZealotAI |
| 20_powerstation | 0 | — | 18: BatteryAmmoPU, BulletAmmoPU, CClientLightningSFX, CultistAI, ExitTrigger, Fanatic, HealthPU, HowitzerAmmoPU, MedKitPU, ObjectivesTrigger, Prophet, ShikariAI, SoundFX, Spawner, SubMachineGunPU, TheHandAI, Trigger, WardPU |
| 21_laboratory | 3 | 161 (3 in sky) | 26: BatteryAmmoPU, BoneLeech, CClientLightningSFX, CameraObj, CinematicTrigger, CivilianAI, CultistAI, DieBugDieAmmoPU, ExitTrigger, Fanatic, FlareAmmoPU, HealthPU, KeyPickup, MadScientistAI, MegaHealthPU, ObjectivesTrigger, Prophet, RandomSoundFX, ShikariAI, SingularityPU, SoulDrudge, SoundFX, Spawner, TheHandAI, Trigger, WardPU |
| 22_office | 4 | 385 (4 in sky) | 19: BatteryAmmoPU, BoneLeech, CultistAI, DeathShroud, DetailSprite, ExitTrigger, Fanatic, FuelAmmoPU, HealthPU, HowitzerAmmoPU, KeyPickup, MedKitPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, Prophet, ShikariAI, Spawner, Trigger |
| 23_rooftop | 3 | 58 (3 in sky) | 22: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, CalebAI, CameraObj, CinematicTrigger, DieBugDieAmmoPU, EnhancedGideon, ExitTrigger, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, KeyFramer, LifeLeechPU, MegaHealthPU, OpheliaAI, ShellAmmoPU, Spawner, Trigger |
| 24_ancientcity | 5 | 75 (5 in sky) | 32: BatteryAmmoPU, Behemoth, BigTrigger, BulletAmmoPU, CameraObj, CinematicTrigger, DeathShroud, DetailSprite, DrudgeLord, DrudgePriest, EnhancedGideon, ExitTrigger, Fanatic, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, KeyFramer, MedKitPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, ProximitiesPU, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, Spawner, Trigger, UndeadGideon, WardPU |
| 24b_arena | 5 | 22 (5 in sky) | 29: AngerPU, BatteryAmmoPU, BulletAmmoPU, CClientExplosionSFX, CClientLightningSFX, CClientWarpGateSFX, CameraObj, CinematicTrigger, DieBugDieAmmoPU, DrudgeLord, ExitTrigger, FuelAmmoPU, GabriellaREV, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, IshmaelREV, KeyFramer, MegaHealthPU, ObjectivesTrigger, OpheliaREV, ProximitiesPU, RemotesPU, ShellAmmoPU, SoulDrudge, Spawner, StealthPU, Trigger, VoodooDollPU |
| 25_ancientone | 5 | 44 (5 in sky) | 22: AncientOne, AncientOneTentacle, AngerPU, AssaultRiflePU, CClientExplosionSFX, CClientLightningSFX, CalebAI, CameraObj, CinematicTrigger, ExitTrigger, GabriellaAI, HealthPU, InvulnerabilityPU, IshmaelAI, KeyFramer, LifeLeechPU, MegaHealthPU, NecroWardPU, ObjectivesTrigger, OpheliaAI, Spawner, Trigger |
| credits | 3 | 7 (3 in sky) | 6: CalebAI, CameraObj, DetailSprite, FlickerLight, KeyFramer, Trigger |
| drop | 0 | — | none |
| end | 4 | 5 (4 in sky) | 8: CalebAI, CameraObj, GabriellaAI, IshmaelAI, KeyFramer, OpheliaAI, RandomSoundFX, Trigger |
| multi/bb_area15 | 3 | 22 (3 in sky) | 17: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, Trigger, WardPU |
| multi/bb_bloodarena | 3 | 46 (3 in sky) | 21: AngerPU, BMGAmmoPU, BatteryAmmoPU, BinocularsPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, TheEyePU, Trigger, WardPU |
| multi/bb_bloodkeep | 0 | — | 19: AngerPU, BMGAmmoPU, BatteryAmmoPU, BigTrigger, BulletAmmoPU, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX, TheEyePU, WardPU |
| multi/bb_bodies | 3 | 19 (3 in sky) | 16: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, Trigger, WardPU |
| multi/bb_bridge | 3 | 101 (3 in sky) | 17: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX, Trigger, WardPU |
| multi/bb_brownstar | 3 | 33 (3 in sky) | 18: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, TheEyePU, Trigger, WardPU |
| multi/bb_cabana | 3 | 12 (3 in sky) | 18: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, RevenantPU, ShellAmmoPU, StealthPU, Trigger, WardPU |
| multi/bb_chosenarena | 5 | 22 (5 in sky) | 16: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, Trigger |
| multi/bb_condemned | 4 | 58 (4 in sky) | 22: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, StealthPU, TheEyePU, Trigger, WardPU |
| multi/bb_crypt | 4 | 53 (3 in sky) | 15: BatteryAmmoPU, BulletAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, StealthPU, Trigger, WardPU |
| multi/bb_deathcanyon | 3 | 4 (3 in sky) | 11: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU |
| multi/bb_drain | 3 | 6 (3 in sky) | 14: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_dustbowl | 3 | 15 (3 in sky) | 11: BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, ShellAmmoPU, SoundFX |
| multi/bb_fields | 3 | 52 (3 in sky) | 10: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, ProximitiesPU, RemotesPU, ShellAmmoPU |
| multi/bb_jgrave | 3 | 65 (3 in sky) | 21: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, TheEyePU, TimeBombPU, Trigger, WardPU |
| multi/bb_midgard | 3 | 21 (3 in sky) | 15: BMGAmmoPU, BatteryAmmoPU, BigTrigger, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, Trigger |
| multi/bb_power | 0 | — | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX |
| multi/bb_rooftop | 3 | 40 (3 in sky) | 16: AngerPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, Trigger |
| multi/ctf_cabana | 3 | 21 (3 in sky) | 18: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, RevenantPU, ShellAmmoPU, StealthPU, Trigger, WardPU |
| multi/ctf_deadcities | 5 | 26 (5 in sky) | 19: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RemotesPU, ShellAmmoPU, TheEyePU, Trigger, WardPU |
| multi/ctf_dmx | 3 | 19 (3 in sky) | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, TimeBombPU, WardPU |
| multi/ctf_jam | 5 | 10 (5 in sky) | 21: AngerPU, BMGAmmoPU, BatteryAmmoPU, BinocularsPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RemotesPU, ShellAmmoPU, TheEyePU, TimeBombPU, WardPU |

## Execution evidence

macOS (Apple clang 21), branch on `a96c5b66`:

- **Dev** (`dev` preset, private data configured): the full headless label runs 1,257 tests
  (nine new `save_movers.*`, including the private case); 1,250 pass. The seven failures are
  the pre-existing private-data content checks (doors.catalog, pickups.catalog,
  ai_recovery.catalog, ai_escape.catalog, assault.ai, mad_scientist.wounds,
  lightning.catalog), the same as before this lot. `save_movers.private` takes 8.5 s.
- **Release** (inspector-free `release` preset): builds; its headless run gives the same
  1,250 of 1,257 with the same seven failures.
- **Clang ASan/UBSan** (`testing.md` recipe, `ASAN_OPTIONS=detect_leaks=0` because Apple
  clang has no LeakSanitizer): no test output holds a sanitizer report. The full headless run
  shared the machine with other sessions' builds (load average above 20): besides the seven
  failures above, 14 tests hit their fixed wall-clock limits, including the known
  `locomotion.cityhub_roam`. Rerun with less contention, every one of them passes
  (`save_movers.private` in 704 s, so its limit is 1,800 s); `ai_runtime.clearance_private`,
  `death_effects.world_gibs` and `coverage.catalog` pass when run alone (43 s, 24 s, 7 s)
  but exceed their 90 s and 20 s limits under parallel load. After the last code change the
  save, lot-class, container, object-contract and sky suites pass under the sanitizers
  (134 of 135; `objects.message_churn` passes in 5.7 s of CPU but exceeded its 10 s wall limit
  at that load).

No GPU, interactive or menu validation is specific to this lot.

## For later lots

- Lots 5, 7, 9, 10 and 11: write strings through `save_fields::write_string` (or
  `WriteToMessageString` for non-empty text); a temporary HSTRING per save consumes session
  string tokens, which are never reused. Fail a block the port cannot represent with
  `RejectPayload` before changing any member.
- Lot 5 owns the player's CDestructable path: the aggregate's byte block now exists
  (`CDestructable::SaveMessage`/`LoadMessage`) and its Load still refuses an associated player.
- Lot 7: a Door's self-spawned Trigger is a LoadSave reference and the door-to-trigger link is
  restored by LINK; declaring Trigger Original unblocks every self-triggered door.

## Limits

- A real game is still refused: every world holds classes of lots 5, 7, 9, 10, 11 or 12.
- Doors with a self-spawned Trigger wait for lot 7.
- Portal state is not modelled; the original Load does not re-apply it either.
- A WorldModelDebris whose start services failed restores without its failure flag.
- Busy, sliding and one-shot sounds are lost as in the original; TripLaser's loop restarts.
