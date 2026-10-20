# Engine-side save/restore audit (later LithTech runtime vs 1998 Blood2 SDK)

Root: the LithTech reference tree (`lithtech/`, a separate checkout beside this repository, never a build dependency). Abbreviations:

- `GS` = `runtime/server/src/game_serialize.cpp` (the whole serializer, 1348 lines)
- `SM` = `runtime/server/src/servermgr.cpp`, `SO` = `runtime/server/src/s_object.cpp`, `SI` = `runtime/server/src/serverde_impl.cpp`
- `SDK` = `sdk/inc/` (later SDK), `AH` = `Blood2/AppHeaders/` (1998 SDK), `B2` = `Blood2/ObjectDLL/`, `SHOGO` = `Shogo/ObjectDLL/`

The later runtime is evidence of later LithTech behavior, not a trace of the 1998 executable. Every "1998" statement below rests on headers or 1998 game usage, unless it is listed in §10.

---

## 0. Entry points

| API | Later impl | 1998 decl |
|---|---|---|
| `SaveObjects(file, ObjectList*, dwParam, flags)` | `SI:2940-2949` → `GS:1091 sm_SaveObjects` | `AH/server_de.h:879` (`char*`, `DDWORD`) |
| `RestoreObjects(file, dwParam, flags)` | `SI:2951-2960` → `GS:1231 sm_RestoreObjects` | `AH/server_de.h:883` |
| `LoadWorld(file, flags)` / `RunWorld()` | `SI:2962-2970` → `SM:2523 DoStartWorld`, `SM:2688 DoRunWorld` | `AH/server_de.h:889-890` |
| `GetSaveFileVersion(uint32&)` | `SI:315` (returns the last read/written `g_dwSaveFileVersion`) | absent in 1998 |

- The file is opened with `streamsim_Open(..., "wb"/"rb")` (`SI:2942,2953`).
- If the file cannot be opened, the call returns `LT_ERROR`.
- `si_SaveObjects` otherwise **always returns LT_OK**. `sm_SaveObjects` returns `void` and the code never checks write errors (`SI:2946-2948`).

There are **no** `RESTOREOBJECTS_KEEPALIVE` or `SAVEOBJECTS_SAVEGAMESTATE` flags in either SDK.

| Flag | Later | 1998 |
|---|---|---|
| `RESTOREOBJECTS_RESTORETIME` | 1 (`SDK/iltserver.h:48`) | 1 (`AH/server_de.h:29`) |
| `SAVEOBJECTS_SAVEGAMECONSOLE` | 1 (`SDK/iltserver.h:52`) | 1 (`AH/server_de.h:31`) |
| `SAVEOBJECTS_SAVEPORTALS` | absent | 2 (`AH/server_de.h:32`) |

---

## 1. File layout (later runtime, version 2002)

The engine writes **no world name**. The game must remember the world and call `LoadWorld` before `RestoreObjects`. Blood2 keeps the world name in client slot metadata and sends it back in its LOADWORLD message.

Encoding:

- All scalars are raw little-endian `sizeof(field)` (`STREAM_WRITE`, `SDK/iltstream.h:15-16`).
- `string` = `uint16 len` followed by `len` bytes, with no NUL (`runtime/shared/src/genltstream.cpp:57-63`). Reads truncate to the buffer size and skip the rest (`:10-52`).
- `DO_CRC_CHECKS` is off (`GS:57`), so there are no per-field tokens.

### 1.1 Header (`GS:1144-1178`)

| Field | Type | Note |
|---|---|---|
| version | u32 | always 2002 (`GS:1144`). Restore accepts 2001 or 2002 (`GS:1269`). |
| objectCount | i32 | `nObjects` after filtering (`GS:1148`) |
| m_GameTime | f32 | `GetTime()` clock (`GS:1150`, `SI:991-994`) |
| m_nTrueLastTimeMS | u32 | `GS:1151` |
| m_nTimeOffsetMS | i32 | `GS:1152` |
| console vars | repeat {u8 1, string name, string value} | only with `SAVEOBJECTS_SAVEGAMECONSOLE`. These are the server "game" con vars (`SetGameConVar` → `console_state`, `SI:2311-2317`). Each string is truncated to 200 chars (`GS:1155-1175`). |
| end of console vars | u8 0 | always written (`GS:1177-1178`) |

### 1.2 Per-object record, repeated `objectCount` times in list order (`GS:355-603`)

