# Original Blood II save/restore trace (server shell + client shell)

Root of cited source: `lithtech/Blood2` in the LithTech reference tree. Abbreviations:

| Tag | File |
|---|---|
| CS / CSH | `ClientShellDLL/BloodClientShell.cpp` / `.h` |
| LS / LSH | `ClientShellDLL/LoadSave.cpp` / `.h` |
| SS / SSH | `ObjectDLL/BloodServerShell.cpp` / `.h` |
| PO | `ObjectDLL/PlayerObj.cpp` |
| INV | `ObjectDLL/InventoryMgr.cpp` |
| SD | `Shared/SharedDefs.h` |
| ET | `ObjectDLL/ExitTrigger.cpp` |

Evidence classes: **SRC** game source; **SDK** 1998 `AppHeaders`; **RET** a local retail 2.1 installation (its readme and one real `Save/Current`; private data, not committed); **LT2** the later LithTech runtime at `lithtech/runtime` (not the 1998 engine, used only where the 1998 engine is opaque); **INF** an inference from code that was not executed.

---

## 1. Storage: slots, files and metadata

### 1.1 Slot ids and directories (SRC)

| Slot | Constant | Directory | Contents |
|---|---|---|---|
| -2 | `SLOT_CURRENT` (LSH:23) | `Save\Current` | the **only directory the server writes**: `Auto.sav`, `Current.sav`, `Keep.sav`, plus the client's `Save.dat` |
| -1 | `SLOT_QUICK` (LSH:24) | `Save\Quick` | a full copy of `Save\Current` plus `Save.dat` |
| 0..7 | `MAX_SAVESLOTS 8` (LSH:20) | `Save\Save0` .. `Save\Save7` | a full copy of `Save\Current` plus `Save.dat` |
| -255 | `SLOT_NONE` (LSH:25) | (none) | the "no reserved slot" sentinel, used by autosave |

- Server paths: `g_szSavePath "Save\\Current\\"` (SS:41), `Auto.sav` (SS:42), `Current.sav` (SS:43), `Keep.sav` (SS:1150-1151, 1198-1199). Under `_DEMO`, `Keep.sav` is written to the current working directory instead (SS:1152-1154, 1200-1202).
- Client paths: `LoadInfo` uses `Save\Quick`, `Save\Current` and `Save\Save%d` (LS:87-92). `SaveInfo`, `ClearSaveSlot` and `CopySlot` use upper-case spellings `SAVE\…` (LS:171, 179, 187, 253-257, 312-341). Windows paths are case-insensitive.
- A slot is a **whole-directory copy**: `CopySlot` clears the destination, then copies every regular, non-read-only file (LS:298-414; `ClearSaveSlot` is at LS:245-287).
- RET: the retail `Save/Current` holds `Auto.sav` (67,482 B) and `Save.dat` (212 B), with no `Quick` or `SaveN` directory.

### 1.2 `Save.dat` = raw `SavedInfo` dump (LSH:28-34, LS:206-232)

| Offset | Field | Size |
|---|---|---|
| 0 | `szName[MAX_SAVENAME+1]` (LSH:21: 100) | 101 |
| 101 | `szCurrentLevel[MAX_CS_FILENAME_LEN+1]` (`basedefs_de.h:74`: 100) | 101 |
| 202 | padding | 2 |
| 204 | `int nGameType` | 4 |
| 208 | `int nCharacter` | 4 |
| = 212 | | |

- The file has no version, checksum, difficulty, date field or screenshot.
- RET confirms the layout:
  - `szName` = "Cabalco Transit System".
  - `szCurrentLevel` = "01_SubwayA", followed by stale bytes "aleb1_cc_c". They are left over from an older, longer value ("worlds_ao\caleb1_cc_c") because `_mbscpy` does not clear the tail.
  - `nGameType` = 8 (`GAMETYPE_ACTION`, SD:532).
  - `nCharacter` = 3 (`CHARACTER_GABREILLA`, SD:346).

### 1.3 Metadata semantics (SRC)

- **Current slot name** = the world title, taken from the DAT world-info `worldname` token (`GetWorldTitle`, CS:7116-…). It is set at `SMSG_LOADWORLD_ACK` through `SetSlotName(SLOT_CURRENT, title)` (CS:5135-5136, LS:449-453), and only when a title exists.
- **Quick and numbered slot name** = `gCurrentSaveInfo.szName + " [Mmm dd hh:mm:ss]"` (the 15 characters of `ctime` after offset 4) (LS:429-447). There is no user-typed description.
- `szCurrentLevel` = the client's `m_szFilename`, a world path relative to `Worlds\` (add-on levels use `worlds_ao\…`). `nGameType` = the client's `m_nGameType`. `nCharacter` = `m_Config.nCharacter` (CSH:258).
- Slot info is read once, in the shell constructor (`LoadInfo`, CS:459; the loop over -2, -1, 0..7 is LS:71-132), and then kept in memory and updated as saves happen. Empty slots read "<EMPTY SLOT>" (LS:25, 41).
- `_DEMO` builds: `LoadInfo` and `SaveInfo` return immediately (LS:67-69, 152-154).
- Menu strings: "RESTART: %1!s!" and "QUICKLOAD: %1!s!" (`ClientRes/ClientRes.rc:1101-1102`).
- Quirks:
  - The constructor's `memset` has its arguments swapped, so it zeroes nothing (LS:40, 43, 45).
  - Range checks use `<= MAX_SAVESLOTS`, which admits index 8 and overflows the array (LS:169, 252, 310, 329, 429).
  - `LoadInfo` copies the buffer even when `ReadFile` failed (LS:114-128).
  - **`PrintError` has an inverted null check**: it returns whenever a shell exists, so every LoadSave error is silent (`ClientUtilities.cpp:73-75`).

### 1.4 Engine save file (format opaque in the 1998 SDK); RET sample `Save/Current/Auto.sav`

