# Player and client (save/load lot 5)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 5 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules
are recorded in the contract's
[implemented player and client](../../contracts/save_load_contracts.md#implemented-player-and-client-lot-5);
the [source ledger](../sources/save_load_player_sources.json) lists the references
consulted. CPlayerObj is the first Original class: `GameSession::save_game` and `load_game`
now save and load the local player with its aggregates and client blob, and reattach it to
a new client. Every other saveable class is still Unsupported, so a save of a real game is
refused by name; no menu, key or file uses these calls yet. M2 closure and M2-GPU-001 are
unchanged.

## What the original does

- `CPlayerObj::Save`/`Load` (PlayerObj.cpp:6541-6873) write 85 fields, then the client blob.
  `CBaseCharacter` writes nothing. The aggregates follow in reverse order of addition:
  `CMovement` (movement.cpp:584-638), `CInventoryMgr` (InventoryMgr.cpp:2843-3061, with
  `CInvItem::Save`, InvItem.h:193-238, and the item subclasses in GameInvItems.cpp) and
  `CDestructable` (Destructable.cpp:1134-1245).
- `CMSG_SAVEGAME` (BloodServerShell.cpp:401-431) carries the client's `SaveClientData` blob
  (BloodClientShell.cpp:6351-6381). The server attaches it to the sender's player, whose next
  Save consumes it. `CPlayerObj::Load` re-broadcasts it as `SMSG_CLIENTSAVEDATA`, which
  `LoadClientData` reads (BloodClientShell.cpp:6391-6431).
- A restored player is reattached through its client reference: `RespawnPlayer` sends
  `SMSG_FORCEROTATION` and `SMSG_EYELEVEL` and skips `GoToStartPoint` because
  `GAME_GOTOSTARTPOINT` is 0, then sets it to 1. `CMSG_SINGLEPLAYER_INIT` is not resent.
- `LoadWorld` sets `GAME_GOTOSTARTPOINT` to 1, `GAME_REVISITING` to 0 and `GAME_DOAUTOSAVE`
  to 1 for a fresh or keep-alive entry, and all three to 0 after a restore
  (BloodServerShell.cpp:974-982).

## Byte layouts

Abbreviations: B Byte, W Word, D DWord, F Float, V Vector, R Rotation, HS HString, LSO
LoadSave object, MO plain object, N nested message. "Carried" means an original field this
port keeps in `PlayerCarried` without interpreting it.

| Block | Fields, in order |
| --- | --- |
| CPlayerObj | B demo playback (carried); D control flags (carried); B firing (carried); B falling; B server falling; F fall start − t (−1 sentinel); F idle start − t (carried); F min and max fall; F binding and server regen (carried); B spectator; F smell time − t; F smell delay; B dead; B movement blocked; B×5 power flags; F×5 power expiries − t; B stack size and B per entry; HS trigger; V last position, last velocity, scale (carried); F pitch; F yaw; R view rotation and V forward, gun muzzle, left gun muzzle (carried); HS player name; B client flags, F×2 mouse axes (carried); B zoom; B zoom type; MO camera (NULL, D7); B last crouch; B last jump (carried); B forced crouch; D frags and melee kills (carried); B character; B skin; B×4 attributes; B×4 binding attributes and B×8 bindings (carried); B movement state; W×3 injured leg, node hit, side hit (`(DBYTE)`, carried); F death timer and B slow death (carried); F old air (carried); F air; B swimming jump and can-swim-jump (carried); B imprisoned; F imprison start − t; F imprison length; B burning and F burning time (carried); B blob flag; N blob |
| CMovement | LSO owner (NULL for the player); R rotation; V right, up, forward, position, last position; D path count; V per point |
| CInventoryMgr | D `0xCECECECE`; B strength; B magic; B god mode (0); B infinite ammo; B×20 right/left weapon types per slot; B×3 inventory bombs (`type + 7 − WEAP_PROXIMITYBOMB` or 0); F×13 ammunition; D item count; per item B type, LSO owner, HS name, display name, picture, highlighted picture, B initialized, dropped, type, active, count, then F discharge time (flashlight, goggles) or LSO seeing eye (eye); D current slot; D current weapon type; D current item (0); D `m_WeapCount` (0); F damage multiplier; F fire-rate multiplier (1); F melee multiplier (1); B drop eye (0); D `0xCECECECE` |
| CDestructable | D `0xCECECECE`; LSO owner; B damage physics; F mass, max health, max mega health, health (or 1.0e6), death health, max armor, max necro armor, armor, resistance; B dead; B nigh invulnerable; B last damage type; B last damage location; V last direction; F last amount; F last percent; HS×5 damage target and message, death target and message, spawn object; V spawn velocity; B god mode; B trigger only; F death delay; D `0xCECECECE` |
| Client blob | B night goggles (discarded on load); F wonky time; B wonky vision; B wonky no-move (0 here); F blind time (absolute); B blind; B fade in; B fade out; F fade; B music level; F last load type (discarded); F game type; HS objectives title; HS objectives text |

## Delivered behavior

- **Declarations**: CPlayerObj is Original with payload version 1; the build schema is
  `{CPlayerObj: 1}`.
- **Byte bodies** for CPlayerObj, CMovement and CInventoryMgr, dispatched from
  `MID_SAVEOBJECT`/`MID_LOADOBJECT` in the original order; the CDestructable block is lot 8's.
  Each converts to the existing typed value and calls the typed Save or Load, so the existing
  validators and staged commits apply. A refusal calls lot 8's `RejectPayload` service.
  Strings use lot 8's token-free `save_fields` helpers (new nullable variants keep a NULL
  HString distinct from an empty one), and relative clocks load through its `restored_time`.