| # | Field | Type | Cite |
|---|---|---|---|
| 1 | nextObjectPos | u32, absolute file offset of the next record (back-patched) | 360-361, 599-602 |
| 2 | objDataPos | u32, absolute offset of the payload length field (back-patched) | 363-364, 582-585 |
| 3 | class name | string | 383 |
| 4 | m_ObjectType | u8 | 387 |
| 5 | m_Flags, m_Flags2 | u32, u32 | 388-389 |
| 6 | m_nRenderGroup | u8 | 390 |
| 7 | position | LTVector (3×f32) | 392 |
| 8 | GlobalForceOverride | LTVector | 393 |
| 9 | m_Scale | LTVector | 394 |
| 10 | m_Rotation | LTRotation (4×f32 quaternion) | 395 |
| 11 | m_UserFlags | u32 | 396 |
| 12 | object name | string, "" if unnamed | 398-408 |
| 13a | OT_WORLDMODEL: BSP name (`m_pOriginalBsp->m_WorldName`) | string | 410-413 |
| 13b | not MODEL and not WORLDMODEL: used filename | string | 417-421 |
| 14 | OT_MODEL: model filename, **written only if `GetModelDBFilename` succeeds** (the reader always reads it, so a failure desynchronizes the stream); child models [1..MAX_CHILD_MODELS-1]; skins ×MAX_MODEL_TEXTURES; render styles ×MAX_MODEL_RENDERSTYLES | strings | 424-468 |
| 14' | every other type, including world models: skin, render style | 2 strings | 469-475 |
| 15 | OT_LIGHT: radius | f32 | 477-480 |
| 16 | OT_CONTAINER: container code | u16 | 482-485 |
| 17 | **NextUpdate countdown** (relative) | f32 | 487 |
| 18 | blocking priority | u8 | 490 |
| 19 | hasSFX; if set: u16 bit length followed by `ceil(bits/8)` bytes | u8 [+u16+bytes] | 493-506 |
| 20 | OT_MODEL: u8 trackerCount; per tracker: ID(u8), CurKey(u32), LastHintTime(u32), Flags(u16), hHintNode, hLastHintAnim, InterpolationMS(u16), RateModifier(f32), tfLastHint(LTransform), TimeRef{Percent, iWeightSet, Prev{iAnim u16, iFrame u16, Time u32 ms}, Cur{…}} | | 509-541 |
| 21 | Color R,G,B,A | 4×u8 | 543-546 |
| 22 | dims | LTVector | 547 |
| 23 | mass | f32 | 548 |
| 24 | velocity | LTVector | 549 |
| 25 | friction | f32 | 551 |
| 26 | acceleration | LTVector | 552 |
| 27 | ForceIgnoreLimitSqr | f32 | 553 |
| 28 | m_InternalFlags (IFLAG_*, including INACTIVE / INACTIVE_TOUCH / INSKY / HASCLIENTREF) | u32 | 555 |
| 29 | if IFLAG_INSKY: sky slot index | u16 | 557-569 |
| 30 | hasClient; if set: Client::m_ClientFlags (u32) and client name (string) | u8 … | 572-579 |
| 31 | ← objDataPos points here: payload length **in bits** | u32 | 593 |
| 32 | game MID_SAVEOBJECT payload | `ceil(bits/8)` bytes | 591-594 |
| 33 | GAMESERIALIZE_CRC `0xABCDEFAF` (always written) | u32 | 596 |

The game payload is produced by `OnSave(&CLTMessage_Save, (float)dwParam)` (`GS:367-371`). The engine calls it **before** writing the engine fields, but the bytes go after them.

`(SFX bits + payload bits) > 0x10000` prints only a warning (`GS:377-380`). That threshold is 8 KiB, which equals `MAX_SAVEOBJECT_LEN 8192` (`SDK/ltbasedefs.h:471`, `AH/basedefs_de.h:85`). The SFX message must be smaller than 0x10000 bits (assert, `GS:497`).

**Not saved by the engine:**

- standing-on and rider links
- model node hide status and model playing/looping, except as encoded in tracker flags
- sprite clocks
- net flags and change flags
- `m_UserData`
- server sound tracks
- deactivation timers (absent in the later runtime)
- portal states (no `SAVEPORTALS` in the later runtime)

### 1.3 Trailer (`GS:1186-1199`)

- u32 terminator `0xFFFFFFFF`.
- Then, for each saved object in the same order:
  - Attachments: u16 count, then {LTVector offset, LTRotation rot, u16 childSerializeID, u32 socket (`m_iSocket`, model node index)} (`GS:946-982`).
  - Interlinks: u16 count, then {u8 type, u16 otherSerializeID} (`GS:985-1026`).
- Final u8 0.

---

## 2. Object selection and order (`GS:1105-1140`)

1. The game builds the `ObjectList`. The engine iterates `m_pFirstLink` → `m_pNext`.
   - `AddObjectToList` **prepends** (`SI:1637-1650`), so the file order is the reverse of the game's insertion order.
2. The engine skips:
   - any class with `CF_ALWAYSLOAD` on itself or an ancestor (`GS:1124`; `cb_IsClassFlagSet` walks parents, `runtime/shared/src/classbind.cpp:142-153`);
   - objects marked `IFLAG_OBJECTGOINGAWAY` (`GS:1128`).
   - Nothing else is excluded. Client player objects are saved, with a client-ref block (field 30), and there is no keepalive filter.
3. `om_ClearSerializeIDs` sets **every** object in every type list to `0xFFFF` (`runtime/shared/src/objectmgr.cpp:1981-1993`). Each saved object then gets `m_SerializeID = index` (u16), so at most 65535 objects.
4. Objects are written in index order, then attachments and links in the same order.