- Header: dword0 = 1113 (meaning unknown, possibly a version), dword1 = **186 = object count** (verified by walking the record chain), dword3 = float 21.372 (INF: the game time).
- Game console block at 0x38: repeated `[u8 more=1][u16 len][name][u16 len][value]`, terminated by a 0 byte. It holds 39 variables, including `GAME_GAMETYPE=8`, `GAME_DIFFICULTY=0`, `GAME_WORLDNAME=Worlds\01_SubwayA`, `GAME_GOTOSTARTPOINT=1`, `GAME_DOAUTOSAVE=1`, `GAME_REVISITING=0`, `playerpos=0`, `LeashLen=450` and the world-info variables (`WorldName`, `MusicDirectory`, `InstrumentFiles`, …).
- Object records: `[u32 nextPos][u32 dataPos][u16 len][class]…`, then the terminator `0xFFFFFFFF`, then 745 trailing bytes (INF: attachments and interlinks, as in LT2).
- Classes in the sample: 103 CDestructableBrush, 15 Trigger, 12 GlowingLight, 12 DetailSprite, 10 CultistAI, 9 CClientExplosionSFX, 7 Door, 5 CivilianAI, 4 KeyFramer, 4 Damage, and one each of Conveyor, Gideon, ObjectivesTrigger, ExitTrigger and CPlayerObj.
- The CPlayerObj record is 816 B, of which 595 B is game payload. It contains the inventory sentinel `0xCECECECE` (INV:2851).
- The structure resembles LT2 (`server/src/game_serialize.cpp:1091-1203`), but LT2 writes version 2001/2002, not 1113. **Do not assume LT2 field order for the 1998 format.**
- Per-object payload limit: `MAX_SAVEOBJECT_LEN 8192` (`basedefs_de.h:84-85`).

---

## 2. Message ids and payloads

Numeric ids are computed from the SD enums (SMSG starts at 100 at SD:29; CMSG starts at 100 at SD:113).

| Id | Name | Direction | Payload | Sent at | Handled at |
|---|---|---|---|---|---|
| 108 | `CMSG_LOADWORLD` | C→S | `string world; u8 gameType; u8 loadType; u8 difficulty` | CS:6312-6318 | SS:390-399 → `LoadWorld` SS:828 |
| 143 | `SMSG_LOADWORLD_ACK` | S→**all** | `u8 success` (sent whether it succeeded or failed) | SS:395-397 | CS:5128-5148 |
| 109 | `CMSG_SAVEGAME` | C→S | `string m_szFilename` (**the server reads and ignores it**); `u8 saveType` (0 = AUTO, 1 = CURRENT); nested client blob (`WriteToMessageHMessageWrite`) | CS:8443-8448, CS:6351-6381 | SS:401-431 |
| 142 | `SMSG_SAVEGAME_ACK` | S→**all** | empty; sent **only on success**; does not say which save type | SS:416-420, 424-428 | CS:5102-5125 (acts only in `GS_SAVING`) |
| 107 | `CMSG_AUTOSAVE` | C→S | empty | CS:5387-5389 | SS:500-519 (`#ifndef _DEMO`) |
| 165 | `SMSG_DOAUTOSAVE` | S→sender | empty; sent only if `GAME_DOAUTOSAVE` is non-zero | SS:504-507 | CS:5360-5364 |
| 144 | `SMSG_CLIENTSAVEDATA` | S→**all** | the nested blob from the save file | PO:6817-6837 (inside `CPlayerObj::Load`) | CS:5150-5154 → `LoadClientData` CS:6391 |
| 148 | `SMSG_PLAYERINIT_SINGLEPLAYER` | S→client | empty; guaranteed and naggled | SS:757-778 | CS:5156-5160 → `InitSinglePlayer` CS:5378 |
| 147 | `SMSG_PLAYERINIT_MULTIPLAYER` | S→client | `u8 type, ff, negTeamFrags, onlyFlag, onlyGoal` | SS:767-774 | CS:5162-5175 |
| 100 | `CMSG_SINGLEPLAYER_INIT` | C→S | `HString name; u8 char, skin, str, spd, res, mag; u8 weaponSlot[1..9]` | CS:1922-2050 (**returns early unless `m_nLastLoadType == NEW_GAME`**, CS:1931) | SS:490-498 → `CPlayerObj::OnMessage` |
| 141 | `SMSG_FORCEROTATION` | S→client | `f32 pitch, f32 yaw` | PO:3363-3366 (sent from SS:789) | CS:5082-5087 |
| 104 | `SMSG_EYELEVEL` | S→client | `f32 m_fEyeLevel` | PO:3269-3271 (sent from SS:790) | CS:4191-4193 |
| 139 | `SMSG_EXITWORLD` | S→all | `HString nextWorld; u8 exitType` | ET:198-201 | CS:4915-4921 → `HandleExitWorld` CS:6222 |
| 106 | `CMSG_GAME_PAUSE` | C→S | `u8 pause`; not sent in multiplayer | CS:5681-5686 | SS:459-471 (sets/clears `SS_PAUSED`) |
| 120 | `CMSG_NEXTLEVEL` | C→S | empty; host only | CS:200-215 | SS:339-343 (multiplayer `Update` only; SS:1458) |
| 110 / 140 | `CMSG_RESTOREGAME` / `SMSG_WORLDINFO` | | **defined, never used** | SD:123 / SD:69 | none |

### 2.1 Client blob (`SaveClientData` CS:6351-6381; `LoadClientData` CS:6391-6431)

| # | Type | Field | On load |
|---|---|---|---|
| 1 | u8 | `m_bNightGogglesActive` | **read into a local and discarded** (CS:6409) |
| 2 | f32 | `m_fWonkyTime` (remaining seconds, decremented at CS:2574) | restored |
| 3 | u8 | `m_bWonkyVision` | restored |
| 4 | u8 | `m_bWonkyNoMove` | restored |
| 5 | f32 | `m_fBlindTime` (an **absolute** client time, `GetTime()+dur` at CS:4984; never rebased) | restored |
| 6 | u8 | `m_bBlind` | restored |
| 7-8 | u8 | `m_bFadeIn`, `m_bFadeOut` | restored |
| 9 | f32 | `m_fFadeVal` | restored |
| (none) | u8 | `m_nCrosshair` | commented out (CS:6370, 6418) |
| 10 | u8 | `m_eMusicLevel` (refreshed from `m_Music` just before the write, CS:6357) | restored |
| 11 | f32 | `(float)m_nLastLoadType` | **read and discarded** (CS:6420) |
| 12 | f32 | `(float)m_nGameType` | restored (CS:6421) |
| 13-14 | HString | `m_hstrObjectivesTitle`, `m_hstrObjectivesText` | restored; `UpdateObjectives` runs if both are non-null (CS:6425-6426) |

After reading, `LoadClientData` also runs `maxfps 0` (CS:6430).

**Blob ownership:**
1. The server reads the blob with `ReadFromMessageHMessageRead` (SS:407).
2. `CPlayerObj::SetClientSaveData` takes it, freeing any older blob (PO:5905-5913).
3. `CPlayerObj::Save` writes a flag byte and the blob, frees it and clears the pointer (PO:6675-6682). The blob is therefore consumed by the **first** `SaveObjects` that includes the player, whether that is a full save or a keepalive.
4. `CPlayerObj::Load` re-broadcasts the blob to **all** clients, because it does not know the `HCLIENT` yet; its comment says only single-player saves are supported (PO:6825-6828).

