# Original Fanatic and Prophet

`Fanatic` and `Prophet` are the selected original `ObjectDLL/Fanatic.cpp` and
`prophet.cpp` classes, registered for world, string (Spawner) and runtime creation.
They build on the existing [AI lifecycle](ai_runtime_contracts.md),
[state](ai_state_contracts.md) and [weapon](ai_fire_contracts.md) ports. Read those
before extending them.

## Construction, properties and initialization

`ConfigureRuntime` applies the original constructors: seeing/hearing 1500,
sensing 200, walk 75, run 180, roll 105, jump 400, flags CRAWL|JUMP|LIMP|DODGE,
strength 5. Fanatic: 50 HP, no armor, BERETTA/COMBAT/ASSAULTRIFLE, bullets 300,
BMG 50, shells 50, grenades 50, rockets 25, flares 40, cells 200, charges 120,
fuel 100. Prophet: 200 HP, 100 armor, BERETTA/COMBAT/ASSAULTRIFLE/COMBAT/
ASSAULTRIFLE, bullets 750, shells 150, grenades 50, rockets 100, flares 100,
cells 600, charges 400, fuel 200, BMG from the AI_Mgr default 0. The registered
property defaults differ where the source differs (BMG 150; Fanatic
Proximity_Bombs 25). World and string creation read the AI_Mgr properties as for
Cultists; these classes have no Male/skin booleans.

Every non-savegame precreate selects `Models/Enemies/{Fanatic,Prophet}.abc` and the
matching skin. Initial update uses one shared animation/sound table per class
(`sounds/enemies/fanatic`, `sounds/enemies/Prophet`), the halved damage multiplier
times the difficulty factor and the original HP/armor formulas. The Prophet then
obtains the melee weapon after base initialization, as its source does.

`AI_Mgr::RuntimeTables` now holds seven shared owners: Cultist male/female,
Civilian male/female, Gideon, Fanatic and Prophet. The existing exchange swaps the
whole array, so [destination rehearsal](destination_trial_contracts.md) and
[NEW_LEVEL](level_change_contracts.md) isolate all seven; earlier "five table
owners" statements are historical.

### Weapon names

`AI_Mgr::WeaponStrToInt` follows the original prefix order and lengths
(`SHOTGU` compares five bytes). A name matching no compiled branch takes the
source default Beretta. The `_ADD_ON` COMBAT/FLAYER/ROCK/NIGHTMARE_FIREBALL branches
are absent: the patched retail (`BLOOD2.REZ`, `BLOOD2P.REZ`, `BLOOD2P2.REZ`)
`object.lto` contain no FLAYER literal, which appears only inside that block; only
`BLOOD2NM.REZ` (the add-on, not mounted) has it. Authored `COMBAT` becomes the Beretta the actor
already holds. A matched family that is not integrated (SHOTGUN, SNIPER, HOWITZER,
PROX_BOMB, ...) still rejects creation; it is never replaced by a Beretta.

### Proximity bombs

The source calls `AddInventoryWeapon(INV_PROXIMITY, (int)Proximity_Bombs)` for every
NPC. Retail stores a slot-10 inventory weapon with
the ammo capped at 10 (the original table for every strength). The AI never selects
it: `FindBestWeapon` scans slots 0..9 and `MC_LayProximity` has no caller. The port
makes the call only for a nonzero count; the source's empty zero-count weapon is
omitted to leave existing NPC inventories unchanged. Earlier
"proximity inventory weapons are not integrated" rejections are superseded.

## Decisions and actions

- **ComputeState** keeps each class's thresholds. Fanatic: health < .25 FindHealth,
  < .5 or SIGHT <= .5 or a height difference far attack, else close; a lit Fanatic
  (`m_fStartTime`) re-enters Special1 at command 3. Prophet: < .15 FindHealth, < .5
  far, an escaping Prophet looks for ammo. Without a stimulus and with the target
  within body height, a Fanatic with health >= .5 searches; the Prophet searches
  only below .30 (source quirk).
- **AttackClose/AttackFar** (`CrouchAttack`) share the source bodies: close threshold
  .33 x seeing and walk/crawl/run selection, far .75. Standing versus crouching fire
  follows dodge flags, health, the current crouch command and target height.
  Prophet's close attack omits the height clause the others have.
- **MC_Fire_Crouch** is the original command 11: pistol clip without a weapon,
  ignored clip result (a blocked resize continues), reload cadence and
  stop-velocity completion.