How the games build the list:

- **Blood2 full save** (`B2/BloodServerShell.cpp:1004-1066`): `USRFLG_SAVEABLE` objects from active `GetNextObject` (1030), then inactive `GetNextInactiveObject` (1039-1044). Players are included when `bSavePlayers` is set. `dwParam=0`, flags `GAMECONSOLE|SAVEPORTALS` (1053-1058).
- **Shogo save**: every active and inactive object, with no filter (`SHOGO/RiotServerShell.cpp:1488-1510`).

Engine iteration order (later): new objects are added at the head of `m_Objects` (`SM:1775`). `GetNextObject` walks forward until it reaches the first inactive object (`SI:2531-2559`). `GetNextInactiveObject` walks backward from the tail (`SI:2562-2590`). State changes move objects to the head or tail (`SO:640-665`).

---

## 3. Handle encoding and remapping

### 3.1 Save-message object references

**Later runtime**, `CLTMessage_Save::WriteObject` (`GS:277-289`):

- Valid, not going away: `WriteBits(m_ObjectType, 4)` then `WriteBits(m_SerializeID, 16)`.
- NULL or going away: type `NUM_OBJECTTYPES` and ID `0xFFFF`.
- Objects not in the save list, including CF_ALWAYSLOAD objects, still have ID `0xFFFF` after step 3, so **they become NULL on load**.

**Later runtime**, `CLTMessage_Load::ReadObject` (`GS:190-220`):

- Reads the 4-bit type and 16-bit ID.
- If the ID is `0xFFFF` or the type is greater than `NUM_OBJECTTYPES`, it returns NULL.
- Otherwise it scans the linear list of that type for a matching `m_SerializeID`.
  - Only objects created by this restore carry valid IDs, because all IDs are cleared again at `GS:1298`.
  - References to world-file objects already present (CF_ALWAYSLOAD objects, or the destination world on a keepalive restore) therefore resolve to NULL.

**Contrast:** ordinary message `WriteObject` writes the u16 runtime object ID (`runtime/shared/src/ltmessage.cpp:115-125`) and resolves it through `m_ObjectMap` (`runtime/server/src/ltmessage_server.cpp:92-104`).

- A nested `ReadMessage`/`WriteMessage` inside a save payload uses `CLTMessage_Read_Server::ReadMessage` (`GS:148-153`). Objects inside it are therefore read with runtime-ID semantics. This is a hazard.

**1998:**

- A separate API exists: `WriteToLoadSaveMessageObject(HMESSAGEWRITE, HOBJECT)` and `ReadFromLoadSaveMessageObject(HMESSAGEREAD, HOBJECT*)`, valid only inside MID_SAVEOBJECT/MID_LOADOBJECT (`AH/CSBase.h:52-53,70-71`).
- `WriteToMessageObject` is explicitly forbidden in MID_SAVEOBJECT (`AH/CSBase.h:49`).
- The byte encoding is unknown. LMessage in 1998 is byte-oriented (`AH/lmessage.h:80-103`), not bit-packed.

### 3.2 Restore sequence (`GS:1231-1346`, `GS:608-943`)

1. Read version and count, then the timers (§5). Recompute the time offset. **Only then** check the version, so a bad file has already overwritten the clock under RESTORETIME (`GS:1242-1275`).
2. Console vars: each one is fed to `cc_HandleCommand2(state, "name value", CC_NOCOMMANDS)` regardless of the restore flags (`GS:1206-1229,1277`). Objects created afterwards see the restored con vars. Values containing spaces go through the console parser.
3. **Pass 1**, `sm_CreateNextObject` for each record until the terminator (`GS:1292`):
   1. Read class name. `FindClassData`, then `pClassData->m_pClass` is dereferenced **before** the NULL check (`GS:634-640`).
   2. Read engine fields 4-17 into `ObjectCreateStruct`: type, flags, flags2, render group, pos, force override, scale, rotation, name, filename(s), skins, render styles, container code, NextUpdate (`GS:645-705`). `m_hClass` is set.
   3. Allocate the object and run its C++ constructor (`GS:708`; `SO:792-804`).
   4. **`OnPrecreate(&ocs, PRECREATE_SAVEGAME = 3.0)`** = MID_PRECREATE (`GS:712`). No props are available; BaseClass reads props only for 1.0/2.0.
      - A zero return skips the object: it is freed, the stream seeks to the next record, and **`pObjects[i]` stays NULL**. Later `pObjects[i]->m_SerializeID` and attach/link restore dereference NULL (`GS:715-720,1299-1301,1052`).
   5. `sm_AddObjectToWorld(..., OBJECTCREATED_SAVEGAME)` (`SM:2352-2440`):
      - `om_CreateObject` (flags, pos, rotation from the OCS)
      - server data, with `m_NextUpdate = ocs.m_NextUpdate` (`SM:1753`)
      - **OnActivate** (MID_ACTIVATING, later only, `SM:2378`)
      - extra data: model/sprite file binding; world model and container BSP lookup by name (`runtime/server/src/server_extradata.cpp:290-312`; world model init `:229-254`)
      - ID allocation; initial world-model rotation (`SM:2415`); BSP insert
      - **`OnObjectCreated(INITIALUPDATE_SAVEGAME = 3.0)`** = MID_INITIALUPDATE (`SM:2423`)
      - `FullMoveObject` teleport, which builds container/touch links (`SM:2431`)
      - On failure it returns the error and **leaks the allocated BaseClass** (`GS:725-728`).
   6. The engine then **overwrites** the following with saved values. Anything INITIALUPDATE set for these is lost:
      - user flags; light radius (`GS:731-735`)
      - blocking priority; **SFX message, only if one was saved** (`GS:738-756`)
      - model trackers, created by ID (`GS:759-800`)
      - color, dims (`ChangeObjectDimensions`), mass, velocity, friction, acceleration, force limit (`GS:802-832`)
      - active/inactive state via `sm_SetObjectStateFlags`, which may call OnDeactivate/OnActivate; then `m_InternalFlags` is overwritten wholesale (`GS:835-836`)
      - a teleport to the saved position, because AddObjectToWorld ignores pos for world models (`GS:840`)
      - sky slot re-registration (`GS:843-847`)
      - client ref recreation (`GS:851-870`)
   7. Not reapplied after INITIALUPDATE: `m_Flags`/`m_Flags2`, the NextUpdate countdown and the name. INITIALUPDATE or OnLoad changes to these win.