If the sender has no player, the blob is never released (INF: a leak).

The nested-message API is SDK `CSBase.h:18-19, 36-42, 73-84`.

---

## 3. Save path (manual slot and quicksave)

### 3.1 Manual save from the menu

1. **F2** (CS:3846-3856) is accepted only in `GS_PLAYING` and only when `!_DEMO`. It calls `PauseGame(TRUE)` (which sends `CMSG_GAME_PAUSE 1`), switches to `GS_MENU` and opens `MENU_ID_SAVE_GAME`. The other route is Esc → Single Player → Save Game. That menu item is enabled only if the client is in a world, not in multiplayer and not dead (`MainMenus.cpp:806-817`; `MenuSinglePlayer.cpp:47, 55-60, 114-116`). **F2 skips that guard.**
2. `CMenuSaveGame::InitSavedGameSlots` lists the 8 slot names (`MenuSaveGame.cpp:37-57`). Selecting row *i* calls `MenuSaveGame(i)` (`MenuSaveGame.cpp:63-69`), whose type defaults to `SAVETYPE_CURRENT` (CSH:191). The return value is ignored.
3. `MenuSaveGame` (CS:8413-8454):
   - It returns FALSE silently when `!m_bInWorld || m_bDead` (CS:8417), or when the slot is outside -1..7 (CS:8424).
   - `SetReservedSlot(i)`, then `SetGameState(GS_SAVING)`.
   - It sends `CMSG_SAVEGAME [m_szFilename, SAVETYPE_CURRENT, blob]`.
4. Server (SS:401-431):
   - Read the payload and attach the blob to the sender's player.
   - Call `SaveGame(SAVETYPE_CURRENT, bSavePlayers=TRUE, bSaveConsole=TRUE)`.
5. `SaveGame` (SS:1004-1064):
   - Path: `Save\Current\Current.sav`.
   - Object list: **every active object, then every inactive object**, that has `USRFLG_SAVEABLE`. Players are included because `bSavePlayers` is TRUE. `USRFLG_SAVEABLE` is bit 3 (`ClientServerShared.h:18`). Characters set it in their non-savegame `MID_INITIALUPDATE` (`BaseCharacter.cpp:77-89`), and 26 object files set it in total. The engine stores user flags in the save, so restored objects keep the flag.
   - Flags: `SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS` = 1|2 (SS:1053-1056; SDK `server_de.h:29-32`).
   - Call `SaveObjects(path, list, dwParam=0, flags)`. Each object then receives `MID_SAVEOBJECT` with `fData=0` (SDK `server_de.h:875-879`, `serverobj_de.h:114-119`). Aggregates such as `CInventoryMgr` (INV:2542-2548) and `CDestructable` (`Destructable.cpp:164-168`) serialize themselves.
   - The engine never writes `CF_ALWAYSLOAD` classes (SDK `serverobj_de.h:62-63`; LT2 `game_serialize.cpp:1123-1125`).
   - Finally `RelinquishList`.
6. If `SaveObjects` returns `DE_OK`, the server broadcasts `SMSG_SAVEGAME_ACK`. Otherwise it sends **nothing**.
7. Client `SMSG_SAVEGAME_ACK` (CS:5102-5125) does something only in `GS_SAVING`:
   1. Copy `m_szFilename` into the reserved slot's `szCurrentLevel`.
   2. If the slot is not `SLOT_NONE`:
      - `CopySlot(SLOT_CURRENT, slot)`: copy the in-memory `gCurrentSaveInfo` over the slot info, create both directories, clear the destination, then copy every file in `Save\Current` (Auto, Current, Keep and Save.dat).
      - `SetSlotName(slot)`: the world title plus a timestamp.
      - `SaveInfo(slot, m_nGameType, GetCharacter())`: rewrite the slot's `Save.dat` (LS:145-233).
   3. `SetReservedSlot(SLOT_NONE)`, `SetGameState(GS_PLAYING)`, `PauseGame(FALSE)`.

### 3.2 Quicksave

- **F5** (CS:3877-3886) requires `GS_PLAYING && m_bInWorld && !m_bDead && !IsMultiplayerGame()` and `!_DEMO`.
- It calls `MenuSaveGame(SLOT_QUICK)` **without pausing**, then `DoMessageBox("Quicksave...", bAsync=FALSE)`. The message box is drawn synchronously for two `PostUpdate` frames and then hidden (CS:8842-8869). It appears **whatever the outcome**.
- Steps 3-7 of 3.1 then follow, with slot -1 (`Save\Quick`).
- The client saves nothing besides the blob and `Save.dat`. HUD, camera, inventory display and settings are not saved client-side; the server re-sends them (section 7).

---

## 4. Autosave: the "Restart" source

1. Server `LoadWorld` sets `GAME_DOAUTOSAVE = (!bRestoring || bRevisiting) ? "1" : "0"`. In practice it is "1" for `NEW_GAME` and `NEW_LEVEL` and "0" for both restore types; `bRevisiting` is always FALSE (SS:980-982).
2. After `SMSG_PLAYERINIT_SINGLEPLAYER`, `InitSinglePlayer` **always** sends `CMSG_AUTOSAVE` (CS:5387-5389), then switches to `GS_WAITING` if paused or `GS_PLAYING` otherwise (CS:5391-5392).
3. The server answers with `SMSG_DOAUTOSAVE` to the sender only if `GAME_DOAUTOSAVE` is set (SS:504-507). An older direct server-side `SaveGame` is commented out (SS:509-514).
4. The client calls `MenuSaveGame(SLOT_NONE, SAVETYPE_AUTO)` (CS:5362). The in-world and not-dead guards still apply. It does **not** enter `GS_SAVING` and reserves no slot. It sends `CMSG_SAVEGAME [file, 0, blob]`.
5. The server runs `SaveGame(SAVETYPE_AUTO, TRUE, TRUE)` → `Save\Current\Auto.sav` and broadcasts the ACK. The client ignores the ACK because it is not in `GS_SAVING`.

Consequences:
- `Auto.sav` = a snapshot taken right after level entry: the keepalive-carried player already at the start point, after single-player init.
- The snapshot is taken while the loading-screen pause is active (`StartNewWorld` → `PauseGame(m_nLoadScreenID)`, CS:6305-6306) (INF).
- RET: `GAME_GOTOSTARTPOINT=1` and `GAME_DOAUTOSAVE=1` are captured inside `Auto.sav`. `RespawnPlayer` had already reset the first to "1" (SS:794); the restore path overrides both (section 5, step 4).

