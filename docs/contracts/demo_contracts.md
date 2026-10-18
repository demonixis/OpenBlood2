# Original demo edition (`--demo`)

`--demo` declares that `--data-dir` holds the 1998 two-level demo (`B2Demo.rez`)
instead of the retail game. The edition is chosen explicitly, never inferred from
directory contents. It replaces the original compile-time `_DEMO` define with a
runtime switch fixed for the session. Read the [menu contracts](menu_contracts.md),
[pickup contracts](pickup_contracts.md), [proximity](proximity_contracts.md) and
[remote-bomb](remote_bomb_contracts.md) contracts before extending the demo paths.
The demo is **not playable yet**; see the limits below.

## Content layout

- The demo ships every resource in one archive, `B2DEMO.REZ` (matched without
  case). It is the only archive inspected and mounted. The retail set is
  `BLOOD2.REZ` (required), `SOUND.REZ`, `BLOOD2P.REZ`, `BLOOD2P2.REZ`, and
  `BLOOD2NM.REZ` (inspected only). Each mode ignores the other edition's files.
  A missing required archive names the other edition's archive when that one is
  present (`add --demo` / `omit --demo`).
- The first single-player level is the original `sFirstLevel`: `Demo_01` with
  `--demo`, `01_SubwayA` otherwise (`first_level`). It is used by menu New Game
  and by `--world` when no `--world` is given. An explicit `--world` wins.
- `Resources` records the edition in `mount_game`. Session copies carry it to
  new-game, level-change and destination-rehearsal sessions. `ServerDE::DemoBuild`
  exposes it to original code through `IsDemoBuild()`, catalogued and traced like
  every adapter boundary. Its value never changes during a session.
- The scripted smoke diagnostics assert retail SubwayA/CityHub content and reject
  `--demo`. Normal launch, `--view-world`, `--check-data`, `--inspect-world`,
  `--inspect-player`, `--start-game` and `--bench` accept it.

## Original `_DEMO` branches connected

- `CInventoryMgr::ObtainWeapon`: after the slot search, players obtain only
  `IsDemoWeapon` types: MELEE, BERETTA, SUBMACHINEGUN, ASSAULTRIFLE, VOODOO and
  TESLACANNON. Any other type is `NotAvailable`, so the Flare Gun is refused. No
  bomb weapon is constructed for any owner. NPC weapons are otherwise unchanged.
- `AddInventoryWeapon` is unconditionally `NotAvailable`.
- `ItemPickupWeapon` initial update removes INV_PROXIMITY/INV_REMOTE pickups
  (deferred) and skips USRFLG_GLOW on every weapon item, as the source `break`
  does. A pickup already pending removal skips its own model placement, because
  checked transforms reject pending objects. The base chain still runs.
- `COMMAND_DETONATE` is compiled out of the demo, so `OnCommandDetonate` is a
  successful no-op.

The default solo profile grants only the Beretta, so player entry is unaffected.

## Shipped build versus source

The shipped demo is an older client than the `_DEMO` source. Its `cshell.dll`
reports `DEMO v1.0 (Build 108)`, its `cres.dll` `Ver 1.0 (Build 178)`, and the
source reports `DEMO v1.0a (Build 109)`. That `cres.dll` holds 43 strings (ids
0..48) and none of the `IDS_MENU_*` strings (500+) that the source menus use.
The menu labels are compiled into `cshell.dll` instead.

## Demo menu

With `--demo`, `MenuCanvas` reads its labels from the demo's own `cshell.dll`
(`read_demo_menu_strings`), never from `cres.dll` and never from embedded text.
The DLL must be exactly 719,872 bytes with `DEMO v1.0 (Build 108)` at file offset
0x932d4. Each of the 24 source string ids maps to the one literal that its
`CMenu*::Build` code pushes, at a fixed `.data` offset. Each literal must start
after a NUL and be printable ASCII of 1..64 bytes. Any mismatch fails without a
partial table. The demo's own casing is kept (`Detail Level [LOW]`, `Sound`,
`16 Bit Sounds`); retail uses capitals.

`Menu::demo` selects the original `_DEMO` branches:

- Main uses `IDS_MENU_MAIN_PRIMEGIBS`. Bloodbath and Prime Gibs are enabled and
  open `screens/bloodbath.pcx` / `screens/info.pcx`.
- Quit opens `screens/info.pcx` with exit and no confirmation box.
- Single player lists only Caleb and Chosen. Chosen opens `screens/modes.pcx`
  instead of character selection.
- The menu ambience maps 1→2 and 3→4, because the demo ships `ambient02`/`ambient04`
  only.

Splash screens follow `Splash_OnKeyDown`. ESC, SPACE and RETURN return to the
unchanged page, or quit after the exit splash. The mouse and other keys are ignored.
The 640×480 PCX fills the canvas, which keeps its aspect like the other pages, whereas the
original stretched it to the screen. Options keeps Display and Audio. Mouse is
disabled because this build has no mouse-page labels to show. Controls, Joystick
and Keyboard stay disabled as in retail.

## Still unavailable

Both demo worlds are DAT v56. Playing them is not a goal: original classes are
ported for the retail worlds, and no demo-specific world support is kept.

Several source `_DEMO` features are not connected:

- The demo build's version string on the main menu and its mouse page.
- End of demo: the `EndGame` exit returns to the main menu and shows `info.pcx`.
- Loading screens `loading1/2.pcx`, demo cheats (SUPERZUG → `Demo_02`), the time
  bomb, autosave and multiplayer differences.

Tests: `menus.demo_state` and `menus.demo_strings` cover the demo menu controller,
splash input and label validation. `menus.demo` composes every reachable page, the
three splash screens and both ambiences from the real demo. `config.demo` covers
parsing, smoke rejection, edition inspection and mismatch hints. `world.resource_mount` covers edition-exclusive mounting.
`remote.demo` covers weapon/bomb/detonate rules and pickup removal on a labelled
synthetic index. No test plays the real demo worlds.