4. Clear all serialize IDs and assign `pObjects[i].m_SerializeID = i` (`GS:1298-1302`).
5. **Pass 2**, `sm_RestoreNextObject` for i in 0..count-1 (`GS:1305-1312`):
   1. Seek to objDataPos.
   2. Read the payload into a memory packet.
   3. **`OnLoad(&CLTMessage_Load, (float)dwParam)`** = MID_LOADOBJECT (`GS:914`). Every restored object already exists, so forward references resolve. Some referenced objects have not received their own OnLoad yet.
   4. Read the trailing CRC from the **stream**, not from the message (`GS:917-938`). It detects stream corruption only; OnLoad over- or under-reads go undetected. The "UNDER/OVER wrote" diagnostics are legacy.
6. Read the terminator, then restore attachments and interlinks for every object (`GS:1314-1332`; §3.3).
7. Back in the caller, `RunWorld` → `PostStartWorld`, then **`OnAllObjectsCreated`** (MID_ALLOBJECTSCREATED, later only) for every object in `m_Objects`, then `sm_FinishUpdateFrame` (`SM:2688-2720`). After that, `NextUpdate` countdowns drive MID_UPDATE.

Order summary: PRECREATE(3) → [ACTIVATING] → INITIALUPDATE(3) → engine state overwrite → [DEACTIVATING if saved inactive] → (all objects) → LOADOBJECT(dwParam) → (all objects) → engine attachments and links → [RunWorld: ALLOBJECTSCREATED] → updates. Blood2 guards INITIALUPDATE work with `fData != INITIALUPDATE_SAVEGAME` (for example `B2/KeyFramer.cpp:402`, `B2/CameraObj.cpp:135`, `B2/PickupObject.cpp:150`).

### 3.3 Attachments and interlinks

**Attachments:**

- Saved only when the child's serialize ID is valid, meaning the child is in the save list (`GS:956-980`).
- Restored with `om_CreateAttachment(parent, child->m_ObjectID, socket, &offset, &rot)` (`GS:1029-1058`).
- If the parent is not saved, the child is restored detached. If the child is not saved, the attachment is dropped.

**Interlinks (engine-restored):**

- Only links **owned** by the object, of type `LINKTYPE_INTERLINK` (0) or `LINKTYPE_CONTAINER` (1), whose other end is saved (`GS:994-1025`; types in `runtime/server/src/interlink.h:9-11`).
- Restored with `CreateInterLink`, which is a no-op if the link already exists (`runtime/server/src/interlink.cpp:134-139`). Game-side recreation in OnLoad is therefore harmless.
- `LINKTYPE_SOUND` is never saved.
- Links are restored **after** all OnLoad calls. During OnLoad, engine links and attachments do not exist yet.

**1998:**

- Interlink and attachment persistence is unknown.
- `CreateInterObjectLink` dedupe is documented (`AH/server_de.h:601-608`).
- Blood2 recreates a link in `SeeingEye::Load` (`B2/SeeingEye.cpp:510-511`). Most classes do not (Projectile, CameraObj, KeyFramer, Destructable). This weakly suggests that the engine restored links.
- 1998 attachments are by node **name** (`AH/server_de.h:625-626`), not by socket index.

---

## 4. World models, brushes and world-file objects