---

## 5. Load / restore path

### 5.1 Entry points

- **Load menu** (opened with F3 at CS:3857-3867, or from the Single Player menu). Rows (`MenuLoadGame.cpp:37-95`):
  - row 0, "RESTART: \<current name\>" → `MenuLoadGame(SLOT_CURRENT)`;
  - row 1, "QUICKLOAD: \<quick name\>" → `MenuLoadGame(SLOT_QUICK)`;
  - rows 2..9 → `MenuLoadGame(row-2)`.
- **F6** (CS:3887-3896), when `GS_PLAYING && !IsMultiplayerGame()`, calls `MenuLoadGame(SLOT_QUICK)`. If that returns TRUE, it shows `DoMessageBox("Quickload...")`.
- **Single-player death**: USE while dead switches to `GS_MENU`, pauses and opens the **Load Game** menu (CS:3488-3526). `_DEMO` opens the main menu instead.
- The `load <level>` console command (CS:217-242) is **not** a restore. In a world it becomes `StartNewWorld(…, LOADTYPE_NEW_LEVEL)`, a keepalive transfer to any level. Outside a world it starts a new custom game.

### 5.2 Client side

1. `MenuLoadGame(nSlot)` (CS:8325-8402):
   1. Take `szCurrentLevel`, `nGameType` and `nCharacter` from the in-memory slot info. Return FALSE silently if the level is empty (CS:8365-8368).
   2. **If the slot is not `SLOT_CURRENT`, call `CopySlot(nSlot, SLOT_CURRENT)`.** This wipes `Save\Current` and copies the slot in, including its `Auto.sav`, `Current.sav`, `Keep.sav` and `Save.dat`. It happens **before** any load attempt, with no rollback (CS:8372-8375).
   3. `loadType` = `LOADTYPE_RESTOREAUTOSAVE` (4) for `SLOT_CURRENT`, otherwise `LOADTYPE_RESTORESAVE` (3) (CS:8380; SD:538-541).
   4. `SetCharacterInfo(nCharacter, MULTIPLAY_SKIN_NORMAL)`; return FALSE if that fails.
   5. Clamp the game type to SINGLE, ACTION or CUSTOM; anything else becomes SINGLE (CS:8393-8396).
   6. `StartNewGame(m_szFilename, STARTGAME_NORMAL, gameType, loadType, DIFFICULTY_MEDIUM)`. **The difficulty argument is ignored on restore** (see 5.3, step 4).
   7. `m_bFirstWorld` is left unchanged.
2. `StartNewGame` (CS:3910-3964):
   1. Reset client fields: current gun, spectator flag, eye level, zoom, Seeing Eye and Orb handles; kill the goggles and Eye sounds; `m_bNightGogglesActive` and binoculars off; crosshair reread from the `CrosshairNum` console variable; first-person camera.
   2. `StartGame({m_Type=STARTGAME_NORMAL})`. The return value is ignored (CS:3950-3952). Per the SDK, `StartGame` keeps the old game running until the new one has started (`client_de.h:122-125`). INF: this replaces the local server session and calls the client's `OnExitWorld` for the old world.
   3. `VoiceMgr.Reset`; `m_eMusicLevel = SILENCE`.
   4. `StartNewWorld`.
3. `StartNewWorld` (CS:6291-6341):
   1. `GetWorldTitle` reads the title, loading screen and `loadscreenID`.
   2. `PauseGame(loadScreenID != 0)`.
   3. `m_NewStatusBar.InitObjectivesTab()` clears the objectives display.
   4. Send `CMSG_LOADWORLD`.
   5. Switch to `GS_LOADINGLEVEL`; `m_nGameType = gameType`; `m_nLastLoadType = loadType`; `m_bInWorld = FALSE`; zoom off; crosshair reset; `m_bBlind = FALSE`; `m_nTrapped = 0`.

### 5.3 Server `LoadWorld` for `RESTORESAVE` / `RESTOREAUTOSAVE` (SS:828-993)

1. Read the payload and call `CultistAI::ResetStatics()`. Return FALSE if the filename is empty. Strip leading slashes; `VoiceMgr` init and `StopAll`.
2. `GAME_WORLDNAME` is first set from the **uninitialized** `szPath` (SS:862, a source bug), then set again to `Worlds\<file>` (add-on: the raw `worlds_ao` path) (SS:864-886).
3. `SetGameInfo(type, difficulty)` runs **only for `NEW_GAME`** (SS:888-893, 1219-1230). On restore, `GAME_GAMETYPE` and `GAME_DIFFICULTY` come back from the saved console block. RET shows both variables in `Auto.sav`; LT2 restores console variables unconditionally (`game_serialize.cpp:1206-1229, 1277`).
4. `RESTOREAUTOSAVE` sets `pFilename = "Auto.sav"`. That value is used **only** for the length check and the error text, because the world path was already built. Both restore types set `bRestoring` (SS:895-905).
5. `LoadWorld(szPath, LOADWORLD_NORELOADGEOMETRY)` (SS:937-958):
   - **There is no `LOADWORLD_LOADWORLDOBJECTS`**, so only `CF_ALWAYSLOAD` classes come from the DAT (SDK `serverobj_de.h:62-63`; LT2 `s_object.cpp:396`).
   - There is no `LOADWORLD_RUNWORLD`. `NORELOADGEOMETRY` is also OR-ed in a second time (SS:950).
   - The engine calls `PreStartWorld(bSwitching)` (SS:555-571): reset the team manager, `SetupGameInfo`, `g_bNextLevel = 0`, `g_hActiveCamera = NULL`.
   - On failure: `BPrint("Error loading world %s: %d")` and return FALSE. No restore is attempted.
6. `RestoreGame(AUTO | CURRENT)` (SS:1075-1094) calls `RestoreObjects("Save\Current\Auto.sav" or "Current.sav", dwParam=0, RESTOREOBJECTS_RESTORETIME)`. **Its return value is ignored** (SS:959-966). During the restore:
   - Each object gets `PRECREATE_SAVEGAME`, `INITIALUPDATE_SAVEGAME` and `MID_LOADOBJECT(fData=0)` (SDK `serverobj_de.h:70, 75, 114-118`).
   - The engine registers client refs for objects that had a client (SDK `server_de.h:408-416`).
   - `CPlayerObj::InitialUpdate(SAVEGAME)` initialises the inventory and damage aggregates and links the player into the AI lists, sets `m_bFirstUpdate = TRUE`, and returns early (PO:590-644).
   - `CPlayerObj::Load` (PO:6694-6863) restores relative clocks as `saved + GetTime()`; zeroes `m_nLast*` to force stat resends; resets the slow-death timers; calls `SetCharacter`; resets nodes and flags; zeroes velocity; forces `m_bZoomView` off (PO:6855-6857); and **broadcasts `SMSG_CLIENTSAVEDATA`** if a blob is present.
