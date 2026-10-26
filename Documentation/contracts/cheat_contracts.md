# Cheats (Options > CHEATS)

The original game has typed cheat codes: `CCheatMgr::Check` (ClientShellDLL/MessageMgr.cpp)
matches the message line, `CCheatMgr::Process` prints a line and sends `CMSG_CHEAT` (code and
state bytes), and `CPlayerObj::ProcessCheat` (ObjectDLL/PlayerObj.cpp:3393) applies it. This port
offers selected cheats through a replacement menu page instead of the typed codes; the server half
is the original code.

## Menu

Options ends with a replacement `CHEATS` row (after VR and SETUP when offered, in both editions).
It opens a small-font page (`options.pcx`, Back returns to Options):

| Row | Kind | Effect |
| --- | --- | --- |
| `CHEATS [OFF/ON]` | setting 14, preference | Every other row is grey while OFF |
| `GOD MODE [OFF/ON]` | setting 15, preference | Original `CHEAT_GOD` while CHEATS is ON |
| `ALL WEAPONS` | `CHEAT_KFA` | Full ammo, then the source weapon list |
| `FULL AMMO` | `CHEAT_AMMO` | `SetFullAmmo` |
| `FULL HEALTH` | `CHEAT_HEALTH` | `CDestructable::Reset`: maximum armor and health |
| `ALL ITEMS` | `CHEAT_GIVEALLINV` | Medkit, flashlight, goggles, the Eye, binoculars at 100 |
| `ALL KEYS` | `CHEAT_ALLKEYS` (replacement, code 100) | Every key this world's pickups still offer |
| `KILL ALL ENEMIES` | `CHEAT_KILLALLAI` | `KILL` trigger to every `AI_Mgr` and the `mime` objects |
| `LEVEL [name]` | setting 16, not saved | Enter, left/right and the mouse cycle the levels |
| `GO TO LEVEL` | `MenuAction::Warp` | New game in the displayed level |

The action rows need an entered, living player (`in_world && alive` when the menu opens); the
page then says `Grey cheats need a game in progress.` After an action the page shows the original
`CCheatMgr` message line (`Monolith Rulez...`, `You have full ammo.`, `You are a GOD.`, ...), or
`Cheats are unavailable in this game.` when the server refuses. ALL KEYS reports the count.

`preferences.cfg` version 6 adds `cheats` and `god_mode` (flags); version 5 and older files load
both off. The demo offers the same rows; see the [demo menu](demo_menu_contracts.md).

## Server rules (original ProcessCheat)

- `CMSG_CHEAT` is the typed `PlayerCheat{code, state}`; `CBloodClientShell::SendCheatMessage`
  sends it only while in a world, and `CBloodServerShell` gives it to the client's player.
- No cheat applies in a network game: the source `IsMultiplayerGame` test reads the server shell's
  NetGame type (`ServerDE::net_game_type`), which every local game leaves single player. Story,
  Chosen (`GAMETYPE_ACTION`) and custom games, Credits included, accept cheats like the original
  (the client's Cheats menu also follows the network game mode). A refused god-mode request is not
  resent for the same player and state.
- KFA keeps the source order: `SetFullAmmo`, then for each of Beretta, submachine gun, shotgun,
  assault rifle, life leech, Tesla cannon, orb, voodoo doll, death ray, minigun, napalm cannon and
  sniper rifle not yet held (`HasWeapon`, right hand only), `ObtainWeapon` into the first free slot
  of ten. A fresh Caleb fills the slots at the death ray, so the last three are refused, as in the
  original. `_DEMO` refuses the non-demo weapons (existing `ObtainWeapon` rule). The weapon voice
  follows (`VME_WEAPON`).
- `SetFullAmmo` sets AMMO_BULLET..AMMO_FOCUS to the strength (focus: magic) column of the original
  table. Outside `_DEMO` it adds 100 of each bomb with `AddInventoryWeapon` (capped at ten); the
  source's separate `AddItem` bomb call has no counterpart, as this port's bomb items are the
  reserved slots.
- GOD sets `m_damage.m_bGodMode` (saved with the player) and plays the powerup voice when on.
- HEALTH: the source also revives a dead player; that branch is not ported, so a dead player is
  refused instead of being half revived by `Heal`.
- KILLALLAI uses the original `SendTriggerMsgToClass` (Trigger.cpp:88), which visits active then
  inactive objects. This port collects the targets before sending: a death creates corpses and
  queues removals during the walk. `CDestructable` turns `KILL` into Death damage from the player,
  so bosses with damage disabled survive.
- ALL KEYS (replacement): `AddKey` of every live, not yet collected `KeyPickup` item. The pickups
  stay in the world and their pickup triggers do not run; a used-up key can be taken again.
- Weapon, clipping, attribute, character, powerup and view cheats are not reached by the menu and
  remain unported (refused).

## Application rules

- `CheatFlow` (`app/cheat_flow.h`) runs at the frame boundary. Every frame it sends `CHEAT_GOD`
  when the entered living player's god mode differs from `cheats && god_mode`, so god mode
  returns after a new game, warp or load, and a save made with god mode loads without it while
  the switch is off.
- The LEVEL list is every mounted `worlds/*.dat` outside `worlds/multi/`, in file order: the 36
  single-player worlds of the retail data, Demo_01 and Demo_02 in the demo. The row follows the
  current world.
- GO TO LEVEL is a new game (`GAMETYPE_SINGLE`, or `GAMETYPE_ACTION` from inside a Chosen game)
  with the current character, at the current difficulty (medium outside a world), through the ordinary loading screen. It does not carry the
  inventory across (the original demo-only `SUPERZUG` used a NEW_LEVEL load).

Tests: `menus.state` (page, gating, preferences flow), `menus.preferences` (version 6) and the
private `menus.cheats` (each branch on SubwayA, the warp to CityHub, Credits accepting cheats and a
synthetic network type refusing them).