- **Nothing is matched.** Restore never updates world-file objects in place. It always **creates new objects** (`GS:723`).
- Correct usage (Blood2 `B2/BloodServerShell.cpp:940-952`, Shogo `SHOGO/RiotServerShell.cpp:1349-1351`): `LoadWorld(path, NORELOADGEOMETRY)` **without** `LOADWORLD_LOADWORLDOBJECTS`, then `RestoreObjects`, then `RunWorld`.
  - With that flag cleared, `LoadObjects` instantiates **only CF_ALWAYSLOAD classes** from the world file and skips `CF_NORUNTIME` (`SO:390-400`).
  - The engine also always creates `MainWorld` objects (an ALWAYSLOAD class) for physics/vis BSPs (`SM:1884-1910,2643`).
- Consequences:
  - Non-ALWAYSLOAD world-file objects destroyed before the save **do not reappear**, because they are neither loaded nor saved.
  - ALWAYSLOAD objects always reappear in their **initial world-file state**, even if removed or changed before the save, and references to them in saved payloads become NULL (§3.1).
    - Blood2 ALWAYSLOAD game classes: GameStartPoint, CameraSpot, ExitHint, Key, LightFX, ParticleSystem, PolyGrid, SoccerGoal, Script (`B2/*.cpp`).
    - Example: KeyFramer removes its `Key` objects on its first update (`B2/KeyFramer.cpp:194-197`). After a restore the Keys are recreated from the world, and the already-initialized KeyFramer does not consume them again.
  - 1998 engine classes that are ALWAYSLOAD (`AH/engineobjects.c`): Model 176, WorldModel 208, Container 246, Sprite 302, Sound 431, StartPoint (C build) 444, World 572, Brush 606, DemoSkyWorldModel 701, SkyPointer 811, FogSphere 855.
    - The C++ `StartPoint` in `AH/cpp_engineobjects_de.cpp:25-26` has **no** flag.
    - The built-in `Sound` replays its loop on the INITIALUPDATE of every restore and then removes itself (`AH/engineobjects.c:382-420`).
  - Later ALWAYSLOAD set (`SDK/ltengineobjects.cpp`): Container 241, Sound 299, MainWorld 415, StaticSunLight 637, Brush 841, DemoSkyWorldModel 855, SkyPointer 948.
- **World-model objects of game classes** (doors, rotating or destructible brushes) are saved with:
  - their BSP name (field 13a) and container code;
  - position, rotation, dims, velocity and so on.
- They are restored as follows:
  - Extra-data init binds the BSP by name and sets pos to the BSP translation (`runtime/server/src/server_extradata.cpp:229-254`).
  - `InitialWorldModelRotate` applies the saved rotation (`SM:2413-2416`; `runtime/shared/src/moveobject.cpp:1406-1411`).
  - The engine then teleports to the saved position (`GS:840`).
  - The geometry is shared and immutable. Only the object transform is state.
- `LOADWORLD_NORELOADGEOMETRY` keeps the BSP if the world name matches (`SM:2541-2556`). Objects are still all destroyed in `DoEndWorld` (`SM:2763`).

---

## 5. Timers and update scheduling

- `GetTime()` returns `m_GameTime` (`SI:991-994`). It advances by the clamped frame time ×timescale (`SM:1174-1175`) and is **reset to 0** on every `LoadWorld` (`SM:2576,2673`).
- The 1998 doc says "seconds since the server started up" (`AH/server_de.h:191`). The later doc says the same despite the reset (`SDK/iltcsbase.h:339`), so the doc is unreliable.
- Save always writes `m_GameTime`, `m_nTrueLastTimeMS` and `m_nTimeOffsetMS` (`GS:1150-1152`).
- **With `RESTOREOBJECTS_RESTORETIME`**, all three are restored (`GS:1249-1254`), so absolute `GetTime()` deadlines in payloads stay valid. Without it they are read and discarded (`GS:1255-1263`), and the clock stays at the new world's 0.
- Either way, `m_nTimeOffsetMS = TrueLast - (now + Offset)` (`GS:1265`). This is only continuity-correct when the saved offset is 0. Only the first true frame delta, used by model animation, is affected.
- The client receives `m_GameTime` in `SMSG_LOADWORLD` (`runtime/server/src/s_client.cpp:538-541`) and per update (`:1072-1075`).
- **NextUpdate is a relative countdown**: `sd->m_NextUpdate -= FrameTime`, and the object updates when it crosses ≤0; a value ≤0 means never (`SO:312-325`; `runtime/server/src/serverobj.h:58`). `SetNextUpdate` stores the value raw (`SI:2595-2601`).
  - The countdown is saved raw (`GS:487`) and restored through `ocs.m_NextUpdate` before INITIALUPDATE (`GS:705`, `SM:1753`).
  - A `SetNextUpdate` call in INITIALUPDATE(SAVEGAME) or OnLoad overrides it.
  - 1998 has the same countdown semantics: "counter … set to 0.0001 when an object is created" (`AH/server_de.h:674-677`).