7. There is no keepalive step.
8. Set `GAME_GOTOSTARTPOINT = "0"`, `GAME_REVISITING = "0"` and `GAME_DOAUTOSAVE = "0"` (SS:974-982). These run **after** the restore, so they override the restored console values.
9. `RunWorld()` (SS:984-990). The engine calls `PostStartWorld` (SS:582-594: `PathMgr.BuildPathList`, `VoiceMgr` init and `StopAll`; LT2 `servermgr.cpp` `DoRunWorld`). On failure: `BPrint("Error running world")` and return FALSE.
10. `SMSG_LOADWORLD_ACK(bResult)` to all clients (SS:394-397).

### 5.4 Client entry and player reattachment

1. `SMSG_CLIENTSAVEDATA` → `LoadClientData` (it was queued before the ACK).
2. `SMSG_LOADWORLD_ACK`:
   - **1**: set the current slot's name to the title and its level to `m_szFilename`, then `SaveInfo(SLOT_CURRENT)` rewrites `Save\Current\Save.dat` (CS:5132-5141).
   - **0**: `SetGameState(GS_MENU)` with no message (CS:5142-5146).
3. The engine loads the world on the client:
   - `PreLoadWorld` (CS:2803-2926): loading screen, team manager, voice `StopAll`, credits flag. The add-on inventory reset (`g_bPlayerInvReset`) is cancelled on restore (CS:2915-2918).
   - `OnEnterWorld` (CS:1504-1640).
4. Server `OnClientEnterWorld(hClient, pClientData, len)` (SS:192-291):
   1. Walk the client refs (`GetNextClientRef`). The **first ref with `CIF_LOCAL`**, or else a ref whose client name matches (`_mbsicmp`), gives `GetClientRefObject` → `RespawnPlayer(obj, hClient)`.
   2. Team assignment. `pClientData` is used only for `NetClientData.m_dwTeam` in team games (SS:244-276).
   3. If no ref matched: `CreatePlayer` (SS:705-735: create, `SetClient`, `Respawn`, `UpdateTeamID`; no start point) followed by `RespawnPlayer`.
   4. `AddClientToList`.
   - In single player the name is empty (CS:1613-1618), so in practice `CIF_LOCAL` is what matches.
   - LT2: the engine deletes the consumed client ref (`s_client.cpp:450-466`). Unclaimed client-ref objects are removed at the next world end (`servermgr.cpp:2738-2746`).
5. `RespawnPlayer` (SS:746-795):
   1. `SetClient`, `SetClientUserData`.
   2. Send `SMSG_PLAYERINIT_SINGLEPLAYER` when `m_GameInfo.m_byType == GAMETYPE_SINGLE`. A local game has no `NetGame`, so the type is 0.
   3. **If `IsMultiplayerGame() || GAME_GOTOSTARTPOINT` → `GoToStartPoint()`. Otherwise → `SMSG_FORCEROTATION` + `SMSG_EYELEVEL`.**
   4. Then `GAME_GOTOSTARTPOINT = "1"`.
6. `InitSinglePlayer` (CS:5378-5395):
   - `SendPlayerInitMessage` is attempted only if `m_bFirstWorld`, and it returns early anyway because the load type is not `NEW_GAME` (CS:1931). **No stats are re-initialised.**
   - `CMSG_AUTOSAVE` is sent and ignored (`DOAUTOSAVE = 0`). `GS_WAITING` or `GS_PLAYING`; input on.
7. The player's first server update (PO:1591-1604) sets move and jump velocities, runs `UpdateClientPhysics` and `TeleportClientToServerPos`, and sends `SMSG_PLAYERSTATSUPDATE` with all flags.

**Ordering note:**
- The server queues `SMSG_CLIENTSAVEDATA` and the ACK inside the `CMSG_LOADWORLD` handler, before the client is put into the world.
- LT2 sends shell messages addressed to all clients regardless of client state (`serverde_impl.cpp:1214-1240`). The engine's world-load packet then drives `DoLoadWorld`, which ends in `OnEnterWorld` (`client/src/clientshell.cpp:850-851`).
- Probable sequence (INF, not verified on the 1998 engine):
  1. `OnExitWorld` for the old world
  2. `LoadClientData`
  3. `LOADWORLD_ACK`
  4. `PreLoadWorld`
  5. `OnEnterWorld`
  6. `PLAYERINIT`, `FORCEROTATION`, `EYELEVEL`
  7. first update stats

### 5.5 Restart (= the Restart row, `SLOT_CURRENT`)

- `MenuLoadGame(SLOT_CURRENT)` takes the level, type and character from `gCurrentSaveInfo` and does **no `CopySlot`**. It uses `LOADTYPE_RESTOREAUTOSAVE`, so the server reads `Save\Current\Auto.sav`.
- `GOTOSTARTPOINT = 0`, so the position comes from the autosave, and that position is already at the start point.
- `DOAUTOSAVE = 0`, so `Auto.sav` is preserved and Restart can be repeated.
- After loading slot N, `Auto.sav` is slot N's copy, so Restart means "the start of slot N's level". This holds because `Auto.sav` travels with `Current.sav`.
- Restart is **not** a fresh level entry: it restores the player and inventory as they were carried into the level.
- `ENDWORLD_RESTART 3` (`ClientServerShared.h:45`) is defined but never used. `m_nLastExitType` is stored and never read (CS:6256).

---

## 6. KeepAlive (`ExitTrigger` → `NEW_LEVEL`) and its interaction with full saves

### 6.1 Transition sequence

1. `ExitTrigger::SendMessages` (ET:188-202):
   - `g_pBloodServerShell->SetStartPointName(name)` (SS:806-816). The name is **stored only in the server shell and never saved**.
   - Broadcast `SMSG_EXITWORLD [nextWorld, exitType]`.
