# Nightmares expansion plan

Status (2026-10-26): decision 1 is **superset numbering** (user, 2026-10-26). Lots **N1** (the
edition, mount order, `IsAddonBuild()`, save edition and numbering) and **N2** (NIGHTMARES row and
new game, `WORLDS_AO` paths, add-on art, CREDITS_AO and the END roll, inspection of all 13
worlds) are implemented ([edition contracts](../contracts/nightmares_edition_contracts.md)).
Lot **N8** (robe Cultists) is implemented too, and the NaN-row proposal below was applied
(user, 2026-10-26): CALEB1_CC_C and CALEB2_CC_C start. OPHELIA_CC_O's DTX DetailSprite is now an
empty sprite as the reference engine reads it. Lot **N6** (GremlinAI, rocks) is implemented:
OPHELIA_CC_O now waits only for N5 (FlayerPU) and GABBY_CC_G for N4 (CombatShotgunPU). The
headless-corpse proposal awaits a decision.
`blood2nm.rez` stays refused as a mod. Every later lot needs the user's go-ahead (see
[open decisions](#open-decisions)).

Blood II: The Nightmare Levels ("Nightmares", version 1.0.233) is Monolith's official
expansion. It is not a data mod: the original ships a second build of the same game code,
compiled with `_ADD_ON` and `_ADDON` (the `Release AddOn` configurations of
`ObjectDLL.dsp`, `ClientShellDLL.dsp` and `ClientRes.dsp` define both macros). Its archive
replaces `OBJECT.LTO` and `CSHELL.DLL` for the whole session, including The Chosen campaign.

Evidence: the reference tree `lithtech/Blood2` (53 files, 144 conditional blocks), the
installed `BLOOD2NM.REZ` (listed and its DATs parsed read-only; nothing executed), its string
literals compared with the retail `OBJECT.LTO`/`CSHELL.DLL`, `NGHTMARE.TXT` and the
installed launcher `BLOOD2.EXE`. The source's `version_string` is `"NIGHTMARES v1.0.233"`,
the same build as the installed readme, and every add-on class name and resource path found
in the source also appears in the shipped binaries.

## Installed data

| File | Size | Notes |
| --- | --- | --- |
| `BLOOD2NM.REZ` | 108,939,094 B, SHA-256 `1cea55af…` | 1,047 entries, all under `*_AO/` except four root files |
| `B2NMSRV.EXE` | | Dedicated server, `_ADDON` build of `Blood2Serv` |
| `NGHTMARE.TXT` | | Readme: "all save game files from previous versions will be incompatible" |
| `BLOOD2NM2.REZ` | absent | Listed by the launcher and the server; optional (`ADDREZ`-style), not shipped here |

REZ content: `SOUNDS_AO` 435, `TEXTURES_AO` 249, `SKINS_AO` 161, `MODELS_AO` 117,
`SPRITETEXTURES_AO` 33, `INTERFACE_AO` 26, `WORLDS_AO` 15 (13 DATs), `SPRITES_AO` 5,
`SCREENS_AO` 2, and root `OBJECT.LTO` (970,240 B), `CSHELL.DLL` (841,216 B), `CRES.DLL` and
`SRES.DLL` (112,640 B each, identical). No retail path is overridden except those four root files.
The `CRES.DLL` string table (790 strings) is **identical** to the retail/patch-2 `CRES.DLL`:
`IDS_MENU_SINGLEPLAYER_NIGHTMARES` (549, "NIGHTMARES"), weapon names 370/371, Soccer/Toe Tag
(127/128) and the goal strings (7017–7020) already exist in retail. Only non-string resources
differ (not examined). The music uses the installed `Music/` folders (Bloat, Cabal, A1, Track1);
there is no add-on music.

## How the original starts it

- **Launcher** (`BLOOD2.EXE`): a "Launch Nightmares" checkbox (readme 5.1, "leave this box
  checked to enable Nightmare gameplay") appends `Blood2Nm.rez` and `Blood2Nm2.rez` to the
  `-rez` list passed to `CLIENT.EXE`, with a Nightmares CD check.
- **Mount order** (`Blood2Serv/CoolServ.cpp` `_ADDON`, the launcher's string table agrees):
  `BLOOD2.REZ`, `SOUND.REZ`, `custom`, `BLOOD2P`…`P9`, `T`, `L`, **`Blood2Nm.rez`,
  `Blood2Nm2.rez`**, then user REZ files. The add-on's `OBJECT.LTO`/`CSHELL.DLL`/`CRES.DLL`
  therefore win over every patch, and user mods still mount after it.
- **Separate identity**: `BLOOD2GUID`/`GAMEGUID` change (`…18a942` vs `…19a842`), the
  version string becomes `NIGHTMARES v1.0.233`, the server uses `NET_GAME_AO` and
  `IDS_*_AO` strings. Old saves are refused by the readme's own statement.
- **Both campaigns stay playable**: Single Player keeps its retail rows and gains a
  `NIGHTMARES` row (`MenuSinglePlayer.cpp`). The Chosen therefore runs under the add-on build
  too, and the add-on branches below that are not world-gated also apply to it.

### Single-player flow

1. `NIGHTMARES` → `MENU_ID_DIFFICULTY` with `SetNightmaresMode(TRUE)` →
   `MenuNewNightmaresGame(difficulty)`: clear `SLOT_CURRENT`, first world
   `worlds_ao\caleb1_cc_c`, character Caleb, `StartNewGame(…, GAMETYPE_SINGLE,
   LOADTYPE_NEW_GAME, difficulty)` (also `maxfps 0`, irrelevant here).
2. World names containing `WORLDS_AO` are used as given (no `Worlds\` prefix, `.dat` added
   on the client), set `m_bAddonLevel` on the server, and use `Screens_ao\Bumper.pcx` as the
   load screen.
3. Authored `ExitTrigger` chain (all with `UnlockKeyName Key`):

   | World | Title (WorldProperties) | Player | Next / start point |
   | --- | --- | --- | --- |
   | `CALEB1_CC_C` | Cold, Cold Grave | Caleb | `ophelia_cc_o` / `ostart` |
   | `OPHELIA_CC_O` | — | Ophelia | `ishmael_cc_i` / `istart` |
   | `ISHMAEL_CC_I` | JoJo's Big Adventure | Ishmael | `gabby_cc_g` / `gstart` |
   | `GABBY_CC_G` | — | Gabriella | `caleb2_cc_c` / `cstart` |
   | `CALEB2_CC_C` | — | Caleb | `endboss_cc_c` / `estart` |
   | `ENDBOSS_CC_C` | — | Caleb (inventory kept) | `End` / `start` |
   | `END` | (credits world) | — | rolls credits |

4. **Character swap** on each `_AO` level change: the suffix `_CC_C/_CC_I/_CC_O/_CC_G` selects
   the character. Server: `DoLevelChangeCharacterCheck` (called from `LoadWorld`, returns
   immediately without `_ADDON`) sets `g_bLevelChangeCharacter`; the next `CPlayerObj::Save`
   (the keep-alive save) calls `TweakSaveValuesForCharacterChange`: new `m_nCharacter`,
   that character's 5/3/3/1-style attributes, `ValidateAttributes`, `ClearPowerupValues`,
   `m_damage.SetStartingCharacterValues()`. Client: `SetCharacterInfo(new, NORMAL)` and
   `g_bPlayerInvReset`, which sends `CMSG_RESETPLAYERINVENTORY` on the next update (empty
   inventory). Exceptions: a restore/auto-save load never resets, and `ENDBOSS_CC_C` neither
   swaps nor resets (Caleb keeps his Caleb2 inventory).
5. `WORLDS_AO\END` (and `WORLDS_AO\CREDITS`, absent from the REZ) set `m_bRollCredits`;
   the credits text is `CREDITS_AO`/`INTRO_AO` instead of `CREDITS`/`INTRO`.
6. Presentation: title splash `screens_ao\title.pcx`, menu background
   `interface_ao\background.pcx`; the GTI movie goes straight to `MOVIE_FINISHED` (no
   `Intro.smk` — movies are not played by OpenBlood2 yet anyway).

## `_ADD_ON`/`_ADDON` branch inventory

"Chosen too" means the branch also changes The Chosen when the add-on build runs it.

| Area | Source | Effect | Chosen too | Lot |
| --- | --- | --- | --- | --- |
| Weapon IDs | `SharedDefs.h` | `WEAP_COMBATSHOTGUN 18`, `WEAP_FLAYER 19`, `WEAP_LASTPLAYERWEAPON 19` (17); every creature weapon shifts by +2 (`LASTPLAYERWEAPON + n`); `WEAP_GREMLIN_ROCK`/`WEAP_NIGHTMARE_FIREBALLS` = +32/+33; inventory bombs shift by +4 | yes (numbering) | N1 |
| Cheats | `SharedDefs.h`, `PlayerObj::ProcessCheat` | `CHEAT_COMBATSHOTGUN`/`CHEAT_FLAYER` after `CHEAT_KNIFE`; `CHEAT_LASTWEAPON = CHEAT_FLAYER` (weapon cheat = code − first + 1, so KNIFE→17, COMBAT→18, FLAYER→19); later codes shift | yes | N4 |
| Characters | `SharedDefs.h`, `ConfigMgr`, `MenuCharacter`, `PlayerObj` tables, `BloodClientShell` | `CHARACTER_M_CULTIST..PROPHET` 4–7, `MAX_CHARACTERS 8`, their models/skins/heights/attributes, menu entries (28 rows), multiplayer team skins, frag icons, view-hand letters, `VMC_*` voice groups 4–7 | multiplayer only | N10 |
| World paths | `BloodServerShell::LoadWorld`, client world/load-screen code | `WORLDS_AO` names unprefixed, `m_bAddonLevel`, bumper screen | — | N2 |
| Campaign | `MenuSinglePlayer`, `MenuDifficulty`, `MenuNewNightmaresGame`, `MenuCustomLevel` | NIGHTMARES row and start; custom-level list adds `Worlds_ao`, `Worlds_ao\Multi_ao` | — | N2 |
| Character swap | `DoLevelChangeCharacterCheck`, `CPlayerObj::Save`, `TweakSaveValuesForCharacterChange`, client `ResetPlayerInventory` | see flow above | — (`_AO` names only) | N3 |
| Behemoth | `behemoth.cpp` | 1000 HP (instead of 5000) in add-on levels | no (`IsAddon`) | N3 |
| Combat Shotgun | `GameWeapons.*` (server/client), `WeaponDefs.cpp`, `InventoryMgr`, `WeaponPickups`, `PickupObject`, `MessageMgr`, `ConfigMgr` | 16-shell primary, `CGasGrenade` alternate on `AMMO_DIEBUGDIE`, `CombatShotgunPU` (Models_ao/Skins_ao powerup path branch), cheat, names | yes (pickup, AI `COMBAT`) | N4 |
| Gas grenade | `GameProjectiles` `CGasGrenade`, `ExplosionFX` `EXP_GAS_GRENADE` (×3 detail), `ObjectFX` `OBJFX_GREEN_SMOKE` | bouncing smoke grenade, 200-unit 3-damage ticks every .5 s with the wonky view | yes | N4 |
| Flayer | `CFlayer`, `FlayerPU`, `CFlayerChain`, `CFlayerPortal`, `SFX_FLAYER_CHAIN_ID` + client `IKChainFX`, `EXP_FLAYER_PRIMARY/ALT/RETRACT/SHATTER`, `OBJFX_BLOOD_TRAIL_1`, `FileCaching` | melee/focus chain hook that tracks, damages, attaches a gib and retracts; alternate portal searches 800 units for 20 s and launches a chain every 2 s | yes (pickup, AI `FLAYER`) | N5 |
| AI weapon strings | `AI_Mgr::WeaponStrToInt` | `COMBAT`, `FLAYER`, `ROCK`, `NIGHTMARE_FIREBALL` prefixes | yes | N4–N7 |
| Gremlin | `Gremlin.cpp`, `CGremlinRock`, `CRockProjectile` | `GremlinAI` (jump, dodge, guard, special, close/far), random 500–1000 rock velocity with an up bias | — | N6 |
| Nightmare | `nightmare.{h,cpp}`, `CNightmareFireballs`, `CNightmareFireball`, AI_Mgr | boss (bite/fireballs/shockwave, `jaw`-node aim, health creation), always sees the player (`CanSee` short circuit for `Nightmare`) | — | N7 |
| Robe Cultist | `CultistAI` | `RobeSkin` keeps the `oldschool` model with its own shared `CAnim_Sound`, `sounds_ao\enemies\RetroCultist` and cache list (retail build clears the flag and forces male); clown-skin detection from skin names on Load | yes | N8 |
| Sorority Civilian | `CivilianAI` | `sounds_ao\enemies\femalemisc` cache for the sorority skin | yes | N8 |
| Orb | `ViewWeapon`, `Weapon` | `generic_orb_pv_t.dtx` for enemy characters (multiplayer) | MP | N10 |
| Soccer / Toe Tag | `SoccerObjects`, `PlayerObj::SoccerGoal`, `MID_GOAL`, `SMSG_SOCCERGOAL`, `SMSG_TRAPPED`, `NetStart`, `NetInfo`, cheer/boo sounds | `NGT_SOCCER` games on `SOC_*` maps; `SoccerBall`/`SoccerGoal` exist in the retail LTO too | MP | N10 |
| Server/net | `Blood2Serv`, `NetStart` | GUIDs, `_AO` strings and dialogs, `Multi_ao` level lists | MP | N10 |

## Selecting it at runtime

Follow the demo precedent: the original's compile-time `_DEMO` is already a runtime
`IsDemoBuild()` (`B2BaseClass.h`, through `ServerDE::DemoBuild`). Proposal:

- **Edition** `Nightmares` beside `Retail` and `Demo` (`app/game_setup`, `setup.cfg`
  `selected.edition=nightmares`, which bumps the strict parser's accepted values). An
  installation folder holding `BLOOD2.REZ` and `BLOOD2NM.REZ` lists both Retail and Nightmares
  entries; choosing one is choosing an entry, as for the demo. `--nightmares` selects it on
  the command line; `Config::nightmares` is never inferred from directory contents.
- **Mounting**: the edition's stock set is the retail set followed by `BLOOD2NM.REZ` and an
  optional `BLOOD2NM2.REZ`, then mods. The archive is accepted only as an edition base, never
  through `--rez`/mods (the current refusal stays). Its `OBJECT.LTO`, `CSHELL.DLL` and
  `SRES.DLL` are inventoried as present but never loaded or executed; its `CRES.DLL` is read
  by the existing PE resource reader (identical strings, so retail text is unchanged). The
  inspector's "Expansion excluded" row becomes "Expansion edition" when mounted.
- **Code switch**: one runtime `IsAddonBuild()` through the server adapter (and the client
  shell), set only by the edition. Every ported `#ifdef _ADD_ON/_ADDON` becomes
  `if (IsAddonBuild())`, so the retail and demo paths stay byte-identical. World-gated
  branches additionally test the original `IsAddon()` (`WORLDS_AO` in the current world name).
- **Saves**: a new `SaveEdition::Nightmares` in the fingerprint and `saves/nightmares/`,
  so retail and add-on saves never load across editions (the readme's own incompatibility).
- **Weapon numbering** is the main decision (see below); whichever is chosen, retail weapon,
  cheat and save numbers must not change.

## Ported classes that change

| Class / area | Change under the add-on edition |
| --- | --- |
| `Resources::mount_game`, `inspect_data`, setup screen, config | edition, mount order, code-file exception for the edition base only |
| `WeaponDefs`, `InventoryMgr`, `CWeapon` factory, client weapon factory, status bar | two player weapons, two creature weapons, numbering/ranges (`<= WEAP_LASTPLAYERWEAPON`, `WEAP_BASEINVENTORY`, `WEAP_MAXWEAPONTYPES`) |
| `AI_Mgr::WeaponStrToInt` (`AIRuntime.cpp`) | four prefixes; the documented "authored COMBAT becomes Beretta" (fanatic/prophet contract) is retail-only behaviour |
| `AI_Mgr::CanSee`, aim origin | Nightmare always-sees branch, `jaw` node for its two weapons |
| `CultistAI` (`AIRuntime.cpp`, `CultistAI.cpp`) | robe Cultist initialization and Load (currently an explicit refusal, used by all 47 Caleb-level Cultists) |
| `CivilianAI` | sorority sound cache (skins already resolved) |
| `Behemoth` | 1000 HP in `WORLDS_AO` levels (spawned in Caleb2, Ophelia, Ishmael) |
| `PickupObject` / `WeaponPickups` | `_ao` powerup model path for the two new weapons |
| `CPlayerObj` Save/keep-alive, `CMSG_RESETPLAYERINVENTORY`, client shell | character swap and inventory reset; NEW_LEVEL capture/restore must apply the tweak before the destination entry |
| Cheats page / `CMSG_CHEAT` | add-on weapon cheats and the shifted range |
| Menus, loading screen, credits, title | NIGHTMARES row, `_AO` art, bumper, `CREDITS_AO` |
| `ExplosionFX`, `ObjectFX`, `SFXMgr` client services | four Flayer explosions, gas explosion, green smoke, blood trail 1, IK chain |
| `class_coverage.md`, `world_preparation.cpp` | the nine "unported add-on registrations" become edition-gated registrations; authored retail `Gremlin` stays absent (the add-on class is `GremlinAI`) |

## World classes

Parsed from the 13 DATs (object lists only). Editor-only `Light`, `DirLight`, `ObjectLight`,
`GlobalDirLight` and `OutsideDef` omitted. Every other placed or spawned class is already
registered, except:

| World | Objects | New placed classes | Notable existing classes / spawns |
| --- | --- | --- | --- |
| `CALEB1_CC_C` | 435 | — | 19 robe `CultistAI`, 7 SoulDrudge, 6 DrudgeLord, 3 BoneLeech, 3 TheHand, 2 Shikari, Water |
| `OPHELIA_CC_O` | 1264 | `GremlinAI` ×9, `FlayerPU` | 17 SoulDrudge, 12 BoneLeech, 5 Thief, 8 sorority `CivilianAI`; spawns Behemoth, CalebAI, OpheliaAI, ZealotAI |
| `ISHMAEL_CC_I` | 1331 | `FlayerPU` | 35 clown `CultistAI`, 6 BoneLeech, 10 `Model`; spawns 15 Shikari, 4 Behemoth, DrudgePriest, DrudgeLord, CalebAI |
| `GABBY_CC_G` | 971 | `GremlinAI` ×12, `CombatShotgunPU` | 7 Shikari, 6 SoulDrudge, 6 TheHand, 4 BoneLeech; spawns CalebAI, ZealotAI |
| `CALEB2_CC_C` | 502 | — | 28 robe `CultistAI`, 3 DeathShroud, IshmaelREV, Zealot; spawns Behemoth, DrudgePriest, GabriellaREV, OpheliaREV, Shikari, Zealot |
| `ENDBOSS_CC_C` | 129 | `Nightmare` (`fakemare`, 2800+400 HP, PASSIVE, STRONG) | 5 `CClientLightningSFX`; spawns a second `Nightmare`, DrudgeLord, Shikari |
| `END` | 27 | — | CalebAI, GabriellaAI, IshmaelAI, OpheliaAI, CameraObj (credits) |
| `BB_TECHNO`, `BB_SEWERS`, `BB_DUNGEON`, `BB_GRAYSKULL` | 1113/650/150/403 | — | Bloodbath maps, existing classes only |
| `SOC_KICKTHEHEAD1/2` | 126/37 | `SoccerBall`, `SoccerGoal` ×2 | soccer maps |

New property: `AIBrain` (Nightmare, value `STRONG`). Spawner strings `models\default.abc`
and `default.abc` (Caleb1, Gabby, Ishmael, Ophelia) are authored mistakes and must keep the
existing absent-class reporting. These counts are a static parse, not a `--inspect-world`
run: the inspection pass is the first test of lot N1.

## Resource gaps

Of 541 distinct file references in the DAT properties, 307 resolve in `BLOOD2NM.REZ`, 147 in
`BLOOD2.REZ` and 45 in `SOUND.REZ`. Unresolved exact paths (to be recorded as authored
missing references, never aliased):

- 19 `sounds_ao\ambient\OPHOLnn.wav` (Ophelia `RandomSoundFX`); the files exist as
  `SOUNDS_AO/CHOSEN/OPHELIA/OPHOLnn.WAV`.
- `sounds_ao\ambient\campfireamb.wav`, `sounds_ao\ambient\stallo.wav`,
  `sound\chosen\caleb\msccaleb1.wav` (`sound`, not `sounds`), `sounds\thunder.wav`
  (three `ObjectivesTrigger`s), `sounds\ambient\{fire2,whistlywindloop,indoorlightning1–5}.wav`.
- Prefix-relative names (`1.wav`…`7.wav` in Gabby/End `RandomSoundFX`, `key.pcx`/`key_h.pcx`
  and `/interface_ao/key.pcx` in `KeyPickup` icons) must be checked against the existing
  RandomSoundFX/KeyPickup resolution before they are called missing.
- Code-referenced paths all exist except three precache-only entries the original never loads
  either: `Skins_ao\Weapons_ao\Flayer_pv_t.dtx`, `Sounds_ao\Weapons_ao\flayer\{fire,alt}.wav`.
- `WORLDS_AO\CREDITS` (code) and `BLOOD2NM2.REZ` (launcher/server) are absent.

All new characters, weapons, enemies, sounds and screens referenced by the add-on code are
present: Gremlin (35 sounds), Nightmare (11), RetroCultist (26), Prophet, cultist, Soul Drudge
and female-misc groups, `oldschool.abc`, both weapons' view/powerup models and skins, the
chain hook, rock, `greenrift.spr`/`greensmoke.spr`, title, bumper and menu backgrounds.

## Lots

Each lot is one commit series with its contract, report and tests, gated by the user. Retail
and demo regressions (unchanged frames, saves, cheat codes and audits) are re-run in every lot.

| Lot | Scope | Tests |
| --- | --- | --- |
| **N1 Edition and mount** | `Edition::Nightmares`, `--nightmares`, setup detection, mount order, edition-base code exception, `IsAddonBuild`, save edition, inspector rows; agreed weapon-numbering scheme in `WeaponDefs` without new weapons | synthetic REZ mount order/override and still-refused mod path; `setup.cfg` round trip and refusal of unknown editions; fingerprint separation; retail numbering unchanged; real data (done): `--nightmares --inspect-world` of 01_SubwayA identical to retail apart from the extra mount (8,489 = 7,446 + 1,047 − 4 overridden root files) |
| **N2 Frontend and world paths** | NIGHTMARES row, difficulty mode, `MenuNewNightmaresGame`, unprefixed `WORLDS_AO` loads, bumper, `_AO` title/background, `CREDITS_AO`, END credits roll, custom-level list | `--inspect-world` for all 13 `WORLDS_AO` DATs (needs the unprefixed path branch) listing exactly the new classes above as unsupported; menu traversal smoke to `CALEB1_CC_C` entry; loading-screen/credits unit tests; retail menu snapshots unchanged |
| **N3 Character chain** | `DoLevelChangeCharacterCheck`, keep-alive tweak, `CMSG_RESETPLAYERINVENTORY`, restore/ENDBOSS exceptions, Behemoth `IsAddon` HP | synthetic NEW_LEVEL chain (attributes, empty inventory, ENDBOSS keeps inventory, a save load does not reset); real-data exit relay Caleb1→Ophelia |
| **N8 Robe/sorority enemies** (before N4 so Caleb1/2 start) | robe Cultist model, shared table, RetroCultist sounds, Load; sorority cache | table/sound-root tests; Caleb1 and Caleb2 entry + 30 s at three difficulties |
| **N4 Combat Shotgun** | weapon, gas grenade + wonky damage, three detail recipes, green smoke, pickup, cheat, AI `COMBAT`, status icon | weapon-data and ammo tests, grenade timing/damage, pickup acknowledgement, AI string mapping per edition (retail keeps Beretta), effect recipes |
| **N5 Flayer** | weapon, chain/portal projectiles, IK chain client effect, four explosions, blood trail | chain track/attach/retract lifetime, portal search/interval, link-broken cleanup, effect recipes |
| **N6 Gremlin** | `GremlinAI`, rock weapon/projectile | state table, dodge/jump, rock velocity bias, Gabby/Ophelia entry + 30 s |
| **N7 Nightmare** | boss, fireballs, always-sees and `jaw` aim branches, `AIBrain` | state/attack tests, tracked fireball lifetime, EndBoss entry and fight |
| **N9 Campaign certification** | Save/Load bodies for every new class, save support declarations, `save_rewrites.h` entries | all seven solo worlds open at 0–2, 30 s simulation, whole-world save/load at entry and after combat, scripted chain Caleb1 → End |
| N10 Multiplayer (deferred) | add-on player characters, Soccer/Toe Tag, `Multi_ao` lists | none until network play exists; `SoccerBall`/`SoccerGoal` stay explicit absent registrations |

N8 is listed separately because both Caleb levels cannot start until the robe refusal is
replaced, while N4–N7 are each confined to one or two worlds.

## Open decisions

1. **Weapon numbering** — decided: (a), implemented in N1. (a) *Superset*: keep every retail number and append the four add-on
   weapons (and their cheats) after the current table, replacing the original's range tests
   with explicit predicates; retail saves and tests stay untouched, but add-on numbers differ
   from the 1999 build (they never leave the process except in our own saves). (b) *Per
   edition*: reproduce the add-on numbering (+2/+4 shifts) when the edition is active; closest
   to the binary, but every `constexpr` weapon number becomes runtime state. Recommendation: (a).
2. **The Chosen under the add-on edition**: run it with the add-on branches (original
   behaviour, e.g. authored `COMBAT` becomes the Combat Shotgun and robe Cultists keep their
   robes) or keep Chosen always on the retail edition. Recommendation: original behaviour.
3. Whether to accept the absent `BLOOD2NM2.REZ` silently (recommended, as the original does)
   and whether `--nightmares` is wanted beside the setup screen entry.
4. Multiplayer content (N10) stays out of scope until network play is requested.

## Proposals after N8 (2026-10-26)

**NaN-positioned rows** (x = NaN in the DAT, created there by the original) — applied 2026-10-26:

- CALEB1_CC_C `object1` is an ObjectivesTrigger with `TouchActivate 0`, activated only by
  `Trigger10`'s message. Its position matters only for its touch volume (unused) and the 3D origin
  of `Sounds\Thunder.wav` (absent from every archive). Proposal: create it at the world origin
  with the touch volume never queried, recorded as a "non-finite authored Pos replaced for a
  message-only trigger" adaptation, so the "Ice Cold" objective still appears.
- OPHELIA_CC_O has eight `SoundFX18` rows (the same looping ambient, `nighttime2.wav`, 500/300
  radii); four are NaN, four are valid. A NaN listener distance is undefined in the original, and
  the four valid copies still play. Proposal: report the NaN copies as explicit omissions (like
  `fallengrills`), not failures, so the world starts.
- Any other class with a non-finite `Pos` stays a not-created failure until reviewed.

**DetailSprite with a DTX texture** — resolved 2026-10-26: the earlier "one-frame texture" premise
was wrong. The reference LithTech `LoadSprite` reads any file as an SPR, so `scorch1.dtx` reads as
zero frames; OpenBlood2 now creates it as that empty sprite (no decal drawn).

**Corpses of headless models**: let `CCorpse` accept a model without a head node with hit-sphere
localization unavailable (limb loss skipped, saved spheres zero), the same body policy as the
living actor. This also gives retail Thief, Naga, BoneLeech, DeathShroud, TheHand and Ancient One
corpses where today they vanish, so it changes retail deaths and the certified whole-world saves;
the save audit would need re-running.