- **Typed player block**: `PlayerState` gains spectator mode, the five power flags and
  expiry offsets, the power sound stack, the idle offset and `PlayerCarried`. NEW_LEVEL
  carries them too; the active-power refusal is removed (CTF, Orb and weapon-callback
  refusals remain). `RestoreModelState` now also resets the zoom type.
- **Client blob**: `SaveClientData`, `LoadClientData` and `m_nLastLoadType` in the client
  shell, `OnSaveGameMessage` (the original `CMSG_SAVEGAME`) in the server shell, and
  `CPlayerObj::SetClientSaveData`. The new `SendClientSaveData` service and
  `take_client_save_data` replace the original broadcast; the new `MessageFullyRead` query
  checks that `LoadClientData` read the blob exactly.
- **save_game**: requires the live local player in the world, builds the request and
  releases a blob a refused save did not consume.
- **load_game**: the *player* stage (`attach_saved_player`): exactly one restored local
  player, a new client through `OnRestoredClientEnterWorld`, the restored client entry, the
  blob, the player messages, weapons, hand models, frame end and attachments. The validation
  stage adds RunWorld's player checks and the session ends `Ready`.
- **Shell console**: the fresh and keep-alive `LoadWorld` values (1, 0, 1).
- **Engine record**: the saved model and skin replace precreate's choice before the
  animation track is applied (departure D13).

## Findings

- **CPlayerObj's precreate forces Caleb.** `PostPropRead` runs on every precreate and sets
  `Caleb.abc`; the later runtime then applies the saved animation trackers to that model
  before `Load` calls `SetCharacter`. For another character the track would be refused
  (an out-of-range clip) or reset by the model change. D13 restores the saved model first;
  the `character` test fails without it.
- **Powers were already in the original Save.** The earlier typed snapshots described the
  source as saving no power state; the original writes all five flags, their relative
  expiries and the sound stack. Saving them removes the transfer refusals and the Stealth
  and Willpower inconsistencies recorded in the timed-power contract.
- **Relative clocks.** `saved + GetTime()` then `− GetTime()` in binary32 can change the
  offset by one rounding step. Loading through lot 8's `restored_time` (the nearest float
  that yields the saved difference) makes a re-save after a load byte-identical.
- **String tokens.** A temporary HSTRING per saved string would consume session string tokens,
  which are never reused; the player's blob and inventory strings are written without them.
- **Real worlds.** SubwayA, SteamTunnels, SubwayB, SubwayC and PowerStation each restore a
  player-only image twice: the always-load rows come back, the player is reattached at its
  position with its health, armor, ammunition, key, Anger and Willpower (with the remaining
  expiry), and the restored world saves again through the original shell with the client blob.

## Tests

`save_player_tests` (labels `headless;objects;save`):

| Case | Checks |
| --- | --- |
| `layout` | An independent decoder of the original layouts reads the player's record exactly: relative clocks and the −1 sentinel, power flags, expiries and sound stack, trigger and name strings, carried fields, zoom bytes, NULL camera, attributes, imprisonment, the nested blob (goggles, wonky, blind, fade, music, load type, game type, objectives), the NULL `CMovement` owner and transform, inventory markers, weapons (dual Beretta), raw ammo, four items in type order with owner index 0 and key strings, current slot and weapon, the tripled multiplier, damage markers, owner index, raw values and trigger strings; then the CanDamage sentinel |
| `roundtrip` | Load into a candidate: position, flags without SHADOW, zero velocity, zoom off, raw attributes and maxima (speed 310 from the raw attribute, no ValidateAttributes), health, armor, resistance, trigger strings, rotation sent to the client, weapons, ammo, key, items inactive, multiplier, powers and imprisonment with their remaining time, carried fields, objectives, fade, blind clock, music level, load type 3, the shell console (1, 0, 0); a re-save matches field by field except the listed rewrites, relative clocks included; the session steps |
| `powers` | Willpower, Stealth, Anger and imprisonment survive a load with their remaining time, the multiplier, nigh invulnerability and alpha; each expires at its saved deadline, including after a second save and load midway |
| `refusals` | No player (save); two players; no local-client flag; an oversized nested blob length (restore); a truncated, wonky, out-of-range or foreign-game-type blob (player); an invalid attribute, boolean, power stack, weapon type, inventory marker, damage marker and damage type (restore); a dead player (player); each leaving the source session, its player, revision, messages and pending blob unchanged; then the intact save loads |
| `character` | An Ophelia player keeps its model, skin, animation track and eye level through a load (D13) |
| `level_change` | NEW_LEVEL carries Anger with its remaining time, the multiplier and imprisonment, drops the key, records load type 2 and the console (1, 0, 1); the destination saves and loads |

