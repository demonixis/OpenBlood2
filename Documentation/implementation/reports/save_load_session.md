# Restore-mode world and candidate transaction (save/load lot 4)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 4 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules
are recorded in the contract's
[implemented restore-mode world and transaction](../../contracts/save_load_contracts.md#implemented-restore-mode-world-and-transaction-lot-4);
the [source ledger](../sources/save_load_session_sources.json) lists the references
consulted. `GameSession::save_game` and `load_game` now save and load in memory through a
candidate session. Every original class is AlwaysLoad, NotSaved or Unsupported, so a save
of a real game is refused by name; no game path, menu or file uses them yet. M2 closure and
M2-GPU-001 are unchanged.

## What the original does

- `CBloodServerShell::LoadWorld` with `LOADTYPE_RESTORESAVE` loads the world without
  `LOADWORLD_LOADWORLDOBJECTS`, so the engine creates only the `CF_ALWAYSLOAD` classes, and
  then calls `RestoreGame`, which runs `RestoreObjects(Current.sav, 0, RESTORETIME)`. After it,
  `GAME_GOTOSTARTPOINT`, `GAME_REVISITING` and `GAME_DOAUTOSAVE` become 0.
- `SaveGame` lists every `USRFLG_SAVEABLE` object, active (`GetNextObject`) then inactive
  (`GetNextInactiveObject`), and saves with `SAVEGAMECONSOLE | SAVEPORTALS`.
- `WorldProperties` sets its console variables and `SetGlobalForce` in `MID_PRECREATE`. It is
  the only caller of `SetGlobalForce` in the object DLL. The later runtime restores the saved
  console inside `RestoreObjects`, after the world objects exist, and its save header holds
  no global force.
- 25 object-DLL files set `USRFLG_SAVEABLE`; `ClientLightFX` sets it only for a light that
  starts on, and `RandomSoundFX` never sets it.

## Delivered behavior

- **Declarations** (`src/game/registration.cpp`): 17 AlwaysLoad classes (the 11 registered
  original `CF_ALWAYSLOAD` classes and the 6 engine built-ins), 9 NotSaved classes, and every
  other class Unsupported. `save_class_schema` derives the build's schema from them; it is
  empty until a class lot declares an Original class.
- **Marking**: Door and Trigger OR `USRFLG_SAVEABLE` into their user flags as the original
  does. All ClientLightFX-family lights and RandomSoundFX are marked as decision-4 departures,
  so a save that holds them is refused by name until lot 10 supplies their payloads.
- **Shell**: selective ports of `SaveGame` and `RestoreGame`, and the original
  `GetNextInactiveObject` service.
- **Restore-mode world** (`src/game/session.cpp`): `load_saved_world` applies the saved CONS
  and CLCK after the world reset and before the first always-load object, creates only
  AlwaysLoad rows, marks every other row `SavedState`, refuses a pending deferred removal and
  calls `RestoreGame` with no step in between. `RestoreObjects` applies the saved console
  again once objects exist, and rolls it back on failure.
- **Transaction** (`src/game/saved_game.cpp`): `save_game` (admission, the unmarked-class
  guard, fingerprint first, then the shell save) and `load_game` (admission and fingerprint
  checks, NEW_LEVEL isolation, a candidate with its own copy of the store, the restore-mode
  world, the lot-5 player boundary, the startup and NEW_LEVEL checks, audio transfer and
  promotion). The `game_load` report names the stage of a refusal.

## Findings

- **Console and global force.** The saved console wins over WorldProperties, because the
  original restores the console after the world objects exist. The global force is not saved
  by the original and only WorldProperties sets it, so its authored value stands after a
  restore. A world without WorldProperties keeps the saved force (test `console`).
- **Sky targets.** 50 of the 58 mounted worlds bind their SkyPointers to Door or
  CDestructableBrush objects. These come from the save, so those worlds cannot be restored
  from an always-load-only image until lot 8; the sky preflight refuses them. The five
  single-player worlds without a SkyPointer (SubwayA, SteamTunnels, SubwayB, SubwayC,
  PowerStation) are the end-to-end case.
- **Real-game refusal.** A save of a fresh SubwayA, SteamTunnels, SubwayB, SubwayC or
  PowerStation is refused, naming the Unsupported saveable classes present (for example
  CClientExplosionSFX, CDestructableBrush, CameraObj, CinematicTrigger, ...).

## Tests

`save_session_tests` (labels `headless;objects;save`; `private` adds the commercial data):

| Case | Checks |
| --- | --- |
| `declarations` | Every registered class is AlwaysLoad, NotSaved or Unsupported as listed; unregistered original always-load classes; an empty build schema; inherited AlwaysLoad and an Original class in a registry schema |
| `marking` | `USRFLG_SAVEABLE` after initial update for eleven saveable rows (including the off light and both RandomSoundFX departures) and eight always-load or NotSaved rows; the save names every Unsupported class; an unmarked Trigger is refused as unmarked; a marked NotSaved object is refused; the store is unchanged by each refusal |
| `refusals` | Fingerprint failure without changing the store; non-single-player game type, live message, deferred removal, pending level change and no world refused; a second unbound session refused |
| `promotion` | Three save/load cycles: committed reports, revision +1 per load, the store copied with `Current` replaced and `Auto` kept, AlwaysLoad rows created and probes `SavedState`, clock, origin, force and console restored, the probe's deadline exact, LoadSave references remapped or NULL, shell variables 0, original lists rebuilt, a re-save byte-identical in OBJS, LINK and ATCH, CLCK differing only by the generator, CONS only by the shell variables; then a load into a session with no world (the menu case) |
| `order` | After a first world (origin 5 s) and a saved one, the restore-mode always-load object sees time 7 and world time 2 and draws the saved generator's next value, not seed 1's; pass-1 precreate sees the restored clock |
| `console` | The saved console (a runtime WorldName and difficulty) wins over WorldProperties; the global force returns to WorldProperties' authored value; a world without WorldProperties keeps the saved force |
| `failures` | 22 refusals, each leaving the source's objects, record count, revision, clock, generator, console, store (revision and image), strings, messages, lists, voices, original lists and active camera unchanged, with the expected stage: pending level change, unnormalized path, keep-alive image, missing console, CTF game type, schema mismatch (admission); world digest, mount set, payload version, unregistered class, null extra class (fingerprint); corrupt CLCK (world); refused precreate, under-read, over-read, truncated OBJS (restore); local-client record (player); leaked message, unavailable service, unported row, missing world texture (validation); a candidate that opened audio output (audio, with the source's output kept); then a final load commits and transfers the output |
| `private` | SubwayA, SteamTunnels, SubwayB, SubwayC and PowerStation: the real save is refused by name; an always-load-only image restores twice with the same always-load rows, no unknown or failed row, only always-load objects, the clock, a runtime `FarZ` console value and the authored gravity; each restored world steps and saves again |

