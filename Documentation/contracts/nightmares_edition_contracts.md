# Nightmares edition contracts (lots N1, N2, N6, N8)

Lot N1 of the [Nightmares plan](../implementation/nightmares_plan.md): the expansion becomes a
selectable **edition** beside retail and the demo. It mounts the expansion archive and exposes the
runtime switch the later lots port the original `_ADD_ON`/`_ADDON` branches behind. Lot N2
(below) adds the frontend and `WORLDS_AO` world paths. The character chain and the new classes
are lots N3–N9; The Chosen plays as before in this edition.

## Selection

- `--nightmares` (exclusive with `--demo`), `Config::nightmares`, and the setup screen's
  `nightmares` edition (`setup.cfg` `selected.edition`, `added.N.edition`, `mod.N.edition`).
  The edition is never inferred from the folder's content.
- A folder holding `BLOOD2.REZ` lists The Chosen; holding `BLOOD2NM.REZ` too, it also lists
  "Blood II: The Nightmare Levels" (after The Chosen). `BLOOD2NM.REZ` alone is no installation.
  A portable copy still prefers The Chosen. Mods are recorded per edition, as for the demo.
- `inspect_data` requires `BLOOD2.REZ` and `BLOOD2NM.REZ` and lists `BLOOD2NM2.REZ` as optional
  (the original launcher and server name it; it is not shipped). Retail keeps listing
  `BLOOD2NM.REZ` as information only; the inspector marks it excluded there.

## Mounting

The original add-on order (`Blood2Serv/CoolServ.cpp` `_ADDON`, matching the installed launcher):
`BLOOD2.REZ`, `SOUND.REZ`, `BLOOD2P.REZ`, `BLOOD2P2.REZ`, `BLOOD2NM.REZ`, optional
`BLOOD2NM2.REZ`, then the edition's mods. All six are base mounts; a later exact path wins, so the
expansion's `OBJECT.LTO`, `CSHELL.DLL`, `CRES.DLL` and `SRES.DLL` replace the retail entries in
the index. As with the retail archives, these code files are **indexed, never loaded or
executed**; `CRES.DLL` strings are read by the bounded PE reader (identical to the retail table).
`BLOOD2NM.REZ`/`BLOOD2NM2.REZ` given as a mod (`--rez` or setup) are refused in every edition
with "The Nightmares expansion is an edition, not a mod". A missing `BLOOD2NM.REZ` refuses
the mount ("Missing BLOOD2NM.REZ"); demo and Nightmares together are refused.

## Runtime switch

`Resources::nightmares()` carries the edition into every session copy. `ServerDE::AddonBuild`
(a pure, bounded interface service, allowed during `MID_SAVEOBJECT`) returns it, and
`IsAddonBuild()` (`B2BaseClass.h`) replaces the original compile-time `_ADD_ON`/`_ADDON`, as
`IsDemoBuild()` replaces `_DEMO`. No ported code tests it yet; retail and demo behaviour is
unchanged.

## Saves