- **Active/inactive** (`OBJSTATE_INACTIVE`, `OBJSTATE_INACTIVE_TOUCH`) is saved in the internal flags and restored through `sm_SetObjectStateFlags` (`GS:835`).
- Later has no auto-deactivation and no ping timers (only a commented console hook at `runtime/server/src/s_concommand.cpp:311-313`).
- 1998 had `SetDeactivationTime`, `PingObjects`, `OBJSTATE_AUTODEACTIVATE_NOW = 3` and `ObjectCreateStruct::m_fDeactivationTime` (`AH/server_de.h:62,679-684`; `AH/basedefs_de.h:225`). Whether 1998 saved them is unknown.
- Blood2 game code saves times **relative** to `GetTime()` (for example `B2/PlayerObj.cpp:6557-6566` and `6699-6708`). It therefore works for both RESTORETIME and keepalive restores.

---

## 6. Sounds and music

- **Server sounds are not saved.** No sound-track data appears in the record. `LINKTYPE_SOUND` links are excluded (`GS:998-999`).
- On `LoadWorld`, `DoEndWorld` removes every object and then **every** server sound track, including `PLAYSOUND_GETHANDLE` ones (`SM:2763-2767,2128-2151`).
  - Saved `HSOUNDDE` values are therefore meaningless. Games must restart looping or attached sounds in OnLoad or INITIALUPDATE.
  - ALWAYSLOAD `Sound` objects re-fire from the world file.
- **Music** is client-side in 1998 (`AH/client_de.h:242-270`). The server has no music API, and nothing about music is saved by the engine. Blood2/Shogo carry client data through their own messages (for example `SaveClientData`, `Blood2/ClientShellDLL/BloodClientShell.cpp:6351,8447`).
- 1998 server-kept sound semantics are described in `AH/basedefs_de.h:525-560`. Their persistence across a world load is unknown.

---

## 7. Client side

- **The engine saves no client state**: no client-only objects, SFX instances, camera or shell data. The client runtime contains no save code.
- A world load disassociates clients and returns them to `WAITINGTOENTERWORLD` (`SM:2749-2760`). All restored objects are new to the client.
- The server sends each object with `CF_NEWOBJECT`, including its stored SFX message (`runtime/server/src/s_net.cpp:582-606`). The client calls `SpecialEffectNotify(obj, msg)` for new objects that have SFX data (`runtime/client/src/shellnet.cpp:826-838`).
  - Client SFX are rebuilt only from the **saved** `m_cSpecialEffectMsg`, which is restored verbatim (`GS:741-756`).
  - Any client-side SFX state such as timers or particles restarts from scratch.
- Game con vars, including any restored by RestoreObjects, are sent to each connecting client (`runtime/server/src/s_client.cpp:520-535`).
- **Client refs:**
  - The engine keeps an `m_ClientReferences` list. A saved object with a client gets a ClientRef `{flags, name, objectID}` and `IFLAG_HASCLIENTREF` (`GS:851-870`).
  - The server shell walks `GetNextClientRef`, tests `GetClientRefInfoFlags & CIF_LOCAL` or matches the name, and returns `GetClientRefObject` from `OnClientEnterWorld` (`B2/BloodServerShell.cpp:206-238`; engine `SI:2107-2165`).
  - The engine then drops that ref (`runtime/server/src/s_client.cpp:450-465`).
  - Unclaimed client-ref objects are removed and the list is cleared only at the **next** world end (`SM:2742-2746`; `SO:703-721`).
  - The 1998 API is identical (`AH/server_de.h:408-416`).

---

## 8. Keepalive (level transition)

- The **engine has no keepalive mechanism** in the later runtime.
  - `FLAG_KEEPALIVE (1<<20)` exists only in 1998 (`AH/basetypes_de.h:354`: "Save and restore this object when switching worlds."). Later, bit 20 is `FLAG_TOUCHABLE` (`SDK/ltbasedefs.h:1950`).
  - There is an orphan declaration `sm_DestroyKeepaliveObjectList()` with no definition and no caller (`runtime/server/src/s_object.h:90-91`). This suggests an earlier engine keepalive list.
- Keepalive is implemented by the game with the same `SaveObjects`/`RestoreObjects`. Only the arguments differ:

| | Full save (Blood2) | Keepalive (Blood2) |
|---|---|---|
| list | `USRFLG_SAVEABLE`, active and inactive, players included (`B2/BloodServerShell.cpp:1020-1050`) | **active** players only, via `GetNextObject` (`:1160-1171`) |
| SaveObjects | `(Current/Auto.sav, list, 0, GAMECONSOLE\|SAVEPORTALS)` (`:1053-1058`) | `(Keep.sav, list, 1, 0)` (`:1174`): no console vars, but header timers are still written |
| when | on request | before the destination `LoadWorld` (`:907-914`) |
| LoadWorld | `NORELOADGEOMETRY` without `LOADWORLDOBJECTS` (`:940-952`) | `LOADWORLDOBJECTS\|NORELOADGEOMETRY`: the destination is fully populated |
| RestoreObjects | `(file, 0, RESTORETIME)` (`:1090`) | `(Keep.sav, 1, 0)` (`:1204`): clock stays at the new world's 0 |
| dwParam use | — | `fData==1`: drop keys (`B2/InventoryMgr.cpp:3024-3027`), deactivate items (`B2/InvItem.h:224-225`) |