2. Client (CS:4915-4921) → `HandleExitWorld` (CS:6222-6280):
   1. Ignore the message if `m_bDead`.
   2. Stop the music, container sound, goggles and Eye sounds; `m_eMusicLevel = SILENCE`.
   3. `m_szFilename = next`; `StartNewWorld(next, m_nGameType, LOADTYPE_NEW_LEVEL)` (difficulty defaults to EASY at CSH:146 and is ignored); `m_bFirstWorld = FALSE`.
   4. The caller then resets the status bar inventory tabs (CS:4920).
   5. **There is no `StartGame`**, so the server session, its console and the shell members persist.
3. Server `LoadWorld` with `NEW_LEVEL` (SS:906-935):
   1. `DoLevelChangeCharacterCheck` (add-on only; SS:2086-…). It may set `g_bLevelChangeCharacter`, which makes `CPlayerObj::Save` call `TweakSaveValuesForCharacterChange` (PO:6549-6555).
   2. **`KeepAliveSave`** (SS:1142-1180):
      - The list is every **active** object for which `IsPlayer` is true (`ObjectUtilities.h:70-77`). There is no `USRFLG_SAVEABLE` check and no inactive pass.
      - `SaveObjects("Save\Current\Keep.sav", list, dwParam=1, flags=0)`: no console, no portals.
      - The result is ignored (SS:913).
   3. The per-level "revisit" save is commented out (Greg 10/5; SS:916-934).
4. `LoadWorld(szPath, LOADWORLDOBJECTS | NORELOADGEOMETRY)`:
   - The old world, including the old player object, ends.
   - The new DAT objects are created and filtered by `B2BaseClass` against the **live** `GAME_DIFFICULTY` and `GAME_GAMETYPE` (`B2BaseClass.h:42-54`, `B2BaseClass.cpp:178-…`).
5. **`KeepAliveLoad`** (SS:1191-1208):
   - `RestoreObjects(Keep.sav, dwParam=1, flags=0)`. There is **no `RESTORETIME`**: the new world's clock starts again (LT2 `servermgr.cpp` `DoStartWorld` sets `m_GameTime = 0`), and the player's relative clocks are rebased by `Load`'s `+GetTime()`.
   - The result is ignored (SS:971).
   - **The only consumer of `dwParam` is `CInventoryMgr::Load`, which deletes `INV_KEY` items when `dwLoadType == 1` (INV:3024-3028).** Every other `Save`/`Load` ignores the parameter, and the `CPlayerObj` branch is commented out (PO:6861-6862).
   - `SMSG_CLIENTSAVEDATA` is sent only if a blob was still pending. Normally the previous level's autosave already consumed it, so none is sent (INF).
6. `GOTOSTARTPOINT = "1"` (because `bRestoringKeepAlive`), `REVISITING = "0"`, `DOAUTOSAVE = "1"`. Then `RunWorld` and `ACK(1)`.
7. Client: the ACK updates the current slot's `Save.dat` (new title and level). `OnEnterWorld` runs with `m_bFirstWorld = FALSE`, so cheats and the gun are not reset.
8. `OnClientEnterWorld` finds the keepalive client ref (`CIF_LOCAL`) → `RespawnPlayer` → `SMSG_PLAYERINIT_SINGLEPLAYER` + `GoToStartPoint()` (the named marker from `m_hstrStartPointName`, SS:782-785) → `GOTOSTARTPOINT = "1"`.
9. `InitSinglePlayer` → `CMSG_AUTOSAVE` → a new `Auto.sav` for the new level (section 4).

### 6.2 Interplay

- After a transition, `Save\Current\Current.sav` is **stale** (from the previous level) until the next manual save or quicksave. It is read only after a `CopySlot` has replaced it, so it does no harm.
- **`Keep.sav` is copied into every slot** (because `CopySlot` copies every file), but no restore ever reads it: the `RESTORE*` load types never set `bRestoringKeepAlive`. The next `NEW_LEVEL` overwrites it before reading it.
- A save made right after a transition is a normal full world in `Current.sav`: the new level's objects plus the keepalive player at its start point, as an ordinary saved object with a client ref and the client blob. Nothing in it depends on keepalive.
- What each file contains:

  | File | Objects | Console state | Client blob |
  |---|---|---|---|
  | Full save | all `SAVEABLE` objects, players included | yes | yes |
  | Keepalive | players only | no | normally none |

- The start-point name is server-shell state. A full restore does not need it (`GOTOSTARTPOINT = 0`). After a load through `StartGame`, the server session is new (INF), so the name is NULL until the next `ExitTrigger`.
- **New game** (`MenuNewGame` CS:8151-8185 and `MenuNewNightmaresGame` CS:8196-8226) calls `ClearSaveSlot(SLOT_CURRENT)` first. That deletes Auto, Current, Keep and Save.dat; the slots are untouched. `MenuLoadCustomLevel` (CS:8465-8481), the `runworld` startup path (CS:1337-1343) and Credits (CS:8297) do **not** clear `Save\Current` (INF: a stale `Current.sav` or `Keep.sav` remains until it is overwritten).
- Multiplayer level rotation (SS:1890-1947) calls the engine's `LoadWorld(LOADWORLDOBJECTS | RUNWORLD)` directly. There is no keepalive and no autosave; players are recreated and teams are restored through `CreateTeamTransIDs` and `GetTeamTransID` (SS:267-273).

---

## 7. Client-side state across save and restore

