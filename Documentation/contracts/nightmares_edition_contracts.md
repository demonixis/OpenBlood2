# Nightmares edition contracts (lot N1)

Lot N1 of the [Nightmares plan](../implementation/nightmares_plan.md): the expansion becomes a
selectable **edition** beside retail and the demo. It mounts the expansion archive and exposes the
runtime switch the later lots port the original `_ADD_ON`/`_ADDON` branches behind. It adds no
expansion behaviour: the NIGHTMARES menu row, the `WORLDS_AO` campaign, the character chain and
the new classes are lots N2–N9. With the edition selected, the game plays The Chosen as before.

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
`saves/retail/`. Loading still requires a `worlds/` DAT path: `WORLDS_AO` saves are lot N9.

## Weapon numbering (decision 1: superset)

Every retail weapon, ammo, cheat and save number is unchanged. The four add-on weapons are
appended after the inventory bombs: `WEAP_COMBATSHOTGUN 52`, `WEAP_FLAYER 53`,
`WEAP_GREMLIN_ROCK 54`, `WEAP_NIGHTMARE_FIREBALLS 55` (the 1999 build numbers them 18, 19,
`WEAP_LASTPLAYERWEAPON + 32/+33` and shifts the others). They have no `WeaponData` yet, so they
cannot be obtained. The original `type <= WEAP_LASTPLAYERWEAPON` test is
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