- On a keepalive restore, references from player payloads to source-world objects become NULL. The player is reclaimed by `OnClientEnterWorld` through its ClientRef.
- Shogo uses the same pattern with `dwParam = LOAD_NEW_LEVEL`. It includes a CStoryTrigger (created with `FLAG_KEEPALIVE` and also manually listed) and player-built keepalives (`SHOGO/RiotServerShell.cpp:940-948,1285-1385`).
  - The flag is set but saved manually anyway. That is evidence that the 1998 engine did not rely on FLAG_KEEPALIVE, but it is unverified.
- Blood2 has `FLAG_KEEPALIVE` commented out in the player flags (`B2/PlayerObj.cpp:2088,3475`).

---

## 9. Error handling (later runtime)

- **Save:**
  - No error checks on writes.
  - If allocation of the filtered array fails, the function returns with an **empty file** and `LT_OK` (`GS:1109-1116`).
  - A model filename lookup failure omits a string, which corrupts the file (`GS:432-435`).
- **Restore:**
  - A short read zero-fills and sets a sticky error (`runtime/kernel/src/sys/win/streamsim.cpp:32-55`). Only the console-var loop checks it (`GS:1219-1222`).
  - The version check accepts only 2001 or 2002 and returns `LT_INVALIDVERSION`, but only after RESTORETIME has overwritten the clock (`GS:1249-1275`).
  - A pass-1 error leaves the **objects already created in the world**, with no OnLoad and no rollback, and pass 2 is skipped. Possible pass-1 errors:
    - missing class: NULL deref first, then `LT_CANTRESTOREOBJECT`
    - `sm_AddObjectToWorld` failure
    - sky index ≥ `MAX_SKYOBJECTS` (`GS:819-826`)
  - Truncation inside the object list reads `nextObjectPos=0` and class "", which leads to the NULL deref above.
  - Pass-1 does not bound the count: more records than `objectCount` overflow `pObjects` (`GS:1287,1292`).
  - A pass-2 CRC mismatch returns `LT_INVALIDFILE` and leaves a partially loaded world (`GS:917-938`). **A failure on the last object is masked**: `cur == count` forces `LT_FINISHED` (`GS:1308-1319`).
  - Attachment or link IDs out of range return `LT_ERROR` (`GS:1047-1050,1074-1077`).
  - The function returns `LT_OK` or the error (`GS:1340-1345`). It never rolls back.
- **Game reactions:**
  - Blood2 **ignores** the `RestoreGame` result and still calls `RunWorld` (`B2/BloodServerShell.cpp:960-966,985`).
  - Shogo/NOLF report the error and skip `RunWorld` (`SHOGO/RiotServerShell.cpp:1361-1370`).
- Error codes match the 1998 ones: `DE_CANTRESTOREOBJECT 58`, `DE_INVALIDVERSION 69`, `LT_INVALIDFILE 77` (`AH/de_codes.h:201,218,232`; `SDK/ltcodes.h:440,477,500`).

---

## 10. 1998 headers vs later runtime

| Item | 1998 (`AH`) | Later |
|---|---|---|
| Signatures | `DRESULT (*SaveObjects)(char*, ObjectList*, DDWORD, DDWORD)` and `RestoreObjects(char*, DDWORD, DDWORD)`, function pointers on ServerDE | same order, `const char*`, `uint32`, `LTRESULT` |
| Flags | `SAVEPORTALS = 2` exists (portal API `Get/SetPortalFlags`, `server_de.h:292-296`; used by `B2/Door.cpp:996-1003`) | absent |
| LOADWORLD_* | 1, 2, 4 | identical |
| fData | "dwParam passed to SaveObjects/RestoreObjects" (`serverobj_de.h:114-118`); EngineMessageFn takes `float fData` (`:152`), and games cast with `(DDWORD)fData` (`B2/PlayerObj.cpp:4227`) | `(float)dwParam` (`GS:370,914`); exact up to 2^24 |
| PRECREATE/INITIALUPDATE | 0..3 (`serverobj_de.h:67-75`) | 0..3, plus 4 CLIENTOBJ; INITIALUPDATE_* are deprecated aliases of OBJECTCREATED_* (`SDK/iltbaseclass.h:419-458`) |
| MIDs | 0-10 identical; **11 = MID_GETFORCEUPDATEOBJECTS** (`serverobj_de.h:133`) | 11 removed; 12 ACTIVATING, 13 DEACTIVATING, 14 ALLOBJECTSCREATED, 15 TRANSFORMHINT (`SDK/iltbaseclass.h`). MID_ACTIVATING/DEACTIVATING/ALLOBJECTSCREATED **do not exist in 1998**. |
| Handle API | `WriteToLoadSaveMessageObject` / `ReadFromLoadSaveMessageObject` (out-param, DRESULT) | message `WriteObject`/`ReadObject` overridden in save messages |
| MAX_SAVEOBJECT_LEN | 8192 bytes | 8192 (warning only) |
| CF_ALWAYSLOAD | 1<<3, "this class and sub-classes … can't be saved" (`serverobj_de.h:62-63`) | 1<<3, inheritance-checked |
| ObjectCreateStruct | type (u16), flags, pos, scale, rotation, container code, `m_UserData`, filename, single skin, name, `m_NextUpdate`, **`m_fDeactivationTime`** (`basedefs_de.h:201-226`) | adds Flags2, render group, global force override, child models, skin/render-style arrays; no deactivation time |
| Object states | 0, 1, 2, **3 AUTODEACTIVATE_NOW** | 0, 1, 2 |
| FLAG bit 20 | FLAG_KEEPALIVE | FLAG_TOUCHABLE |
| CIF flags | adds CIF_FORCENEXTUPDATE (1<<4) | 0..3 |
| Sky | `AddObjectToSky(hObj, index)` (`server_de.h:258`) | same, persisted through IFLAG_INSKY plus the slot |