| State | Saved? | On restore | Citations |
|---|---|---|---|
| Objectives | blob title and text (single player and custom only, CS:4479-4510) | `InitObjectivesTab` in `StartNewWorld`, then `UpdateObjectives` from the blob. `OnExitWorld` frees the strings. | CS:6309, 6425-6426, 1721-1730 |
| HUD / status bar | no | `OnEnterWorld` → `m_NewStatusBar.Reset()` (health 100, armor and ammo 0, weapon and inventory tabs cleared; the objectives surface is not touched). The server re-sends values because `m_nLast*` is zeroed and the first update sends `SMSG_PLAYERSTATSUPDATE`. | CS:1626; `NewStatusBar.cpp:1117-1140`; PO:6713-6717, 1597-1601 |
| Inventory / weapons display | no (server side only) | `OnExitWorld` clears weapon ids and view weapons; the server inventory restore drives them again (INF) | CS:1673-1690 |
| Cheats | god mode and infinite ammo in the server inventory | `m_CheatMgr.Reset()` only if `m_bFirstWorld` | INV:2853-2854; CS:1622 |
| Music | `m_eMusicLevel` | silence in `StartNewGame`; restored by the blob; `OnEnterWorld` plays the IMA level (or the CD list); no playlist position | CS:3957, 6419, 1631-1636 |
| Camera | no client field | pitch and yaw via `SMSG_FORCEROTATION`; eye level via `SMSG_EYELEVEL`, but the server's `m_fEyeLevel` is **not saved** (PO:6626, 6769); first person forced. The cinematic `CameraObj` is server-saved and re-activated (`CameraObj.cpp:610-660`). `CPlayerObj` writes `m_hCameraObj` with `WriteToMessageObject` inside `MID_SAVEOBJECT` (PO:6622/6765), which the SDK forbids (`CSBase.h:49-53`). | CS:3947-3948, 1578-1580 |
| Fade / wonky / blind | blob | `OnEnterWorld` resets fade (CS:1512) and wonky (CS:1549-1551). If the blob arrives first (the probable order), the restored fade and wonky are overwritten; **blind survives** (cleared in `StartNewWorld` before the blob, not touched by `OnEnterWorld`), with a non-rebased absolute `m_fBlindTime`. | CS:6331, 6413-6417 |
| Night goggles | blob byte | discarded; `StartNewGame` and `OnExitWorld` force it off | CS:3938, 1705, 6409 |
| Zoom / binoculars | no | cleared on the client (CS:3921, 3940, 6327) and the server (PO:6855-6857). Patch 2.0 note: binoculars after loading a zoomed save. | readme.txt:1174 |
| Crosshair | no (commented out) | reloaded from the `CrosshairNum` console variable | CS:3942-3945, 6328-6329 |
| Game type | `Save.dat` and blob | `StartNewWorld` sets it from slot info; the blob then overrides it | CS:6322, 6421 |
| Difficulty | server console variable only | from the restored console block | §5.3 step 4 |
| Character | `Save.dat` (client) and the player record (server) | `SetCharacterInfo` before the load; `CPlayerObj::Load` → `SetCharacter` | CS:8385; PO:6634/6777, 6841 |
| `m_bFirstWorld` | no | unchanged by loads | CS:457, 6279 |
| `m_nLastLoadType` | blob (discarded) | equals the requested restore type, which suppresses `SINGLEPLAYER_INIT` | CS:6323, 1931 |
| Pause | no | `StartNewWorld` pauses when the level has a load screen; `InitSinglePlayer` → `GS_WAITING` | CS:6305-6306, 5391 |
| Settings / console | client: none | client console and config files are untouched | |

---

## 8. Error handling (original behaviour)

| Failure | Behaviour | Citations |
|---|---|---|
| Save refused (not in a world, dead, bad slot) | `MenuSaveGame` returns FALSE; the menu ignores it; no UI | CS:8417, 8424; MenuSaveGame.cpp:68 |
| `SaveObjects` fails | **no ACK** → the client stays in `GS_SAVING`. `Update` has no `GS_SAVING` case and only renders (CS:2504-2738). `OnKeyDown` has no `GS_SAVING` branch, so Esc does nothing (CS:3674-3693), and F-keys require `GS_PLAYING`. **Soft lock**; a menu save also stays paused. | SS:416-429 |
| Sender has no player | save proceeds without a blob; the blob leaks (INF) | SS:409-411 |
| File copy, clear or info write fails | `PrintError` prints nothing; `CopySlot` aborts part-way. The ACK path still calls `SetSlotName` and `SaveInfo`, so slot metadata can describe missing or partial `.sav` files. | LS:275-280, 402-407; CS:5115-5117 |
| Load of an empty slot | `MenuLoadGame` returns FALSE silently; no "Quickload..." box | CS:8365-8368, CS:3892-3893 |
| `CopySlot` into `Current` before the load | `Save\Current` is wiped and replaced before success is known; no rollback | CS:8372-8375 |
| `StartGame` fails | ignored | CS:3952 |
| World load fails | server `BPrint` → `ACK(0)` → client `GS_MENU`, no message, `m_bInWorld` FALSE | SS:952-957; CS:5142-5146 |
| `RestoreObjects` fails | **ignored**. The world runs with only `CF_ALWAYSLOAD` objects. With no client ref, `CreatePlayer` makes a fresh player; `GOTOSTARTPOINT = 0` means no start point, only `FORCEROTATION`/`EYELEVEL`; the player stays at the create position (0,0,0) with default stats. The ACK still reports 1 (INF). LT2 `sm_RestoreObjects` keeps already-created objects (`game_serialize.cpp:1231-1345`). | SS:959-966, 705-735, 782-791 |
| `KeepAliveSave` / `KeepAliveLoad` fails | ignored → a fresh player at the start point; the carried inventory is lost (INF) | SS:913, 971 |
| `RunWorld` fails | `BPrint` → `ACK(0)` | SS:985-990 |
| Version mismatch | there is no game-level check. Retail 2.1 readme: "ALL SAVE GAME FILES FROM PREVIOUS VERSIONS WILL BE INCOMPATIBLE" | readme.txt:16-17; NGHTMARE.TXT:14 |
| ACK race | ACKs carry no type and go to every client. A `GS_SAVING` client could treat an autosave ACK as its own and copy `Save\Current` before `Current.sav` is written (INF; a narrow window at level entry) | SS:418, 426 |

---

## 9. Multiplayer and CTF guards

- **Menu Save item** is disabled in multiplayer (`MainMenus.cpp:809`).
- **F5 and F6** are disabled in multiplayer (CS:3879, 3889). This matches patch 2.0: "Quick Load/Save hotkeys are disabled in multiplayer now." (readme.txt:1249).
- **F2 and F3 are not guarded for multiplayer** (CS:3846-3867):
  - F2 opens the save menu, and `MenuSaveGame` has no multiplayer check. It sends `CMSG_SAVEGAME`; the server has no host or multiplayer check either (SS:401-431). INF: the host writes `Save\Current\Current.sav`, the ACK goes to everyone, and the requesting client copies its **own local** `Save\Current`.
  - F3 → load → `StartGame(STARTGAME_NORMAL)` leaves the session and starts a local single-player game.