Updated tests: `save_objects.phases` now expects the console to be applied again over an
always-load object; `save_objects.order` uses Trigger as its Unsupported original class
(CameraSpot is AlwaysLoad); `lights.lifetime` expects an initially-off light to be saveable.

## Execution evidence

macOS (Apple clang 21), branch on `9b9946b9`:

- **Dev** (`dev` preset, private data configured): the full headless label runs 1,248 tests
  (eight new `save_session.*`, including the private case); 1,241 pass. The seven failures
  are the pre-existing private-data content checks (doors.catalog, pickups.catalog,
  ai_recovery.catalog, ai_escape.catalog, assault.ai, mad_scientist.wounds,
  lightning.catalog), the same as on `9b9946b9` before this lot.
- **Release** (inspector-free `release` preset): builds; its headless run gives the same
  1,241 of 1,248 with the same seven failures.
- **Clang ASan/UBSan** (`testing.md` recipe, `ASAN_OPTIONS=detect_leaks=0` because Apple
  clang has no LeakSanitizer): the full headless label passes 1,240 of 1,248. The failures
  are the same seven plus the known ASan timeout of the slow private
  `locomotion.cityhub_roam`, and no test output holds a sanitizer report. After the last test
  edit, `save_session.*`, `save_objects.*`, the object contract suite and the light tests
  passed again under the sanitizers (45/45). Leaks are checked through object, string,
  message and list counts, not by LeakSanitizer.

No GPU, interactive or menu validation is specific to this lot.

## What lot 5 needs

- Replace the *player* refusal in `load_game`: require exactly one local-client record (D6)
  and attach a new local client through the restored-client branch
  (`OnRestoredClientEnterWorld`, `SetClient`, `SMSG_FORCEROTATION`, `SMSG_EYELEVEL`; no
  `GoToStartPoint`, no fresh `SINGLEPLAYER_INIT`), then deliver `SMSG_CLIENTSAVEDATA` and run
  run_world's player checks (in world, alive, associated, client music) before promotion.
- Declare CPlayerObj, CBaseCharacter's chain and the aggregates (CMovement, CInventoryMgr and
  items, CDestructable) Original with payload versions, and replace their
  `PRECREATE_SAVEGAME` refusals.
- In `save_game`, require a live in-world player associated with the local client, and
  attach the client's `SaveClientData` blob before the shell's `SaveGame`.
- Keep the typed NEW_LEVEL path working, and keep the decision that the saved console wins:
  the three shell variables are already forced to 0 after `RestoreGame`.

## Limits

- No original class has Save/Load bodies yet; a real game is refused by name.
- The restored session has no player and stays in the inspection state until lot 5.
- Menus, files, F5/F6 and the UI clearing on commit are lot 6.
- A restored sky object together with its always-load SkyPointer, which binds again at its
  first update, is a lot 8/13 check.
- The audio-transfer refusal is exercised with SDL's dummy audio driver.