**1998 engine BaseClass** (`AH/engineobjects.c:13-127`; `AH/cpp_engineobjects_de.h:96-110`):

- `bc_EngineMessageFn` acts only on MID_PRECREATE with `fData == 1.0` or `2.0` (reads Name, Pos, Rotation, Flags, the boolean flag props, Filename, Skin).
- For **every** message, including MID_SAVEOBJECT and MID_LOADOBJECT, it forwards to the aggregates in list order.
  - `bc_AddAggregate` prepends, so aggregates run in reverse order of addition.
  - It returns 1.
- **The base class writes or reads no data of its own.**
- The C++ wrapper only sets `ocs->m_ObjectType = m_nType` on PRECREATE when the type is OT_NORMAL. That covers SAVEGAME too, but the saved type is already present.
- The later `BaseClass` is equivalent (`SDK/ltengineobjects.cpp:56-…`; `SDK/iltbaseclass.cpp:138-142`).

---

## 11. Unknown or unverified for 1998

1. The actual save file format: version number, field set and order, header, and whether the console, portals or timers are serialized as in §1. Nothing in the 1998 SDK specifies it.
2. The encoding of `WriteToLoadSaveMessageObject` (index width, NULL or out-of-list sentinel) and whether `ReadFromLoadSaveMessageObject` returns an error for unresolvable references.
3. Whether the engine saved and restored **interlinks** and **attachments** (attachments were by node name in 1998).
4. Whether model animation state (index, time, looping, playing), node hide status and sprite frames were saved.
5. Whether the SFX message was saved and restored and replayed to clients in the same way.
6. Whether `SAVEOBJECTS_SAVEPORTALS` restored portal flags at restore time or at LoadWorld.
7. Whether game time reset to 0 on LoadWorld (the doc says "since the server started up") and whether RESTORETIME semantics match.
8. Whether `m_fDeactivationTime`, auto-deactivation and ping state were saved.
9. Whether FLAG_KEEPALIVE triggered any engine behavior. The Shogo double use and the Blood2 commented flags suggest it did not.
10. The relative dispatch order of PRECREATE, INITIALUPDATE and LOADOBJECT, and whether engine fields (user flags, dims, velocity) overwrite INITIALUPDATE results. Blood2's `fData != INITIALUPDATE_SAVEGAME` guards are consistent with INITIALUPDATE preceding LOADOBJECT, as in the later runtime.
11. Whether a PRECREATE(SAVEGAME) returning 0 was handled, and how errors or truncation left the world (partial or rolled back).
12. Whether the 1998 engine already excluded CF_ALWAYSLOAD and going-away objects at save time. The header documents ALWAYSLOAD exclusion, but the mechanism is unverified.
13. Whether 1998 restored client refs for objects without clients, and the CIF flag mapping.
14. Server-kept sound persistence across LoadWorld or RestoreObjects.
15. Save-list order (head-prepend list) and its effect on dispatch order.

## 12. Implications for an OpenBlood2 design (derived)

- Use a two-pass restore with a dense index remap, NULL for anything outside the list, and both passes over the same order. Run PRECREATE(3) → INITIALUPDATE(3) → engine-field overwrite → all LOADOBJECT → links and attachments → RunWorld.
- Restore must run on a world loaded **without** world objects except ALWAYSLOAD ones. Do not save ALWAYSLOAD objects, and do not resolve references to them.
- Save the NextUpdate countdown raw. Save `GetTime` and restore it only for full saves. Keepalive keeps the new world's clock.
- Engine sounds are never persisted. Client SFX are rebuilt from the saved SFX message on reconnection.
- Avoid the later runtime's hazards listed above: NULL holes, the masked last-object CRC, clock overwrite before the version check, the conditional model string and the missing rollback. All of them are documented here as reference behavior, not as requirements.