Fingerprint edition `2` (`nightmares`, after retail `0` and demo `1`), so Nightmares and retail
saves never load across editions (the expansion readme declares them incompatible); the extra
mount is a second, independent difference. Files go to `saves/nightmares/` beside
`saves/retail/`. Saving inside a `WORLDS_AO` level is refused ("Saving in the Nightmares levels
is not supported yet", the level-entry snapshot included) until the add-on classes save (lot N9);
loading still requires a `worlds/` DAT path.

## Weapon numbering (decision 1: superset)

Every retail weapon, ammo, cheat and save number is unchanged. The four add-on weapons are
appended after the inventory bombs: `WEAP_COMBATSHOTGUN 52`, `WEAP_FLAYER 53`,
`WEAP_GREMLIN_ROCK 54`, `WEAP_NIGHTMARE_FIREBALLS 55` (the 1999 build numbers them 18, 19,
`WEAP_LASTPLAYERWEAPON + 32/+33` and shifts the others). Only the rocks have `WeaponData` (lot N6);
the other three cannot be obtained yet. The original `type <= WEAP_LASTPLAYERWEAPON` test is
`IsPlayerWeaponType` (WEAP_NONE included, as in the source; it also covers the two add-on player
weapons), used by the AI weapon-switch ammo check. Add-on cheats will be appended to the cheat
codes with their weapons (lot N4).

## Tests

`config.nightmares` (options, exclusivity, required/optional archives), `world.resource_mount`
(order, override, refused mod path, missing archive, exclusivity), `setup.state`/`setup.detect`
(round trip, detection, readiness), `save_file.fingerprint` (edition recorded and refused across
editions), `save_slots` (directory) and `weapon` (numbering and predicate). Real data
(2026-10-26, installed GOG copy): `--nightmares --inspect-world` of 01_SubwayA matches retail
apart from the extra mount (8,489 resources in 5 mounts versus 7,446 in 4: 1,047 added, four root
files overridden); `--rez BLOOD2NM.REZ` is refused. `WORLDS_AO` worlds cannot be requested
before lot N2's original path branch.

## Lot N2: frontend and world paths

- **World paths** (`world_resource_path`, `addon_world_name`). The original `_ADDON`
  `LoadWorld`/`GetWorldTitle`: in the add-on build a name containing `WORLDS_AO` (any case,
  anywhere) is used as given, only `.dat` added; every other name keeps the `Worlds\` prefix.
  Retail never treats it specially (`worlds/worlds_ao/…`, which does not exist). The request
  parser, NEW_LEVEL queue/commit, destination inspection and rehearsal and the loading screen
  all take the edition; world preparation accepts `worlds_ao/` DATs only in this edition.
- **NIGHTMARES row** (`MenuSinglePlayer.cpp` `_ADDON`): first Single Player row,
  `IDS_MENU_SINGLEPLAYER_NIGHTMARES` (549, present in every `cres.dll`) and command 105. It opens
  the difficulty page in Nightmares mode (`SetNightmaresMode`, which clears story mode; story
  mode and the Chosen character page clear it again); Back returns to Single Player. A
  difficulty emits `MenuCommand::nightmares`, which `menu_world_start` turns into
  `MenuNewNightmaresGame`: `worlds_ao\caleb1_cc_c`, Caleb, `GAMETYPE_SINGLE`. The add-on menu
  strings require 549.
- **Art.** Title splash `screens_ao\title.pcx`; every menu page's background
  `interface_ao\background.pcx` (`MainMenus`/`MenuBase`); a `WORLDS_AO` level's loading screen is
  always `Screens_ao\Bumper.pcx`, whatever the DAT names, with its title and load-screen ID
  unchanged. The movie skip is moot (movies are not played).
- **Credits.** The add-on reads `TEXT/CREDITS_AO` (33 entries, versus 87 for `CREDITS`) for the
  menu and in-world credits and the END roll; there is no `INTRO_AO` (the intro roll is not
  ported). `WORLDS_AO\END` (and the absent `WORLDS_AO\CREDITS`) roll the credits over the world.
- **Non-finite authored positions.** The DAT reader now retains a non-finite `Pos` vector for any
  class (formerly only editor lights) as `invalid_editor_position`, never exposed as a pose; the
  original creates such objects at that position. Other non-finite vectors still refuse the world.
  The session decides per reviewed class (user decision, 2026-10-26):
  - an ObjectivesTrigger with `TouchActivate` false (message only: `Trigger::ObjectTouch` ignores
    touches) is created at the world origin, recorded as "Non-finite authored Pos replaced by the
    world origin (message-only trigger adaptation)". Its position only places the unused touch box
    and the origin of its sounds (CALEB1_CC_C `object1`: `Sounds\Thunder.wav`, absent from every
    archive); a player USE ray at the origin could still activate it, as at any position;
  - a SoundFX is an explicit omission (`EntryState::InvalidAuthoredPosition`, "Omitted:
    non-finite authored position"): an ambient emitter at a NaN position has an undefined listener
    distance, and OPHELIA_CC_O keeps the four valid copies of the same `SoundFX18`;
  - any other class stays a `Failed` entry until reviewed.

Real data (2026-10-26): `nightmares.installed` inspects all 13 `WORLDS_AO` DATs. Unported
classes are exactly `FlayerPU`, `GremlinAI` (Ophelia), `FlayerPU` (Ishmael), `CombatShotgunPU`,
`GremlinAI` (Gabby), `Nightmare` (EndBoss) and `SoccerBall`/`SoccerGoal` (the two SOC maps).
Not-created entries: 19 + 28 robe Cultists (Caleb1/Caleb2, lot N8), the NaN-positioned rows
above, three chrome `CDestructableModel` (Gabby, the existing environment-map limit) and one
`DetailSprite` whose `spritetextures\scorch1.dtx` is not an SPR (Ophelia). END starts and rolls
CREDITS_AO; the menu's Nightmares new game reaches CALEB1_CC_C and is refused on its 20 failed
entries. Headless: `nightmares.paths`, `nightmares.menu`, `nightmares.strings`.

## Lot N8: robe Cultists

- **Model.** `CultistAI::PostPropRead` (unconditional in both builds): `RobeSkin` selects
  `Models_ao\Enemies_ao\oldschool.abc` with `oldschool.dtx`, forces male and draws no skin
  number. The retail `InitialUpdate` then clears the robe flag (a plain male); the add-on keeps it.
- **Shared table.** A new runtime table slot 30 (`m_Robe_Anim_Sound`, sound root
  `sounds_ao\enemies\RetroCultist`) beside the male and female ones, exchanged and reset with the
  others (`RuntimeTables` is 31 slots). The source order is preserved: a robe Cultist is male,
  so the first male-or-clown-or-robe instance builds the male table (possibly from the oldschool
  model); the robe table is built independently; robe Cultists bind it, other males the male
  table. Load derives the robe flag from an `OLDSCHOOL` skin in the add-on build only.
- **Body damage.** The oldschool model has only torso, limb and gun nodes (no head, neck or
  pelvis); the donor reads an uninitialized head position for its hit spheres. As for Thief, Naga,
  BoneLeech and the other headless models, robe Cultists use the explicit body-damage policy
  (torso reaction, no regional localization), also after a Load.
- **Sorority cache.** The add-on `CivilianAI::CacheFiles` only preloads `sounds_ao\enemies\femalemisc`
  (`femdie`, `femgig`, `fempain`), which no code plays (the sound root stays `f_civ`). OpenBlood2
  has no `SS_CACHING` preload, so this is a documented no-op; the sorority skins already resolve.
- **Known gap: no robe corpse.** `CCorpse` generates its own hit spheres and refuses a model
  without a head node, so a robe Cultist's death records "Corpse setup failed" and the original
  flow hides and removes the actor without a corpse. This already applies to every headless model
  (Thief, Naga, BoneLeech, …) in retail; see the plan's proposal.

Real data: all 19 (Caleb1) and 28 (Caleb2) robe Cultists are created. CALEB2_CC_C starts, runs
30 s with all 28 healthy on the RetroCultist table, and an awake one takes torso body damage and
dies (no corpse, no frame failure). With the position rules above, CALEB1_CC_C starts too: the
menu's Nightmares new game enters it, `object1` sits at the origin and still delivers the "Ice
Cold" objective through a trigger relay, and 30 s simulate without frame failures. OPHELIA_CC_O
omits its four NaN copies and creates its DetailSprite authored with `spritetextures\scorch1.dtx`
as the empty sprite the reference engine reads it as ([sprite contracts](sprite_contracts.md#non-spr-sprite-object-files));
its start now refuses only on 10 unported entries (9 GremlinAI, a FlayerPU; lots N5/N6).

## Lot N6: Gremlins and their rocks

- **GremlinAI** (`Gremlin.cpp`, registered with RandomHitPoints 10, AIState and hidden Dims
  19×30×24). Constructor values: hearing 2000, sensing 300, smelling 1200, seeing 700, walk/roll
  320, run/jump 640, FLAG_JUMP | FLAG_NOAMMOCOLLECT, not Cabal, 65 HP, `SHIKARI_CLAW` and `ROCK`.
  Model `Models_ao\Enemies_ao\Gremlin.abc`, shared table slot 31 (`RuntimeTables` is 32 slots) with
  sound root `sounds_ao\enemies\Gremlin`; standard hit spheres (the model has `head`/`neck`, other
  names use the predecessor fallback). Creation is refused outside the add-on build ("GremlinAI is
  an add-on class (Nightmares edition only)"), as the class exists only in BLOOD2NM's object.lto.
- **States.** Like ShikariAI with the source's differences: ComputeState has no height test
  (SIGHT ≤ .75 attacks from afar; the smell test reads the argument); AttackClose/AttackFar go
  straight to ComputeState without a target (no exception for a jump in flight); AttackFar opens
  with an `IsRandomChance(40)` Bold taunt (`RandomInt(0, 99) < 40`, then waiting for the clip), then
  fire/compute/walk, then compute/jump (IsLedge only when the earlier tests fail)/advance, and
  otherwise ComputeState; MC_Fire_Stand throws rocks for AttackFar, guard or a target above and
  higher than the body, else claws, always with `FIRE_STAND[4]` (no draw), and restores the claw when
  the clip ends; Dodge, GuardLocation and Special1 (eating) match Shikari. A taller target such as a
  Cultist counts as above the body, so it draws rocks even at close range.
- **Rocks** (`CGremlinRock`, `WEAP_GREMLIN_ROCK` 54: TYPE_MELEE, 10–20 damage, projectile and
  alternate `CRockProjectile`). Each shot draws a speed in [500, 1000] (`RandomFloat`) for both fire
  modes (`CWeapon::projectile_speed`), adds a world-up bias of `1 − (v − 500)/1000` to the direction
  and normalizes it. `AI_Mgr::WeaponStrToInt` maps `ROCK` (four bytes) only in the add-on build (the
  retail build keeps the source default Beretta); AI aiming uses the source `default` branch
  (`r_gun`, else the body position and forward: the Gremlin has no `r_gun`).
- **CRockProjectile** (`AddonProjectiles.{h,cpp}`): a gravity `rock1_pu` model flown by the shared
  model-projectile service (the grenade's first-contact overlap rule also covers an unbounced rock).
  Its contact replaces the base explosion: a touched character takes `m_fDamage × bounces_left / 5`
  (DAMAGE_TYPE_NORMAL, the shooter responsible); a world polygon reflects 0.3 of the normal velocity,
  halves it and draws new spins; any other contact stops it; each counted contact plays
  `Sounds\Bounce\basketball.wav` (500/50, priority 1) and uses one of five bounces. It never
  explodes, fades over its last 1.5 s and is removed at 6.5 s. As for the grenade, the replacement
  solver has already removed the inward velocity before the callback, so a floor landing counts one
  bounce and slides instead of rebounding (no invented restitution).
- **Saves.** `GremlinAI` and `CRockProjectile` are declared Unsupported until the add-on save lot
  (N9); WORLDS_AO saves are refused anyway.

Tests: `weapon` (rock data), `gremlin.configuration` (constructor values, model, table, ROCK
mapping, ComputeState), `gremlin.claw` (a same-height target clawed), `gremlin.rock` (a far target:
rock clip, 791-unit speed and upward unit direction, floor contact, bounce sound, fade and removal
at 6.5 s; a direct unbounced hit deals its full 15 damage), `gremlin.retail` (no GremlinAI, ROCK is
the Beretta). Real data: OPHELIA_CC_O creates its 9 Gremlins and GABBY_CC_G its 12; OPHELIA now
waits only for its FlayerPU (lot N5) and GABBY for its CombatShotgunPU (lot N4). Neither world can
start yet, so the throw is verified on a synthetic floor with the real assets, not in those worlds.
