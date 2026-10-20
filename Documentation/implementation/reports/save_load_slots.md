# Slots, Quick and Restart (save/load lot 6)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 6 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules are
recorded in the contract's
[implemented slots, Quick and Restart](../../contracts/save_load_contracts.md#implemented-slots-quick-and-restart-lot-6);
the [source ledger](../sources/save_load_slots_sources.json) lists the references consulted.

The Save and Load screens, F5/F6 and the death Load menu now save and load real files. Most real
games are still refused, by class name, until the class lots (7 and 9 to 13) declare their classes
Original. M2 closure and M2-GPU-001 are unchanged.

## What the original does

- **Slots.** `Save\Current` is the only directory the server writes. A slot or the quicksave is a
  whole-directory copy made after the server's ACK. Loading copies the slot over `Save\Current`
  before trying to load (LoadSave.cpp:245-414; BloodClientShell.cpp:8325-8454).
- **Names.** The Current name is the world title (`GetWorldTitle`, the world info `worldname`).
  A slot appends `" [%.15s]"` of `ctime` + 4, and an empty slot reads `<EMPTY SLOT>`
  (LoadSave.cpp:25, 41, 429-453).
- **Menus.** Load lists `RESTART: %s`, `QUICKLOAD: %s` and the eight slots; row 0 is
  `SLOT_CURRENT`, row 1 `SLOT_QUICK`, row n slot n − 2. Save lists the eight slots
  (MenuLoadGame.cpp, MenuSaveGame.cpp).
- **Keys.** F5 quicksaves in play without pausing and shows "Quicksave..."; F6 quickloads and
  shows "Quickload..." only if the load started. Both are compiled out of the demo and disabled in
  multiplayer (BloodClientShell.cpp:3877-3896).
- **Death.** USE while dead in single player opens the Load menu as the top-level menu; the demo
  opens the main menu (BloodClientShell.cpp:3488-3526).
- **Restart.** `Auto.sav` is taken after each NEW_GAME and NEW_LEVEL entry while
  `GAME_DOAUTOSAVE` is set. It travels with each slot copy, and New Game clears `Save\Current`
  ([audit §4, §5.5, §6.2](../save_load_audit/shells.md#4-autosave-the-restart-source)).
- **Errors.** Every LoadSave error is silent (`PrintError`'s inverted check); a failed save
  soft-locks the client (audit §8).

## Delivered behavior

- `src/game/save_slots.*`: slot file names, the preference directory, the `ctime` + 4 timestamp,
  the `GetWorldTitle` parser and `GameSave`↔container conversion. `SaveSlots` adds the listing,
  `save`, `capture_level_entry`, `clear_level_entry` and `load`, with an inspection report.
- `src/app/save_flow.h`: `SaveFlow` (menu commands, quicksave, quickload, level entry),
  `SaveNotice` and `reset_after_promotion`, the shared NEW_LEVEL and load UI/input reset.
- Menus: `MenuAction::SaveGame`/`LoadGame` with the original command ids and row mapping, the
  `MenuSaveSlots` listing, `Menu::open_load` (Load as the top level) and `set_saves`.
  `MenuItem::insertion` fills the cres.dll `%1!s!`. The "not available yet" footer becomes the
  unavailable reason.
- `MenuInput`: owned F5/F6 requests, cleared by `reset`. `CBloodClientShell::dead_use_menu`
  records USE during the death camera.
- `GameSession::save_game` takes the save type; the level-entry snapshot sends `SAVETYPE_AUTO` and
  reads the `Auto` store image.
- Frontend: the per-user directory (not used by smoke runs or the demo), the frame-boundary
  requests, the notice overlay, the snapshot after each entry, error modals (a refused F5/F6 first
  opens the menu, which pauses), and inspector rows.
- Departure D14: a refused snapshot removes `current.ob2sav`, so Restart never reaches an
  earlier level.

## Verification

macOS 27.2 (arm64), Apple clang 21, dev preset (RelWithDebInfo with inspector), commit base
`cfe2bf58`.

- New `save_slot_tests` (labels `headless;menus;save`):
  - `naming`: file names, the `saves/retail` preference path, the space-padded timestamp,
    `worldname` parsing with quotes and `;`, and the file-name fallback.
  - `meta`: real slot and quick files; the header/META quick read; a fresh listing equal to
    the written META; menu rows, insertions and enable state; disabled slots.
  - `roundtrip`: save to slot 2, a fresh listing loads it through the real file (new revision,
    restored health), a save after the load has the same OBJS size, LINK, ATCH and
    fingerprint, a refused save keeps the file and leaves no temporary, then an overwrite.
  - `corrupt`: a flipped byte (CRC) leaves the session untouched; truncation is unreadable in
    the listing and refused on load; a quicksave renamed to a slot is refused by kind; a
    non-save file; an empty slot returns `Empty`.
  - `mismatch`: an added mod is refused, naming `mod.rez`, from a session with no world; a bumped
    CPlayerObj payload version is refused, naming both OpenBlood2 versions; the untouched slot
    then loads into the empty session.
  - `keys`: F5 only in play, alive and in a world; F6 also when dead; neither in the menu, the
    inspector or the demo; repeats, F2 and F3 do nothing. Empty quickload notice, "Quicksave..."
    overwrites, notice expiry, refused F5 while dead, F6 replaces the dead session.
  - `demo`: saves, snapshots and loads refused for the demo edition; demo Solo has two rows and
    `open_load` stays on Main.
  - `restart`: New Game snapshot (bare title, kind level entry); slots embed it as AUTO; Restart
    repeats and is not refreshed by a restore; NEW_LEVEL replaces it; loading slot 0 restores
    slot 0's AUTO in memory and in `current.ob2sav`; a slot without AUTO clears Restart; a
    refused snapshot removes the file.
  - `reset`: a menu load clears the selection, controlled object, F5/F6 and dead-USE requests
    and queued shell input, and requests play.
  - `traversal`: Solo → Save → slot 2 (menu closes), the rows follow the file, Solo → Load →
    slot 2, a refused save keeps the page, death USE → Load as the top level → Esc resumes, then
    Restart from the death menu.
  - `refusal`: a saveable class without support is named ("SlotUnported (Unsupported)") and the
    previous file is byte-identical.
  - `private` (real data): a SubwayA New Game snapshot and slot save are refused, naming 13
    classes (CClientExplosionSFX, CameraObj, CinematicTrigger, CivilianAI, CultistAI,
    DetailSprite, ExitTrigger, Gideon, GlowingLight, KeyFramer, ObjectivesTrigger, RandomSoundFX,
    Trigger). No slot or current file is written. Once every class is supported, the test
    switches to load and Restart checks.
- `menus.artwork` (private) expects the `<EMPTY SLOT>` insertion instead of "unavailable".
- Dev headless suite: 1272/1280 pass. The failures are the 7 known pre-existing private-data
  checks (doors, pickups, ai_recovery, ai_escape and lightning catalogs, assault.ai,
  mad_scientist.wounds) and `locomotion.cityhub_roam`, which exceeded its 120 s timeout while
  the machine ran at load average 181 from parallel sessions. Run directly, it passes: 33 s of
  CPU in 4 min 56 s of wall time.
- Inspector-free Release headless suite: 1274/1281, failing only the same 7 private-data checks.
- Clang ASan/UBSan (Apple clang 21, `detect_leaks=0`; LeakSanitizer is unsupported on macOS):
  every `save_slots`, `save_messages`, `save_file`, `save_objects`, `save_session`,
  `save_player`, `save_movers` and `menus` test passes with no sanitizer report.
  `menus.profiles` exceeded its 180 s timeout under load and passes when run directly (2 min 36 s).
- No GPU menu smoke was run: the `--menu-smoke` traversal never touches saves, and the menu flow
  is covered by the headless `traversal` case.