- **Dodge** (`RollDodge`) rolls right/left, otherwise forward (13, optional loop) or
  backward (14), unlike CultistAI.
- **FindHealth**: the original `FindObjectInRadius` scans only exit, smell, ammo and
  actor lists. HealthPU is a PowerupPickup, so no candidate is ever found and the
  state escapes on its first command. Its later path commands are unreachable and
  fail explicitly if forced.
- **Fanatic escape** searches a visible ExitHint avoiding the target, uses
  Special1 on any stimulus, missing hint, arrival or a blocked route; hide runs
  to any hint, else attacks. **Prophet escape** switches to melee and swings within
  75 units. The Prophet has no hide or special handler; the original base versions
  are empty, so those states are no-ops (the port's base reports "not integrated").
- **Prophet melee** `MC_Fire_Stand` plays FIRE_STAND[IntRandom(4,6)] without firing;
  damage comes from model keys. Its `Fire` chooses alternate fire when both bodies
  move at <= 5 units/s, the weapon is not the assault rifle, napalm or sniper rifle,
  and ammo covers the alternate use. A missing target velocity keeps primary fire
  (the source reads an uninitialized vector).

## Fanatic kamikaze

Special1: play SPECIAL[0]; hide `torso_extra1`, set 50 HP and 50 armor, start the
clock, and create a BaseClass smoke source at that node (OT_NORMAL, 0.01 s update,
FLAG_FORCECLIENTUPDATE). The next update before AI_Update publishes the original
`OBJFX_SMOKING_2` (21, scale 3, SCALENUMPARTICLES) through the existing
object-particle service and attaches the source to `torso`. Command 3 faces and runs
until within 100 units or 30 s; command 4 detonates.

Detonation (also the lit Fanatic's Dying override) creates a `CHowitzerShell` at the
body with `Setup(-up, WEAP_HOWITZER, 50, 250, 200, self)`, calls `CreateGibs(up,
mass >> 5, EXPLODE, 50)` through the shared AI gib path, removes the smoke and its
attachment, and sets `m_bRemoveMe`. A missing node/transform fails the state
explicitly; smoke, shell and gib failures are cosmetic and stay inspectable.
As in the source update, any Dying state also removes an attached smoke source.

## CHowitzerShell

The shell's single update casts 3000 units along its forward vector with
`INTERSECT_OBJECTS | IGNORE_NONSOLID` and the liquid filter, recasting once past
its shooter. On a hit it teleports there, uses the existing impact/normal path and
`EXP_HOWITZER_PRIMARY` (9) explosion record with `c4\explosion_1.wav` (radius 1000,
inner 100, MISC_MEDIUM), applies radius damage and, for a character shooter, direct
damage to a hit object. It then returns false and is removed; a miss expires. No
type-9 visual recipe is ported yet: the explosion record lists each component as
unavailable. The Fanatic removes itself the update after creating its shell; the
source handle stays usable until the end of that frame, so a shooter already
pending removal is unlinked first and the blast is attributed to no one, as after
MID_LINKBROKEN. The player Howitzer weapon remains unavailable.

## Other world objects

`SniperRiflePU` is the original weapon pickup (value 10, `Sniperrifle_pu`). The sniper
rifle weapon is not integrated, so the player inventory refuses the offer and the
pickup stays in the world. Spawners may now create `Fanatic` and `Prophet`
([Spawner contracts](spawner_contracts.md)).

## Inspection, evidence and limits

The Skeletons AI panel shows shared attack values, dodge flags and roll command,
FindHealth calls, kamikaze start/fuse/detonations, smoke source/attachment/effect,
errors, and the Prophet's melee swings and alternate choices.

Retail 05_CDMB initializes four Fanatics and the Prophet; `--start-game` passes, and
a Spawner trigger creates a ready Fanatic. Tests:
`fanatic_prophet.weapons`, `decisions` and `howitzer` (synthetic, labelled actors and
targets), `catalog` (real models, three difficulties), `kamikaze`
(real Fanatic sprints to the player, smokes and its blast deals damage), `actions`
(rolls, crouch fire, melee swing) and `world`.
Not implemented: CacheFiles, save/load (restore rejects), the Fanatic's Load smoke
re-creation, the howitzer explosion visuals, the sniper rifle and Howitzer player
weapons, and verified retail AI timing. See the
[source ledger](../implementation/sources/cdmb_world_sources.json).