Updated tests:

- `save_session.*`: the synthetic worlds enter a player. `declarations` expects CPlayerObj
  Original. `refusals` checks the player requirement. `promotion` checks the attached player,
  the `Ready` state and the console. Its re-save compares every probe record bytewise; the
  player record is checked by `save_player`. `failures` refuses an image without the
  local-client flag. `private` runs the real worlds with the player.
- `save_objects.order` uses CultistAI as its Unsupported original class.
- `save_movers.private` (lot 8) enters a player in every world (the explicit inspection marker
  on multiplayer maps, with the test-only CTF team cleared because CTF state is never saved), so
  its images hold the local player; its comparison applies the player's listed rewrites.
- `airship.anger` expects active Anger to be captured with its expiry.
- `player_state.failures`, `player_state.capture`, `player_restore.failures`,
  `character_nodes.restore` and `medkit.stealth` now expect powers to be saved, and use CTF
  state or an invalid sound stack as the remaining refusals.

## Execution evidence

macOS (Apple clang 21), rebased on lot 8 and the enhanced renderer merge (`1d4d43b6`):

- **Dev** (`dev` preset, private data configured): the full headless label runs 1,269 tests
  (six new `save_player.*`); 1,262 pass. The seven failures are the pre-existing private-data
  content checks (doors.catalog, pickups.catalog, ai_recovery.catalog, ai_escape.catalog,
  assault.ai, mad_scientist.wounds, lightning.catalog). `save_session.private` (five real
  worlds with the player) and `save_movers.private` (58 worlds entered, 50 of 50 sky worlds
  restored, `drop` whole) pass.
- **Release** (inspector-free `release` preset): builds; its headless run gives the same
  1,262 of 1,269 with the same seven failures.
- **Clang ASan/UBSan** (`testing.md` recipe, `ASAN_OPTIONS=detect_leaks=0` because Apple
  clang has no LeakSanitizer): 1,261 of 1,269; the same seven failures plus the known ASan
  timeout of the slow private `locomotion.cityhub_roam`, and no sanitizer report. Leaks are
  checked through object, string, message and list counts.

No GPU, interactive or menu validation is specific to this lot.

## What lots 6, 9 and 11 need

- **Lot 6** (slots, Quick and Restart, menus): call `save_game`/`load_game` at the owning
  frame boundary. On a committed load, clear UI selections, queued commands and input
  ownership and exclude the preparation time, as after NEW_LEVEL. The level-entry snapshot
  is taken after single-player initialization: `GAME_DOAUTOSAVE` is now 1 after a fresh or
  keep-alive load and 0 after a restore. The session is `Ready` after a load and needs no
  RunWorld.
- **Lot 9** (pickups and projectiles): make SeeingEye Original and replace the
  `CInventoryMgr` refusal of a thrown eye (a non-NULL `m_hSeeingObject`). The original
  `CInvTheEye::Load` calls `SeeingEye::Init(owner, NULL client)`, and the eye's own Load
  re-links its owner and sends `MID_ATTACH`. Remote bombs are not in the player's saved list
  (the original loses `m_pBombList` too).
- **Lot 11** (AI core): declare the NPC classes. Their `CMovement` records carry their own
  owner (accepted only when it is the loading object). The aggregate `CDestructable` and
  `CInventoryMgr` bodies apply unchanged; NPC inventories are typed like the player's. CCorpse
  reuses the `CDestructable` block, whose owner reference is set only when initial update did
  not bind it.

## Limits

- No other original class has Save/Load bodies; a real game is refused by name.
- Wonky vision, the night-goggles state, the current item cursor and non-key item names have
  no port storage: the goggles flag and load type are discarded as in the original, the rest
  is written with the original idle values and a blob that carries wonky vision is refused.
- Dead players, CTF state, an active Orb and a thrown eye are not saved.
- Menus, files, F5/F6 and the UI clearing on commit are lot 6.