- `CMSG_LOADWORLD` has no sender or host check (SS:390-399; INF: any client could trigger a world load).
- `CMSG_AUTOSAVE` is sent only from `InitSinglePlayer`; `InitMultiPlayer` does not send it (CS:5406-5416).
- `RespawnPlayer` always calls `GoToStartPoint` in multiplayer (SS:782).
- `PauseGame` sends no `CMSG_GAME_PAUSE` in multiplayer (CS:5681).
- Server `Update` returns immediately in single player (SS:1458).
- `IsMultiplayerGame` is true when the client game mode is neither `STARTGAME_NORMAL` nor `GAMEMODE_NONE` (CS:7676-7686). The server's `IsMultiplayerGame` is `m_GameInfo.m_byType != GAMETYPE_SINGLE` (SSH:38).
- `CPlayerObj::Load` notes that only single-player saves are supported; `SMSG_CLIENTSAVEDATA` is broadcast (PO:6825-6828).
- CTF / team: no save-specific code. Teams persist across multiplayer level changes through trans-ids only.
- `_DEMO` disables everything: the F2/F3/F5/F6 key handlers are compiled out with `#ifndef _DEMO`; `LoadInfo`/`SaveInfo` are no-ops; `CMSG_AUTOSAVE` is compiled out on the server; `Keep.sav` lives in the working directory; death + USE opens the main menu.

---

## 10. Retail notes and SDK-versus-code differences

### 10.1 Retail 2.1 readme (retail `readme.txt`)

- **:16-17**: saves from earlier versions are incompatible with 2.1 (also `NGHTMARE.TXT:14`).
- **:21-25**: known 18_Temple stuck bug; the workaround is "save your game, then reload".
- **:1170-1175** (patch 2.0, "Load/Save" section):
  - Fixed a number of load/save lockups, doors that would not open, and problems changing worlds.
  - Fixed the Action key not working after a reload.
  - The alternate-ammo tab is handled now, though the readme says a bug remains.
  - Fixed binoculars after loading a zoomed save (PO:6855-6857).
  - Fixed the Laundromat phone ring, which may also fix other load/save sound bugs.
- **:1166**: the current item icon no longer carries over into the next level.
- **:1249**: quick load and quick save are disabled in multiplayer.
- **:1157-1158, :1324-1326**: the unsupported `world` console command bypasses Blood2 setup (a 2.0 fix makes it set the starting position); 13.1 adds `Load <level-name>`, implemented at CS:217-242.
- Source vintage (INF): the source contains the 2.0 multiplayer F5/F6 guard and the `_ADDON` (Nightmares) code, so it matches the 2.1 / Nightmares era.

### 10.2 SDK versus code

- `SaveObjects`: "Flags is a SAVEOBJECTS_ define" (`server_de.h:877-879`). The code ORs two flags, 1|2 (SS:1056).
- `RestoreObjects` has a single restore flag, `RESTOREOBJECTS_RESTORETIME` (`server_de.h:29`). It is used for full restores; keepalive passes 0.
- `dwParam` reaches `MID_SAVEOBJECT` / `MID_LOADOBJECT` as `fData` (`server_de.h:877, 881`; `serverobj_de.h:114-118`). Blood2 uses 0 for a full save and 1 for keepalive; only `CInventoryMgr` reads it.
- `CF_ALWAYSLOAD` objects are "always loaded from the level file and can't be saved" (`serverobj_de.h:62-63`). This class flag is what makes restore without `LOADWORLD_LOADWORLDOBJECTS` work.
  - Blood2 classes with the flag: GameStartPoint, PathPoint, **Key**, SmellHint, ExitHint, WorldProperties, CameraSpot, Script, ParticleSystem, PolyGrid, SparksObj, LightFX, WreckingBall and SoccerGoal.
  - Engine classes (`engineobjects.c`): Model, WorldModel, Container, Sprite, Sound, StartPoint, World, Brush, DemoSkyWorldModel, SkyPointer and FogSphere.
  - Consequence: Key objects are recreated from the DAT on every restore, while KeyFramer restores its own key list (`KeyFramer.cpp:752, 814`).
- Client refs: the SDK says they "come from when a world is saved" (`server_de.h:408-416`).
  - `OnClientEnterWorld(pClientData, len)` (`servershell_de.h:32-35`) receives `StartGameRequest.m_pClientData` (`basedefs_de.h:743-746`). Blood2's single-player request is memset to zero (CS:3950), so that data is NULL.
  - The SDK has **no engine-level client save data**. Blood2 tunnels its client blob through the player's object record with nested messages and re-broadcasts it.
- Object references inside saves must use `WriteToLoadSaveMessageObject` / `ReadFromLoadSaveMessageObject` (`CSBase.h:49-53, 70-71`). `CPlayerObj::Save` breaks this rule for `m_hCameraObj` (PO:6622, 6765). `CameraObj` uses the correct calls (`CameraObj.cpp:628, 655`).
- Dead or unused code:
  - `CMSG_RESTOREGAME`, `SMSG_WORLDINFO`, and the empty `enum RestoreTypes {}` (SD:552-554).
  - `GAME_RESTORING` is never set (SS:36); `GAME_REVISITING` is always "0".
  - `BuildSavePath` is commented out (SS:1096-1132).
  - The `PostStartWorld` comment promises an autosave that it does not perform (SS:576-594).
  - The legacy `Menu.cpp` has its own restart and quicksave items (Menu.cpp:2415-2433), but it is **not in `ClientShellDLL.dsp`** (only `LoadSave.cpp` and `MenuLoadGame.cpp` are, at dsp:411, 471).
- Macro inconsistency: `_ADD_ON` (SSH:53, 111) versus `_ADDON` (SS:864, PO:6549, CS:2865).

---

## 11. Invariants a faithful reimplementation must keep (derived)

1. The engine save is written **only** to `Save\Current\{Current,Auto,Keep}.sav`. Slots are whole-directory copies made by the client after the server's ACK. Loading copies the slot into `Current` **before** the restore.
2. A full restore uses `LoadWorld(NORELOADGEOMETRY)` **without** world objects (only `CF_ALWAYSLOAD`), then `RestoreObjects(dwParam=0, RESTORETIME)`, then forces the start-point, revisit and autosave variables to 0, then `RunWorld`. The player is reattached through a saved `CIF_LOCAL` client ref and kept in its saved position (`FORCEROTATION` + `EYELEVEL`).
3. A transition: `KeepAliveSave` (active players only, `dwParam=1`, flags 0) **before** the world load; then `LoadWorld(objects)`; then `KeepAliveLoad(dwParam=1, no time)`, which drops keys; then go to the named start point, then autosave.
4. Restart = `Auto.sav` taken after the single-player init of the latest `NEW_GAME` or `NEW_LEVEL` entry. Restores never refresh it.
5. The client blob lives in the player's record; it is consumed by the next player save and broadcast on load.
6. Difficulty and game type persist through the saved game console, not through `Save.dat`. `Save.dat` carries the title + timestamp, the level, the game type and the character.
7. Many failure paths are silent: no ACK on a failed save (soft lock), ignored restore and keepalive results, a silent `PrintError`. Any deliberate improvement should be documented as a departure.
