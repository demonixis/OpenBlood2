# Native state (save/load lot 13)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 13 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[implemented native state](../../contracts/save_load_contracts.md#implemented-native-state-lot-13);
the [source ledger](../sources/save_load_native_sources.json) lists the references consulted.
Lot 13 adds no payload and declares no class. It does four things:

- it confirms that every engine-implemented class is the original always-load or editor-only class;
- it checks that a load republishes the sky and the PolyGrids exactly once, with the original
  and the enhanced renderer;
- it carries the VoiceMgr and CommLink client state across a load as the original client does;
- it refuses CTF saves with a message that names CTF.

A save of a real game is still refused by name, because every world except `drop` holds classes
of lots 9, 10, 11 or 12. M2 closure and M2-GPU-001 are unchanged.

## What the original does

- **Engine classes.** `engineobjects.c` declares every runtime engine class `CF_ALWAYSLOAD`:
  Model, WorldModel, Container, Sprite, Sound, StartPoint, World, Brush, DemoSkyWorldModel,
  SkyPointer and FogSphere. The others (the lights, OutsideDef, FastApproxArea, ProjectorSprite,
  RaySelecter) are `CF_NORUNTIME`. The engine never saves either kind
  ([shells audit §10.2](../save_load_audit/shells.md#102-sdk-versus-code)). PolyGrid is the game's
  own `CF_ALWAYSLOAD` class, and VolumeBrush recreates its runtime surface in its Load
  ([world objects](../save_load_audit/world_objects.md)).
- **Client.** A load runs `MenuLoadGame` → `StartNewGame`, which calls `VoiceMgr.Reset`
  (BloodClientShell.cpp:3955). Reset clears the unique-line flags and the next play time
  (VoiceMgr.cpp:843-860). The client process, and with it `m_bOn`, the random state and each
  group's last choice, continues. `m_CommLink` is never reset by `StartNewGame`, and nothing saves it.
- **Multiplayer.** The menu Save item and F5/F6 are disabled in multiplayer (MainMenus.cpp:809).
  The CTF classes carry no save code and are never marked saveable
  ([shells audit §9](../save_load_audit/shells.md#9-multiplayer-and-ctf-guards)).

## What OpenBlood2 does

- **Declarations.** BaseClass is NotSaved; Model, Container, Sprite, Sound, DemoSkyWorldModel,
  SkyPointer and StartPoint are AlwaysLoad with payload version 0. WorldModel, FogSphere, World
  and Brush are unregistered: no shipped world places them, and World and Brush are editor-only
  geometry. FlagObject and FlagStand stay NotSaved. The `PRECREATE_SAVEGAME` refusals of Sound
  and the sky classes are unreachable and keep their behavior; their comments now say why. An
  image holding a record of one of these classes is refused by name at the fingerprint stage
  (`class Sound is not registered`).
- **`check_restored_publications`** (`src/game/world_validation.cpp`) runs in `load_game`'s
  validation stage. It requires:
  - live sky registrations unique, and each slot's object registered and slotted once;
  - one published instance per published PolyGrid;
  - no surface shared by two volumes;
  - no PolyGrid beyond the authored rows and the volume surfaces.

  A failure refuses the candidate (`Restored publications: …`) and leaves the source untouched.
- **`CVoiceMgr::InheritAcrossLoad`**, called when `load_game` promotes the candidate:
  - it copies the on/off state, the random generator and the group choices;
  - it clears the unique flags;
  - it leaves the next play time and the voice handle to the restored session, which starts with
    none.

  NEW_LEVEL's `InheritHistory` is unchanged (`StartNewWorld` keeps the unique flags).
- **CommLink.** The candidate client's panel is idle; a running conversation is lost (its
  ConversationTrigger belongs to lot 7). The original's leftover panel, drawn over the restored
  world, is not reproduced.
- **CTF.** `save_game` refuses a CTF game type, a team flag in `g_ctf`, a live FlagObject or
  FlagStand, and a player on a team or carrying a flag. The refusal reads
  `CTF games cannot be saved (multiplayer, out of scope): <reason>` and leaves the store
  unchanged. `load_game` refuses a CTF console at admission (`CTF saves cannot be loaded`).
  Other multiplayer types keep the D12 message.

## Tests

`tests/save_native_tests.cpp` (`save_native.*`, labels headless, objects, save) and
`tests/save_native_gpu_tests.cpp` (`gpu.save_native`, labels gpu, private, save):

| Case | Checks |
| --- | --- |
| `declarations` | Every engine-implemented class has its original support (BaseClass NotSaved, seven AlwaysLoad, payload version 0); WorldModel, FogSphere, World and Brush are unregistered; FlagObject and FlagStand are NotSaved |
| `polygrid` | A synthetic world with an authored PolyGrid: `SaveObjects` skips it as always-load; three save/load cycles each recreate exactly one published PolyGrid, with no failure, which survives 10 steps. Injected duplicates (a second PolyGrid instance, a doubly registered sky object, an unregistered slot) are each refused by name. An image whose record is PolyGrid, Sound, SkyPointer or Model is refused by name at the fingerprint stage, and the source keeps its surface |
| `voices` | Source unique flags, a disabled manager, a group choice and a 100 s cooldown: after the load the flags are clear, the next play time is 0 with no voice, the on/off state and group choice are kept, the CommLink is idle; a new cooldown is measured against the restored clock |
| `ctf` | A player on a CTF team and the CTF game type are refused naming CTF, Bloodbath keeps the D12 message, the store is unchanged; a CTF console is refused at admission |
| `private` | All 58 mounted worlds with the local player: the refusal of each save (table below); every CTF map refused naming CTF; whole worlds loaded twice with byte-identical re-saves after the first load and 30 clean steps; every world with a sky or a PolyGrid loads an image of all its Original objects and must return the same sky registrations, the same number of PolyGrids and every volume surface once, before and after 10 steps |
| `gpu.save_native` | 02_CityHub and 13_Downtown before a save and after loading an image of every Original object, with the original and the enhanced (Standard) renderer: the same sky draws (5 each), the same PolyGrid entries (2 and 15) and draws, no PolyGrid presented twice |

`save_session.failures` now expects the CTF admission message for a CTF console, and the D12
message for Bloodbath.

## Real worlds

58 of 58 worlds enter with the player. `drop` saves whole and restores twice. The four CTF maps are
refused naming CTF; their image test clears the inspection player's team. 53 worlds republish
their sky and PolyGrids with no duplicate. Sky/PolyGrid is restored sky objects / PolyGrids
(volume surfaces); "—" means the world has neither.

| World | Sky / PolyGrid | What still blocks a whole save |
| --- | --- | --- |
| 01_subwaya | — | 7: CClientExplosionSFX, CivilianAI, CultistAI, DetailSprite, Gideon, GlowingLight, RandomSoundFX |
| 02_cityhub | 5 / 2 (2) | 18: AssaultRiflePU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, Gideon, MedKitPU, ProximitiesPU, RandomSoundFX, RemotesPU, SoundFX, SubMachineGunPU, WardPU |
| 02_cityhubb | 5 / 2 (2) | 18: BoneLeech, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, MegaHealthPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShikariAI, SoulDrudge, SoundFX, SubMachineGunPU |
| 02_cityhubc | 5 / 2 (2) | 23: BatteryAmmoPU, BoneLeech, BugSprayPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, CultistAI, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FlashlightPU, Flicker4WaveLightFX, FlickerLight, HealthPU, MegaHealthPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, SoundFX, SubMachineGunPU |
| 03_museum | 5 / 7 (7) | 13: CClientParticleStreamSFX, CultistAI, Fanatic, Gideon, GlowingLight, InvulnerabilityPU, MadScientistAI, NecroWardPU, Prophet, ProximitiesPU, RandomSoundFX, ShotgunPU, SoundFX |
| 04_steamtunnels | 0 / 7 (7) | 15: CClientParticleStreamSFX, CivilianAI, DetailSprite, DrudgeLord, Fanatic, FlareAmmoPU, FlickerLight, HealthPU, MedKitPU, NecroWardPU, NightGogglesPU, RemotesPU, ShikariAI, SoulDrudge, SoundFX |
| 05_cdm | 5 / 5 (5) | 17: BerettaPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, DetailSprite, Fanatic, HealthPU, KeyPickup, MedKitPU, MegaHealthPU, NecroWardPU, RandomSoundFX, ShikariAI, SoulDrudge, SoundFX, StealthPU, WardPU |
| 05_cdmb | 4 / 0 (0) | 5: BMGAmmoPU, Fanatic, Prophet, SniperRiflePU, SoundFX |
| 06_airship1 | 4 / 0 (0) | 3: CultistAI, DetailSprite, Fanatic |
| 06_airship2 | 4 / 0 (0) | 12: AngerPU, AssaultRiflePU, BugSprayPU, CultistAI, DetailSprite, Fanatic, FlareGunPU, KeyPickup, MedKitPU, Prophet, SniperRiflePU, SoundFX |
| 07_cathedral | 6 / 1 (1) | 19: AngerPU, BatteryAmmoPU, BoneLeech, BulletAmmoPU, CClientExplosionSFX, ClientLightFX, DetailSprite, DrudgeLord, Fanatic, FlareAmmoPU, KeyPickup, MegaHealthPU, NecroWardPU, NightGogglesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, SoulDrudge, VoodooDollPU |
| 08_subwayb | — | 5: CClientExplosionSFX, CivilianAI, Fanatic, GlowingLight, RandomSoundFX |
| 09_abandoned_buildings | 6 / 3 (3) | 31: BMGAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, CClientWarpGateSFX, CivilianAI, DieBugDieAmmoPU, DrudgeLord, Fanatic, FlareAmmoPU, FlashlightPU, Flicker3WaveLightFX, Gideon, GlowingLight, HowitzerPU, MadScientistAI, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, Prophet, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, ShotgunPU, SoulDrudge, SoundFX, StrobeWaveLightFX, Thief, TimeBombPU, ZealotAI |
| 10_sewagetreatmentplant | 6 / 2 (2) | 12: AssaultRiflePU, BMGAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DrudgeLord, Fanatic, FlashlightPU, MedKitPU, ShellAmmoPU, ShikariAI, SoulDrudge, Thief |
| 11_slaughter_house | 4 / 6 (6) | 24: BMGAmmoPU, BoneLeech, CClientParticleStreamSFX, CivilianAI, CultistAI, DeathRayPU, DrudgeLord, Fanatic, FlareAmmoPU, FlareGunPU, HealthPU, MedKitPU, MegaHealthPU, NecroWardPU, Prophet, ProximitiesPU, RandomSoundFX, RemotesPU, SoulDrudge, SoundFX, TheEyePU, Thief, TimeBombPU, ZealotAI |
| 12_train_station | 5 / 1 (1) | 29: AngerPU, BatteryAmmoPU, BinocularsPU, BoneLeech, BulletAmmoPU, CClientExplosionSFX, CClientParticleStreamSFX, CivilianAI, ClientLightFX, CultistAI, DieBugDieAmmoPU, DrudgeLord, Fanatic, FlareAmmoPU, FlashlightPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SniperRiflePU, SoulDrudge, SoundFX, Thief, VoodooDollPU, ZealotAI |
| 12_train_stationb | 5 / 1 (1) | 10: BMGAmmoPU, BoneLeech, CClientParticleStreamSFX, Fanatic, Prophet, RemotesPU, ShellAmmoPU, SoundFX, TimeBombPU, ZealotAI |
| 13_downtown | 7 / 15 (15) | 26: BMGAmmoPU, BatteryAmmoPU, BoneLeech, BugSprayPU, BulletAmmoPU, CClientParticleStreamSFX, CivilianAI, ClientLightFX, DetailSprite, DieBugDieAmmoPU, DrudgeLord, FlareAmmoPU, FlashlightPU, HowitzerAmmoPU, NapalmCannonPU, NightGogglesPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, ShotgunPU, SniperRiflePU, SoulDrudge, TheHandAI, TimeBombPU, ZealotAI |
| 14_subwayc | — | 18: AssaultRiflePU, BatteryAmmoPU, CClientParticleStreamSFX, CivilianAI, DieBugDieAmmoPU, DrudgeLord, Fanatic, FlareAmmoPU, FuelAmmoPU, GlowingLight, NecroWardPU, Prophet, RandomSoundFX, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, Thief |
| 15_crypt | 5 / 3 (3) | 22: BMGAmmoPU, BatteryAmmoPU, Behemoth, BulletAmmoPU, DieBugDieAmmoPU, DrudgeLord, FlareAmmoPU, FlashlightPU, FuelAmmoPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, RandomSoundFX, ShellAmmoPU, ShikariAI, SoulDrudge, StealthPU, TheHandAI, Thief, VoodooDollPU |
| 16_bridge | 4 / 4 (4) | 24: AssaultRiflePU, BatteryAmmoPU, BoneLeech, BulletAmmoPU, CClientParticleStreamSFX, CultistAI, DetailSprite, DieBugDieAmmoPU, Fanatic, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, Prophet, ProximitiesPU, RemotesPU, ShellAmmoPU, ShikariAI, ShotgunPU, SniperRiflePU, SoulDrudge, SoundFX, Thief, WardPU |
| 17_entrance | 4 / 4 (4) | 10: BulletAmmoPU, DetailSprite, Fanatic, HealthPU, KeyPickup, MiniGunPU, Prophet, ShellAmmoPU, SoundFX, WardPU |
| 18_temple | 5 / 2 (2) | 18: BMGAmmoPU, BulletAmmoPU, CultistAI, DieBugDieAmmoPU, DrudgeLord, DrudgePriest, FlareAmmoPU, FlashlightPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, LifeLeechPU, MegaHealthPU, NecroWardPU, ShellAmmoPU, ShikariAI, SoulDrudge, Thief |
| 19_lobby | 4 / 4 (2) | 13: Behemoth, CultistAI, DeathShroud, DieBugDieAmmoPU, DrudgePriest, Fanatic, FlareAmmoPU, FuelAmmoPU, KeyPickup, MegaHealthPU, Prophet, SoundFX, StealthPU |
| 19_lobbyb | 4 / 0 (0) | 10: BMGAmmoPU, BatteryAmmoPU, DieBugDieAmmoPU, Fanatic, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, Prophet, ShikariAI |
| 19_lobbyc | 4 / 0 (0) | 14: AngerPU, BatteryAmmoPU, Behemoth, BulletAmmoPU, DrudgeLord, Fanatic, FuelAmmoPU, HealthPU, MegaHealthPU, NecroWardPU, ShikariAI, SoundFX, WardPU, ZealotAI |
| 20_powerstation | 0 / 1 (1) | 14: BatteryAmmoPU, BulletAmmoPU, CClientLightningSFX, CultistAI, Fanatic, HealthPU, HowitzerAmmoPU, MedKitPU, Prophet, ShikariAI, SoundFX, SubMachineGunPU, TheHandAI, WardPU |
| 21_laboratory | 4 / 4 (4) | 20: BatteryAmmoPU, BoneLeech, CClientLightningSFX, CivilianAI, CultistAI, DieBugDieAmmoPU, Fanatic, FlareAmmoPU, HealthPU, KeyPickup, MadScientistAI, MegaHealthPU, Prophet, RandomSoundFX, ShikariAI, SingularityPU, SoulDrudge, SoundFX, TheHandAI, WardPU |
| 22_office | 5 / 11 (11) | 15: BatteryAmmoPU, BoneLeech, CultistAI, DeathShroud, DetailSprite, Fanatic, FuelAmmoPU, HealthPU, HowitzerAmmoPU, KeyPickup, MedKitPU, MegaHealthPU, NecroWardPU, Prophet, ShikariAI |
| 23_rooftop | 4 / 0 (0) | 16: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, CalebAI, DieBugDieAmmoPU, EnhancedGideon, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, LifeLeechPU, MegaHealthPU, OpheliaAI, ShellAmmoPU |
| 24_ancientcity | 6 / 3 (3) | 24: BatteryAmmoPU, Behemoth, BulletAmmoPU, DeathShroud, DetailSprite, DrudgeLord, DrudgePriest, EnhancedGideon, Fanatic, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, ShikariAI, SoulDrudge, UndeadGideon, WardPU |
| 24b_arena | 6 / 0 (0) | 22: AngerPU, BatteryAmmoPU, BulletAmmoPU, CClientExplosionSFX, CClientLightningSFX, CClientWarpGateSFX, DieBugDieAmmoPU, DrudgeLord, FuelAmmoPU, GabriellaREV, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, IshmaelREV, MegaHealthPU, OpheliaREV, ProximitiesPU, RemotesPU, ShellAmmoPU, SoulDrudge, StealthPU, VoodooDollPU |
| 25_ancientone | 6 / 2 (2) | 15: AncientOne, AncientOneTentacle, AngerPU, AssaultRiflePU, CClientExplosionSFX, CClientLightningSFX, CalebAI, GabriellaAI, HealthPU, InvulnerabilityPU, IshmaelAI, LifeLeechPU, MegaHealthPU, NecroWardPU, OpheliaAI |
| credits | 4 / 0 (0) | 3: CalebAI, DetailSprite, FlickerLight |
| drop | — | none (saved whole, restored twice) |
| end | 5 / 0 (0) | 5: CalebAI, GabriellaAI, IshmaelAI, OpheliaAI, RandomSoundFX |
| multi/bb_area15 | 4 / 0 (0) | 16: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_bloodarena | 4 / 0 (0) | 20: AngerPU, BMGAmmoPU, BatteryAmmoPU, BinocularsPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU, TheEyePU, WardPU |
| multi/bb_bloodkeep | — | 18: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX, TheEyePU, WardPU |
| multi/bb_bodies | 4 / 2 (2) | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, WardPU |
| multi/bb_bridge | 4 / 0 (0) | 16: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DetailSprite, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX, WardPU |
| multi/bb_brownstar | 4 / 0 (0) | 17: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, TheEyePU, WardPU |
| multi/bb_cabana | 4 / 0 (0) | 17: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, RevenantPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_chosenarena | 6 / 0 (0) | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU |
| multi/bb_condemned | 5 / 0 (0) | 21: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, StealthPU, TheEyePU, WardPU |
| multi/bb_crypt | 5 / 1 (1) | 14: BatteryAmmoPU, BulletAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, ProximitiesPU, RandomSoundFX, ShellAmmoPU, SoundFX, StealthPU, WardPU |
| multi/bb_deathcanyon | 4 / 0 (0) | 11: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU |
| multi/bb_drain | 4 / 1 (1) | 14: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU, StealthPU, WardPU |
| multi/bb_dustbowl | 4 / 1 (1) | 11: BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, ProximitiesPU, ShellAmmoPU, SoundFX |
| multi/bb_fields | 4 / 0 (0) | 10: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HowitzerAmmoPU, ProximitiesPU, RemotesPU, ShellAmmoPU |
| multi/bb_jgrave | 4 / 2 (2) | 20: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, NightGogglesPU, ProximitiesPU, RandomSoundFX, RemotesPU, ShellAmmoPU, TheEyePU, TimeBombPU, WardPU |
| multi/bb_midgard | 4 / 0 (0) | 13: BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MedKitPU, MegaHealthPU, NecroWardPU, ProximitiesPU, ShellAmmoPU |
| multi/bb_power | 0 / 1 (1) | 15: AngerPU, BMGAmmoPU, BatteryAmmoPU, BulletAmmoPU, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, MegaHealthPU, NecroWardPU, ProximitiesPU, RemotesPU, ShellAmmoPU, SoundFX |
| multi/bb_rooftop | 4 / 0 (0) | 15: AngerPU, BatteryAmmoPU, BulletAmmoPU, CClientParticleStreamSFX, DieBugDieAmmoPU, FlareAmmoPU, FuelAmmoPU, HealthPU, HowitzerAmmoPU, InvulnerabilityPU, MegaHealthPU, ProximitiesPU, RemotesPU, ShellAmmoPU, StealthPU |
| multi/ctf_cabana | 4 / 0 (0) | CTF games cannot be saved (multiplayer, out of scope): a team flag is in the world |
| multi/ctf_deadcities | 6 / 0 (0) | CTF games cannot be saved (multiplayer, out of scope): a team flag is in the world |
| multi/ctf_dmx | 4 / 2 (2) | CTF games cannot be saved (multiplayer, out of scope): a team flag is in the world |
| multi/ctf_jam | 6 / 1 (1) | CTF games cannot be saved (multiplayer, out of scope): a team flag is in the world |

## Execution evidence

macOS (Apple clang 21, Apple M5, Metal), on `cfe2bf58`, then rebased onto lots 6 and 7 (`2ff31aec`),
where the save tests, `save_native.private`, `gpu.save_native` and `gpu.surface` pass again (79 of 79; the
blocking table above is from this rebased build):

- **Dev** (`dev` preset, private data and GPU tests configured): the full headless label runs
  1,274 tests (five new `save_native.*`); 1,267 pass. The seven failures are the pre-existing
  private-data content checks (doors.catalog, pickups.catalog, ai_recovery.catalog,
  ai_escape.catalog, assault.ai, mad_scientist.wounds, lightning.catalog). `save_native.private`
  takes 22 s. `gpu.save_native` passes on Metal (1.2 s), and `gpu.surface` still passes.
  After the rebase onto lots 6 and 7: 1,287 of 1,294, with the same seven failures.
- **Release** (inspector-free `release` preset): builds; its headless run gives the same 1,267
  of 1,274 with the same seven failures.
- **Clang ASan/UBSan** (`testing.md` recipe, `ASAN_OPTIONS=detect_leaks=0` because Apple clang
  has no LeakSanitizer): no test output holds a sanitizer report. The full headless run shared
  the machine with other sessions (load average 60–110). 52 private or long tests exceeded their
  fixed wall-clock limits; texture.mapping and model_view.builtin_resources failed under that load
  and pass when rerun. After the last code change, every save test (including `save_native.private`,
  `save_session.private`, `save_movers.private` and `save_player.*`) and every object-contract test
  passes under the sanitizers (70 of 71). `objects.message_churn` exceeded its 10 s wall limit at
  load average 111, as already recorded for lot 8.
- The launcher's post-build `install_name_tool` step reports a missing Homebrew rpath in these
  worktree build directories. It is unrelated to this lot, and the game and tests build.

## For later lots

- Slots and menus (lot 6): a CTF game never reaches a slot, because `save_game` refuses it naming
  CTF; the load's promotion now also carries the VoiceMgr values.
- Lot 10: owner-bound effects republished by restored owners can extend
  `check_restored_publications` in the same way (particle streams, warp gates, lights).
- Lot 14: run `save_native.private` and `gpu.save_native` in the certification; the blocking
  table above should shrink to nothing.

## Limits

- PolyGrid wave phases and particle state restart; the original client restarted them too.
- The original's leftover CommLink panel after a load is not reproduced.
- The GPU check compares presentation counts, not pixels: the image omits the classes of later
  lots, so the restored frame legitimately differs.
