# OpenBlood2 save/load map — what exists, what a whole-world save must cover

Repository paths are relative to this repository at commit `8c1ad35b`.
Original reference tree used for comparison: `lithtech/Blood2` (abbrev. `B2/`); later engine: `lithtech/runtime` (abbrev. `LT/`).
Roadmap item: `Documentation/roadmap.md:72` (M3 3.3 "Save/load: original slots, Quicksave/Quickload, Restart rows … whole-world plus player serialization"; building blocks listed as partial).

---

## 0. TL;DR

- **No persistence exists.** No file I/O, no byte format, no versioning. Everything is owned in-memory C++ structs with validation (`PlayerRestoreState` = `InventoryState` + `DestructableState` + `PlayerState` + engine body fields).
- **Only 3 original Save/Load paths survive, retyped:** `CInventoryMgr::Save/Load`, `CDestructable::Save/Load`, `CPlayerObj::SaveFields/LoadFields/RestoreModelState`. All other original `Save(HMESSAGEWRITE)/Load(HMESSAGEREAD)` methods and `MID_SAVEOBJECT/MID_LOADOBJECT` handlers were **stripped** on import; ~26 classes **explicitly refuse `PRECREATE_SAVEGAME`**.
- **The SDK message layer cannot carry original save data:** `CSBase` exposes only `WriteToMessageDWord`/`WriteToMessageHString` (+ reads). No Byte/Word/Float/Vector/Rotation/Object/LoadSaveObject/HMessageWrite primitives; `EndHMessageRead` is "unsupported". Typed side-car payloads (Damage/Key/Pickup/Immobilize) bypass bytes.
- **`SaveObjects`/`RestoreObjects` exist as thunks that always return `DE_ERROR`** and record an "unavailable service" (which itself blocks `run_world`/NEW_LEVEL acceptance). No original caller invokes them.
- **The transactional machinery for replacing a session already exists** (NEW_LEVEL `process_level_change`, `start_new_game`): build a fresh `GameSession` at a stable address while the source's original globals are swapped out, then promote at the owning frame boundary (`src/app/frontend.cpp:467`). A load would reuse this pattern.
- **The engine clock is not restorable** (`Objects::time_` private, only `advance()`); original code stores ≥136 absolute `GetTime()` timestamps. Engine handles (`HOBJECT`, `HSTRING`, `HATTACHMENT`, `SoundId`, `ObjectId`) are session-local, non-reused tokens → need remapping.

---

## 1. Existing Save/Load building blocks (NEW_LEVEL / KeepAlive)

### 1.1 Typed state headers (in-memory value snapshots, no handles)

| Struct | File | Contents |
|---|---|---|
| `InventoryState` | `src/game/inventory_state.h:16-29` | `right/left` 13 weapon-type bytes, 13 raw `ammo` floats, `keys` (`KeyItem` owned strings+charges), optional `medkit_charge`, optional `flashlight`/`goggles` (`FlashlightState{count,remaining}` :11-15), `eye`, `binoculars`, `strength/magic/current_slot`, `infinite_ammo`, `damage_multiplier`. Comment :9-10 "Owned source-level subset of CInventoryMgr::Save, not a retail save-file ABI. No engine handles…" |
| `InventoryLoadType` | `inventory_state.h:30-34` | `Restore=0` (keys kept), `KeepAlive=1` (keys dropped; source `dwLoadType==1`) |
| `DestructableState` | `src/game/destructable_state.h:9-20` | damage-physics/dead/nigh-invuln/god/trigger-only flags, mass, 4 health/armor caps, health, death_health, armor, resistance, last type/location/direction/amount/percent, death_delay, 5 strings (damage/death target+message, spawn_object), spawn_velocity. Owner identity deliberately absent (:7-8). |
| `PlayerState` | `src/game/player_state.h:10-25` | smell time **offset**+delay, dead, movement_blocked, imprison flag/**start offset**/length, falling/server_falling, **fall_start_offset**, min/max fall y, pitch/yaw, air, nullable `trigger`/`name` (`optional<string>` = null vs empty HSTRING), crouch flags, character/skin, 4 attributes, `move_state` (0..14 or 16). `operator==` defaulted. |
| `PlayerRestoreState` | `src/game/player_restore.h:17-34` | world path, object name, model, skin, position/scale/dimensions/velocity/acceleration, color/alpha, rotation, `AnimationTrack`, `hidden_nodes`, flags/user_flags, blocking_priority, `next_update` (**remaining seconds**, :28), crouched, eye_level + the three structs above. `PlayerRestoreMode {SavedPosition, KeepAlive}` :9-13. |

### 1.2 Original-code Save/Load (retyped)

- `CInventoryMgr::Save/Load` — decl `src/game/original/InventoryMgr.h:26,29`; impl `src/game/original/InventorySave.cpp` (header comment :1-2 "Selected InventoryMgr.cpp:2843-3061 … Typed values replace message serialization; staging replaces destructive Term-before-read").
  - `valid()` bounds :8-63 (attributes 1..6, slot ranges, multiplier 0..1000, ≤256 keys, strings ≤100 bytes NUL-free, dual-hand rules).
  - `Save` :66-106: needs live owner, refuses during weapon fire callback (:83-87); copies weapon types, raw ammo, keys, items, slot, multiplier.
  - `Load` :108-208: requires live **unassociated** owner (:113); stages a second `CInventoryMgr` (`staged.Init`, `ObtainWeapon` right then left :133-148), raw ammo after recreation (:149-151), keys dropped for KeepAlive (:152-153), items recreated (flashlight child light :162-173), then swap-commit (:186-200), rebind weapons (:202-205), `++revision`.
- `CDestructable::Save/Load` — decl `src/game/original/Destructable.h:14-15`; impl `src/game/original/DestructableSave.cpp` (comment :1-2 "Original Destructable.cpp:1134-1245…").
  - `Save` :61-101 (health sentinel `can_damage ? hp : 1e6` :75). `Load` :103-163: refuses associated player (:112-117), validates SpawnObject (:120-124), assigns raw, `can_damage = hp < 1e6` (:153), clears attacker/hit diagnostics (:154-155), re-inserts intrusive `m_DestructableHead` link if unlinked (:156-161).
- `CPlayerObj::SaveFields/LoadFields/RestoreModelState` — decl `src/game/original/PlayerObj.h:145-150` (flags `restore_initialization`, `restore_fields_loaded` …); impl `src/game/original/PlayerSave.cpp` (comment :1-3 "Selected PlayerObj.cpp:6541-6865 values …").
  - `available()` :55-91 refuses: not live/initialized/in damage callback; CTF flag state (:64-68); active Orb (:69-73); **any active power except Invisible/Invulnerable** (:74-82, "requires unported saved expiration timers"); weapon firing.
  - `SaveFields` :94-150 stores clocks as `x - GetTime()` (:102,107,111). `LoadFields` :152-219 adds destination `GetTime()` back, allocates both HSTRINGs before swap (:166-186), zeroes stat caches (:211-212), no Respawn/ValidateAttributes.
  - `RestoreModelState` :223-260: original Load tail — `SetCharacter`, `reset_character_nodes`, fixed flags without SHADOW (:248-250), zero velocity, zoom off.
- `INITIALUPDATE_SAVEGAME` early return in `CPlayerObj::InitialUpdate` — `src/game/original/PlayerObj.cpp:77-87` (binds aggregates, faction lists, then returns before fresh stats/scent/scheduling). `CBaseCharacter::EngineMessageFn` skips flag/user-flag setup on savegame `:20-28`.

### 1.3 Shell captures (diagnostic, at exit)

- `CBloodServerShell::CapturePlayerInventory/Damage/Fields` — `src/game/original/BloodShells.cpp:39-100`, result records `src/game/original/BloodShells.h:37-60` ("Inventory only; never a successful KeepAliveSave" :44). Cleared in `PreStartWorld` `BloodShells.cpp:14-25`. Called from the message pump on exit `src/game/session.cpp:410-418`.

### 1.4 Engine-level player capture/restore (GameSession)

`src/game/player_restore.cpp`:
- `validate_engine` :10-60 — same world for SavedPosition, single-player only, Caleb model/skin only (:26-34), finite transform/unit quaternion, `next_update` 0..3600, eye level matches crouch, color/alpha 0..1, **refuses dead/dying player** (:46-47), validates animation with a zero advance (:54-58).
- `capture_player` :62-104 — preconditions (:67-77): `!objects.busy()`, alive, `client_movement`, attributes received, client in world, no attachment/world/model body, mass/friction match, ACTIVE, not auto-deactivating, scheduled, no pending door push. Copies record fields (:79-96) incl. `next_update = due - time` (:96).
- `restore_player` :105-220 — requires no client (:116-120); creates `CPlayerObj` with **`Creation::Restore`** (:145) → `MID_PRECREATE/MID_INITIALUPDATE` with reason 3; writes engine record fields directly (:152-167), `ResizeObject` (:168-172), inventory/damage/fields/model-state load (:173-178), `record->due = time + next_update` (:183-184), connects new client token (:185), `OnRestoredClientEnterWorld` (:186-191; shell `BloodShells.cpp:145-178`), client `OnEnterWorld(…, true)`, pump/update/sync weapons/hand models/finish_frame/refresh_attachments (:197-200). Rollback (:133-144) removes every object created since the pre-snapshot — **player-only cleanup, not world rollback**.
- `GameSession::Restoration` in-memory slot: `src/game/server_adapter.h:483-492` (`optional<PlayerRestoreState> saved`, counters, last ObjectId inspection-only).

### 1.5 NEW_LEVEL transaction and rehearsal

- `queue_level_change` `src/game/level_change.cpp:9-29` (one owned pending request, no handles).
- `process_level_change` `level_change.cpp:31-194`: consume intent → `capture_player` (:72) → retain difficulty (:75-81) → `OriginalGlobals source_globals.exchange()` + unbind `g_pServerDE`/`ServerAdapter::active_` (:89-93) → new `GameSession` copying only `resources` (:94-95) → `initialize()` + `load_world(fresh NEW_GAME msg)` + `check_world_initialization` + `restore_player(KeepAlive, start)` + presentation checks (:96-130) → on failure destroy destination under its own adapter and restore source (:131-144) → swap globals, `audio.transfer_output_to` (last fallible step :154-169) → copy VoiceMgr history, world_exit, `world_revision+1`, start name, `m_bFirstWorld=false` (:172-179) → `current.reset()` (source destructors under source globals) → bind destination (:180-192).
- `OriginalGlobals` `src/game/original_globals.h:12-44`: 7 DLink heads + 7 counts (Cabal, Monster, Destructable, Smell, Exit, AmmoPickup, Camera), `g_hActiveCamera`, `AI_Mgr::RuntimeTables` (array of 30 `shared_ptr<CAnim_Sound>`, `src/game/original/AI_Mgr.h:136`), `g_ctf`. `empty()` :37-43 is the cleanup invariant.
- `start_new_game` `src/game/new_game.cpp:8-91`: same pattern (destination built, `run_world`, audio transfer, promote).
- `rehearse_destination` `src/game/destination_trial.cpp:12-143`: same isolation, destination destroyed inside a lambda scope (:62-128); never promotes.
- `decode_world_request` refuses NEW_LEVEL for in-place `load_world` (`src/game/original/session_load.cpp:86-91`); `parse_world_request` rejects any load type other than NEW_GAME/NEW_LEVEL — i.e. no `LOADTYPE_RESTORESAVE`/`RESTOREAUTOSAVE` (`session_load.cpp:56-61`, message "saves/multiplayer are unimplemented").

### 1.6 HMESSAGEWRITE / HMESSAGEREAD implementation

- Handle type: `OpenBlood2::MessageHandle` — 64-bit value identity, not a pointer (`src/game/legacy/basedefs_de.h:29-46,52-53`). Allocator: process-wide atomic `next_message_identity`, never wraps (`src/game/server_adapter.cpp:15-23`).
- Declared SDK surface (`src/game/legacy/server_de.h:25-37`): **only** `StartHMessageWrite`, `WriteToMessageDWord`, `WriteToMessageHString`, `ReadFromMessageDWord`, `ReadFromMessageHString`, `EndHMessageWrite`, `EndHMessageRead`, `ResetRead`; plus `StartMessageToObject`/`EndMessage` (:242-243). Header comment :21-24: absent services "fail to compile".
- Storage: `ServerAdapter::Message` `src/game/server_adapter.h:355-366` = `vector<uint8_t> bytes`, cursor, bad/routed/borrowed flags, sender/receiver/id, and **typed optional side-cars** `immobilize`, `damage`, `key`, `pickup`. Live map `messages_` :411; `max_live_messages = 65536` :299; `MessageStats` :300-304.
- Implementation `server_adapter.cpp`: `StartHMessageWrite` :588-608; `writable/readable` :609-637 (end-of-message rewind :628-629, truncation → bad); `WriteToMessageDWord` :638-658 (LE 32-bit; refused if a typed side-car is set; 64 KiB cap); `WriteToMessageHString` :659-683 (null → `UINT32_MAX`, else u32 length + bytes); `ReadFromMessageDWord` :684-694; `ReadFromMessageHString` :695-720 (≤4096, no NUL, returns caller-owned HSTRING); `EndHMessageWrite` :721-734; **`EndHMessageRead` :735-739 → `unsupported("… nested owned readers are not exposed yet")`**; `ResetRead` :740-746; `StartMessageToObject` :747-766; `EndMessage` :767-784 (synchronous dispatch via `Objects::message`, then erase).
- Typed side-cars: `WriteDamage/ReadDamage` `src/game/server_weapons.cpp:145-197` (requires empty `bytes`, attribution checks); `Write/ReadImmobilize` `server_weapons.cpp:15,31`; `Write/ReadKeyMessage` `src/game/server_keys.cpp:14,34`; `Write/ReadPickupMessage` `src/game/server_pickups.cpp:71,103`.
- **Not declared anywhere in `src/`:** `WriteToMessageByte/Word/Float/Vector/CompVector/Rotation/Object/String/HMessageWrite`, `ReadFromMessage{Byte,Word,Float,Vector,Rotation,Object,String,HMessageRead}`, `WriteToLoadSaveMessageObject`, `ReadFromLoadSaveMessageObject`, `GetNextInactiveObject`. (grep confirmed.)
- Original usage of those primitives across `B2/ObjectDLL` (all calls, not only save): WriteByte 492, WriteFloat 422, WriteVector 320, WriteDWord 219, WriteHString 157, WriteObject 45, WriteWord 31, WriteString 16, **WriteToLoadSaveMessageObject 14 / ReadFromLoadSaveMessageObject 15**, WriteRotation 8, WriteHMessageRead 2.
- `WorldMessage` (load command only) is a separate local byte vector with string/byte writers: `src/game/session.h:47-56`, `src/game/original/session_load.cpp:12-37`; read via bounded `Reader` `src/engine/formats/reader.h` (LE, latched error; no matching Writer class in `src/engine`, test-only writers in `tests/model_fixture.h:8`, `tests/world_tests.cpp:32`).

### 1.7 Current snapshot "format"

In-memory only, owned values, bounded by per-field validation (strings ≤100 B NUL-free, keys ≤256, attributes 1..6, finite vectors via `physics_vector`, unit quaternion ±0.001, next_update ≤3600 s), relative clocks, no engine identities, no version, no serialization/deserialization, no file. Survives unload/reload of the same world (contracts). The only persisted file in the app is `preferences.cfg` (see §5.4).

---

## 2. Original classes: Save/Load kept, stripped, or refused

### 2.1 Grep results in `src/`

- `MID_SAVEOBJECT`/`MID_LOADOBJECT`: constants only `src/game/legacy/serverobj_de.h:9-10`; the **single handler** is `CDestructableModel::EngineMessageFn` `src/game/original/DestructableModel.cpp:411-415` → sets `trigger_error = "Destructible model save/restore is unavailable"`, returns 0.
- `::Save(`/`::Load(`: only `InventorySave.cpp:66,108`, `DestructableSave.cpp:61,103` (+ `PlayerSave.cpp` `SaveFields/LoadFields`). No `Save(HMESSAGEWRITE, DDWORD)`/`Load(HMESSAGEREAD, DDWORD)` signature survives.
- `SaveObjects`/`RestoreObjects`: SDK function pointers `server_de.h:240-241`; thunks `server_adapter.cpp:432-441` → `unsupported()` (:494-499 = `record_unavailable` + `ObjectError::Unsupported` + `DE_ERROR`); service catalogue marks both `Unavailable` (`src/game/service_catalog.inc:141,143`); test `tests/object_contract_tests.cpp:531-532` asserts `DE_ERROR`. **No game code calls them.** Calling one would add to `unavailable_services`, which rejects `run_world`/NEW_LEVEL acceptance (session_start_contracts).
- `PRECREATE_SAVEGAME` / `reason == 3` **refusals** (object creation rejected for Creation::Restore):
  `AIRuntime.cpp:374` (all NPCs: "NPC save restoration is not implemented"), `CameraObj.cpp:79`, `ClientExplosionSFX.cpp:28`, `ClientSplatFX.cpp:26` (≠NORMAL), `ClientLightningSFX.cpp:12`, `CinematicTrigger.cpp:38`, `ClientLightFX.cpp:89`, `ClientWarpGateSFX.cpp:9`, `DestructableModel.cpp:419`, `DestructableBrush.cpp:17`, `Corpse.cpp:78` (≠NORMAL), `DetailSprite.cpp:36`, `FlagObjects.cpp:12`, `Gib.cpp:131` (NORMAL/WORLDFILE only), `KeyFramer.cpp:18`, `KeyPickup.cpp:10`, `PickupObject.cpp:29`, `RandomSoundFX.cpp:53`, `PolyGrid.cpp:31` (≠NORMAL), `SkyObjects.cpp:8`, `Sound.cpp:16`, `SoundFX.cpp:57`, `SparksObj.cpp:36`, `Tchernotronic.cpp:29`, `TripLaser.cpp:27`, `Spawner.cpp:45`, `Door.cpp:257`, `Trigger.cpp:226` (all in `src/game/original/`).
- `INITIALUPDATE_SAVEGAME` branches that *do* exist (skip fresh setup, as in source): `PlayerObj.cpp:22,83`, `AIRuntime.cpp:579`, `Tchernotronic.cpp:48`, `TripLaser.cpp:42`, `VolumeBrush.cpp:91` (no surface republish), `PolyGrid.cpp:60` (no Setup), `Debris.cpp:41`, `DestructableBrush.cpp:19`. `B2BaseClass` only runs `InitialUpdate()` for WORLDFILE (`B2BaseClass.cpp:53`).
- Explicit "save/load unavailable" comments: `DetailSprite.h:6`, `ExitTrigger.h:4`, `TripLaser.h:5`, `Spawner.h:5`, `RandomSoundFX.cpp:1`, `Tchernotronic.h:6`, `PathMgr.h:19`, `Movement.cpp:96`, `Projectile.cpp:91`, `Sound.cpp:17`, `SoundFX.cpp:58`, `session_load.cpp:3`.
- `USRFLG_SAVEABLE` (bit 3) still set by `CBaseCharacter` (`PlayerObj.cpp:26-28`, value `1u<<3`), `Tchernotronic.cpp:68`, `TripLaser.cpp:142`, `SeeingEye.cpp:22`, `VolumeBrush.cpp:90`. Original sets it in 27 files (`B2/Shared/ClientServerShared.h:18`) — the port does not consistently preserve this "saveable" marking.

### 2.2 Original classes that had `MID_SAVEOBJECT` (53 files in `B2/ObjectDLL`) vs. port

All of the following have a same-named port file whose Save/Load was **not imported**: AI_Mgr, AncientOne, CinematicTrigger, ClientLightningSFX, CivilianAI, ClientExplosionSFX, CameraObj, ClientLightFX, CultistAI, ClientParticleStreamSFX, ClientWarpGateSFX, ConversationTrigger, DestructableModel (refusal handler), EnhancedGideon, Fanatic, DetailSprite, DestructableBrush, Debris, Door, ExitTrigger, ItemPickups, KeyFramer, Gideon, Naga, ObjectivesTrigger, RandomSoundFX, RotatingDoor, SeeingEye, Spawner, PickupObject, SoundFX, Trigger, Projectile, Tchernotronic, Rotating, UndeadGideon, VolumeBrush, WorldModelDebris, BoneLeech, TripLaser, Corpse, Movement, ZealotAI. Retyped subsets: Destructable, InventoryMgr, PlayerObj. No same-name port file (merged elsewhere): BaseCharacter (→ PlayerObj.cpp `CBaseCharacter`), AncientOneTentacle, ClientLaserBeamSFX, Rain, TheHandAI, thief, drudgepriest (→ LaterEnemies/Thief.h).
Largest original Save bodies (WriteToMessage* call counts per file, incl. non-save writes): PlayerObj 178, ClientWarpGateSFX 111, ClientExplosionSFX 94, AI_Mgr 46, InventoryMgr 41, VolumeBrush 40, Projectile 37, DestructableModel 33, Trigger 30, Destructable 30.

### 2.3 Original/later-engine reference flow (context only)

- Blood2 server: `B2/ObjectDLL/BloodServerShell.cpp` — `CMSG_SAVEGAME` :400-430 (stores client save data on player, `SaveGame(SAVETYPE_CURRENT|AUTO)`); `LoadWorld` restore branch :900-990 (`LOADTYPE_RESTORESAVE` → world loaded **without** `LOADWORLD_LOADWORLDOBJECTS`, then `RestoreGame` → `RestoreObjects(path, 0, RESTOREOBJECTS_RESTORETIME)`; NEW_LEVEL → `KeepAliveSave` + `KeepAliveLoad`); `SaveGame` :1004-1064 (all active **and inactive** objects with `USRFLG_SAVEABLE`, players optional, console+portals flags); `RestoreGame` :1075-1094; `KeepAliveSave/Load` :1142-1210 (`Keep.sav`, players only, dwParam=1). Paths `Save\Current\Current.sav`, `Auto.sav` (:41-43).
- Client save data: `B2/ClientShellDLL/BloodClientShell.cpp:6351-6380` writes night-goggles, wonky/blind timers, fade in/out/value, **music level**, last load type, game type, **objectives title/text**; `LoadClientData` :6391-6430. Slots: `B2/ClientShellDLL/LoadSave.h:23-25` (`SLOT_CURRENT -2`, `SLOT_QUICK -1`).
- Later LithTech serializer `LT/server/src/game_serialize.cpp`: `sm_SaveObjects` :1091+ (serialize IDs = list index :1139, writes game time :1150, console vars :1155); `sm_RestoreObjects` :1231+ (restores game time when `RESTOREOBJECTS_RESTORETIME` :1249-1251; pass 1 creates every object with `PRECREATE_SAVEGAME` :712 and `OBJECTCREATED_SAVEGAME`=INITIALUPDATE_SAVEGAME :723-724 from saved engine fields; assigns serialize IDs :1301; pass 2 `OnLoad` (MID_LOADOBJECT) per object :914; then attachments :1326 and interlinks :1329). Object refs inside game data use LoadSave object IDs. Sounds are **not** serialized; e.g. original `SoundFX::Load` restarts its sound (`B2/ObjectDLL/SoundFX.cpp:538-561`).

---

## 3. Engine object model (`src/engine/objects.*`)

### 3.1 Identity

- `ObjectId {slot, generation}` `src/engine/objects.h:78-86`. Slots are **reused** (first free slot, `objects.cpp:350-353`); generation incremented on reclamation (`objects.cpp:750`); `get(ObjectId)` checks generation (`objects.cpp:578-583`).
- `HOBJECT = DObject*` (`basedefs_de.h:50`, `DObject{ObjectId}` `objects.h:309-312`). Tokens are append-only `handles_` (`objects.h:276`, `objects.cpp:354-355`) → **never reused within one `Objects`**; hard cap 65,536 creations per session (`objects.cpp:327`). Lookup `identities_` map (`objects.h:277`, erased at reclamation `objects.cpp:748`).
- Each `GameSession` owns a fresh `Objects` → identities restart per world (level change/new game build a new session). Same for HSTRING tokens (`string_tokens_`, cap 65,536, `server_adapter.cpp:502-508`), lists (`list_tokens_` :540-546), client tokens (`server_clients.cpp:9-19`).
- `HATTACHMENT = uint64` session counter `next_attachment_` (`objects.h:271`, `basedefs_de.h:58`).
- Occupied-slot index `records_` (sorted by slot, published before constructor; `objects.h:274`, `objects.cpp:360-364`); `snapshot()` returns a fresh copy (`objects.cpp:604-609`). `next_alive()` (slot order, Alive only) backs `GetNextObject` (`objects.cpp:616-631`, thunk `server_adapter.cpp:170-180`, active-only).

### 3.2 Creation / lifetime

- `Creation {Runtime, World, String, Restore}` = reasons 0..3 (`objects.h:51-57`; name "Restore callback" `objects.cpp:164`).
- `Objects::create` `objects.cpp:320-480`: schema/property validation, slot+token, constructor, nested property context, `MID_PRECREATE` with `(float)reason` (:382), consume `ObjectCreateStruct` after precreate (:395); for OT_CONTAINER/OT_WORLDMODEL bind first same-named BSP model and build `ContainerShape` (:396-445, posed shell + immutable base/basis :441-443); OT_SPRITE load (:446-456); OT_MODEL load (:457-467); then `life=Alive`, `set_next_update(m_NextUpdate)` (**relative delay**) and `MID_INITIALUPDATE` with reason (:472-477).
- `Life {Constructing, Alive, Pending, Destroying}` (`objects.h:58-64`). `remove()` `objects.cpp:642-665`: idempotent, deletes attachments touching it, marks Pending, unschedules, queues. `finish_frame()` `:714-755`: per pending: break links → `MID_LINKBROKEN` to owners (:727-738), destructor under callback depth, erase index/identity, reset slot, ++generation. `clear()` `:756-767` (teardown). `busy()` `objects.h:209-212` = callback depth / advancing / finishing / clearing — the guard every save/restore entry point uses.
- `message()` `:768-779` / `engine_message()` `:780-791` (touch suppressed when inactive). `dispatch()` `:291-297` calls `BaseClass::_EngineMsgFn` — a `MID_SAVEOBJECT/MID_LOADOBJECT` dispatch would pass `void* data` = pointer to an `HMESSAGEWRITE` value (handle is a value type now).

### 3.3 Scheduling and clock

- `time_` (double) and `frame_time_` private (`objects.h:285`), accessors `time()`/`frame_time()` (:244-251). **No setter**; only `advance(seconds)` (`objects.cpp:679-713`). `GetTime()` thunk returns `float(objects_.time())` (`server_adapter.cpp:106-110`); `GetFrameTime()` :111-115.
- `set_next_update` `objects.cpp:666-678`: `due = time_ + delay` (**absolute**), `scheduled = delay > 0`. `advance()` runs due ACTIVE objects once, MID_UPDATE return ignored (:703-710); FLAG_REMOVEIFOUTSIDE culling (:692-699); auto-deactivation (:700-702).
- Deactivation: `deactivate_at` absolute (`src/game/server_activation.cpp:37-38,90`), `activity` OBJSTATE_* (`objects.h:171`, values `basedefs_de.h:139-140`).
- World time: `ServerAdapter::world_time() = objects_.time() - world_time_origin_` (`server_adapter.h:155-158`, origin :349, reset :173). `GetWorldTime` :159-163.
- Original game code calls `GetTime()` ≥136 times in `src/game/original/*.cpp` and stores many absolute deadlines (e.g. `AI_Mgr::m_fLastUpdate` `AI_Mgr.h:150`). **A whole-world restore needs either a restorable engine clock (LithTech `RESTORETIME` semantics) or per-field rebasing.** Existing player path uses rebasing (offsets).

### 3.4 Links, attachments, lists

- Inter-object links: `links_` vector of (owner,target) HOBJECT pairs (`objects.h:280`), `link/unlink/link_count` `objects.cpp:792-812`; thunks `server_adapter.cpp:422-431`. Broken links notify the owner (`objects.cpp:727-738`).
- Attachments: `attachments_` map HATTACHMENT → `Attachment{id,parent,child,node,offset,rotation,resolved}` (`objects.h:87-95,270`); `attach()` `src/engine/attachments.cpp:6+` (visual children only: no solid/gravity/rayhit, no chains); refreshed each step (`session.cpp:746`). Record field `attachment` (`objects.h:147`).
- ObjectLists (`FindNamedObjects`, `CreateObjectList`) are adapter-owned (`lists_`, `server_adapter.h:412-413`; `make_list/add/remove/release` `server_adapter.cpp:540-587`); usually transient.
- Original intrusive DLink lists (7 heads, see §4.9) are rebuilt by constructors/`InitialUpdate`/`Load` (e.g. `DestructableSave.cpp:156-161`, `SmellHint.h:15-31`); **order = insertion order**.

### 3.5 Per-object engine state (`ObjectRecord`, `objects.h:114-183`) and save need

| Field(s) | Line | Save need |
|---|---|---|
| `id`, `handle`, `type` (HCLASS), `game` (`unique_ptr<BaseClass>`) | 116-119 | class name + saved index (remap); game object via class Save/Load |
| `state` (`ObjectCreateStruct`: type, flags, pos, scale, rotation, container code, user data, filename, skin, name, next update, deactivation time) | 120; struct `basedefs_de.h:106-118` | **yes** (LithTech saves these engine vars) |
| `properties` (authored map), `reason` | 121-122 | authored props needed only for world objects re-created from DAT; restore uses reason 3 |
| `life` | 123 | only Alive saved; Pending excluded (LithTech skips "going away") |
| `color`, `alpha` | 124-125 | yes (model RGBA, sprite multiplicative) |
| `dimensions` | 126 | yes (collision half-extents; differs from model dims) |
| stair/overlap/placement diagnostics, `placement_nudge(s)`, `last_step_height`, `last_overlap_depth` | 127-133 | no (diagnostic) — but placement nudges are already applied to pos |
| `velocity`, `acceleration` | 134 | yes |
| `contact_normal/polygon/object`, `grounded`, `last_impact_*`, `impact_notifications` | 134-141 | transient (re-queried); contact_object is a handle |
| `world_body`, `character_body`, `model_body`, `body_properties` (mass/friction/pushable/rotating_debris/polygon_bounds `src/game/mover_messages.h:5-11`), `blocking_priority` | 142-144 | yes (or re-derived by class Load calling Configure*Body) |
| `body_steps/pushes/result` | 145-146 | diagnostic |
| `attachment` | 147 | yes, via attachment table |
| `client_movement` | 148 | re-established at client association |
| `model` (shared resource), `sprite`, `sprite_track {ms, frame, has_frame}` (`src/engine/sprite.h:13-18`) | 149-151 | resource path from `state.m_Filename`; **sprite_track yes** |
| `container_` (bound shape), `world_model_base`, `world_model_basis`, `world_model` (posed shell), `rotation_steps`, `world_model_index/candidates` | 152-164,182 | base re-bound from DAT by name; posed shell regenerable from base+basis+pos/rot (`src/game/server_rotation.cpp:99-100,197`); save pos/rot |
| `mover_*` counters, `mover_blocker` | 165-166 | diagnostic/handle |
| `animation` (`AnimationTrack {animation, time, looping, done, first, revision}` `src/engine/model.h:86-99`) | 167 | **yes** (LithTech saves model anim trackers) |
| `hidden_nodes` | 168 | yes |
| `model_error` | 169 | no |
| `user_flags` | 170 | yes |
| `activity`, `auto_deactivate`, `deactivation_period`, `deactivate_at` (abs) | 171-173 | yes (rebased) |
| `activation_pings`, `activation_probe` | 174-175 | diagnostic |
| `due` (abs), `scheduled`, `updates` | 176-178 | **yes** (rebase `due`; LithTech stores NextUpdate) |

Engine-wide (Objects): `time_`, `frame_time_`, `links_`, `attachments_` + `next_attachment_`, `pending_` (must be empty at a save boundary), `diagnostics_` (no). Also `ServerAdapter::gravity` (`server_adapter.h:345`, `SetGlobalForce`).
Collision: `CollisionWorld` is static per DAT (`src/engine/collision.h:48-58`), only work counters mutate → rebuild, no save.

---

## 4. Runtime state outside objects

### 4.1 Audio voices (`src/engine/audio.*`, `src/game/server_audio.cpp`)

- `AudioVoice {id, owner HOBJECT, request (resource, speaker HOBJECT, space, radii, volume, loop, streaming, priority, reverb), clip, position, cursor(double), left/right, done, speaker_lost, world_owned, auto_remove, loops, error}` `src/engine/audio.h:46-60`; mixer `voices_` map `:113`, ≤128 voices (`audio.cpp:246`).
- `SoundId` from process-wide `next_sound` (`audio.cpp:22`, "never reset on unload/reload or a new mixer", :266). Game objects retain `SoundId`s in ≥34 member declarations in `src/game/original/*.h` (e.g. `CAnim_Sound::m_hVoxSound` `Anim_Sound.h:32`, `CBloodClientShell::goggles_loop/eye_loop/item_voice` `BloodShells.h:112,132`).
- Ownership modes: object loop (`PlayObjectLoopSound` `server_audio.cpp:4-16`), character one-shot `retain` → `auto_remove = !retain` (:17-33), cinematic (:34-48), client/world-owned one-shots (:49-65, `world_owned=auto_remove=true`), fixed-position (:66-75), local (:76-89), detached (:90-103), world-owned loops transferred from consumed `Sound` objects (:104-125; session entry state `WorldSound`, `session.h:16,35`). Reaping: non-world-owned voices die with their owner; speaker loss marks done; auto-remove on completion (`server_audio.cpp:156-186`).
- `AudioMixer::clear()` `audio.cpp:522-536` (voices, music loop, instruments, caches); `transfer_output_to` `:498-521` moves only SDL output + gains (voices stay with their world).
- Implication: original Load re-starts sounds (no cursor); world-owned transferred loops have no game object → need their own save record (resource, position, radii, loop) or re-derivation from the DAT `Sound` rows.

### 4.2 Music

- Client logic `CMusic` (`src/game/original/Music.h:31-50`): `logical_level`, counters, `last_command`. Engine transport `AudioMixer::music_loop` (`ImaLoopTransport`, `src/engine/ima_transport.h:9-90`: pending replacement, level, prepared programs, clocks, bank, next_due …) and `instruments` (`DlsSampler`, `audio.h:100-101`, ids never reused `src/engine/dls_sampler.h:141`). `MusicCatalog` is rebuilt from world properties at load (`session.cpp:329-333`). Original saved only the music **level** (§2.3) → restart transport from `logical_level` on load.

### 4.3 Client-side FX collections (all on `ServerAdapter`, `server_adapter.h:103-146`; cleared by `reset_world_time` :169-206; stepped in `GameSession::step` `session.cpp:756-772`)

| Collection | Owner-bound? | Notes |
|---|---|---|
| `polygrids` (`src/game/polygrids.h:6-30`) | yes, `owner HOBJECT` :8 | published by PolyGrid/VolumeBrush InitialUpdate, skipped on SAVEGAME (`PolyGrid.cpp:60`, `VolumeBrush.cpp:91`) → must be republished after load |
| `particle_streams` (`particle_streams.h:6-31`) | yes :8 | own RNG :32 |
| `warp_gates` (`warp_gates.h:6-36`) | yes :9 | own RNG :36 |
| `object_particles` (`object_particles.h:6-32`) | yes :8 | Gideon shield/powerup trails etc. |
| `light_effects` (`vector<CLightFX>`, `server_adapter.h:237`; `CLightFX{source HOBJECT, radius, off_start_time}` `original/LightFX.h:7-12`) | yes | ClientLightFX/GlowingLight republish |
| `camera_effects` (`vector<CameraView>` :243; `CameraView{object, settings, position, rotation}` `camera_messages.h:11-17`) | yes | CameraObj |
| `sky` (`SkyState`, `src/engine/sky.h:10-20`, stores `ObjectId`s) | yes | SkyPointer/DemoSkyWorldModel |
| `explosion_sprites/models/lights/rings/flames/fragments`, `sparks`, `lightning`, `blood_spurts`, `blood_splats` (persistent oriented sprites), `weapon_sprites`, `laser_beams`, `flesh_gibs` | detached (positions only; `flesh_gibs` takes owner only at publish) | transient cosmetic; retail lost these on load. Each has its own `std::minstd_rand random_{1}` and monotonic `next_id_`/`next_event_` |
- Report vectors (`debris_effects`, `explosion_effects`, `scene_explosions`, `brush_destructions`, `spawn_reports`, `pickup_reports`, `projectile_reports`) are diagnostics.

### 4.4 Camera, fade, HUD, objectives, other client shell state (`CBloodClientShell`, `src/game/original/BloodShells.h:76-193`)

- Camera: `view` (CameraView, :146), `death_camera` (:149), `rotation` (:152), `camera_position`/`listener_position`; `g_hActiveCamera` global (`CameraObj.cpp:9`); CameraObj objects/list.
- Screen fade: `CScreenFade fade{fade_in, fade_out, value, messages}` (`ScreenFade.h:5-14`), stepped `session.cpp:724`. Original saved fade in/out/value.
- HUD: `stats` (`PlayerStats`, :154, server-published; player Load zeroes last-sent caches so republish occurs `PlayerSave.cpp:211-212`), `boss_health`/visibility (:177-179), crosshair/zoom/FOV (:121-127), night goggles/eye/creature/blind state and loops (:110-114), `ctf` (:166-175).
- Objectives: `objectives` (`MissionObjectives`), `objectives_resolved` (:181-184). Original saved objectives title/text strings in client save data.
- Conversation: `communication` (`CCommLink`: start/text/length/ratio timers, voice SoundId, portrait, `src/game/original/CommLink.h:10-18`).
- Voice manager: `voice_manager` (CVoiceMgr: `played`, `on`, `random_`, group `last`); NEW_LEVEL copies via `InheritHistory` (`src/game/original/VoiceMgr.cpp:82-93`, called `level_change.cpp:172`).
- Credits roll (`m_bRollCredits`, `m_Credits` :90-98), `world_exit` (:157-165), view weapons `weapons` ×2 (`CViewWeapon`, own RNG `ViewWeapon.h:47` that generates shot seeds `ViewWeapon.cpp:128-130`), `move_mgr` (CMoveMgr prediction; reset by ForcePos on restore), input edge latches (never saved), `m_bFirstWorld/m_bFirstUpdate`, `restored_entry` (:86).
- `OnExitWorld` resets almost all of this (`BloodShells.cpp:299-345`).

### 4.5 Message queues

- Object messages: synchronous, live only during dispatch (`messages_`, §1.6) → empty at a frame boundary (check `message_count()==0`, `server_adapter.h:295-298`).
- Client events: `clients_[h].events` (`vector<PlayerEvent>`, `server_adapter.h:369-375`), drained by `pump_player_messages` (`session.cpp:367-425`, ≤8 rounds); `take_client_events` `server_clients.cpp:155-162`.
- Pending level change `GameSession::pending_level_change` (`server_adapter.h:466`) — must be empty/refused at save.

### 4.6 RNG state

- Gameplay: `ServerAdapter::random_` `std::minstd_rand{1}` (`server_adapter.h:348`), reseeded to 1 per world (:201); `RandomInt` `server_audio.cpp:129`, `RandomFloat` `server_lights.cpp:12-13`. AI weapon seeds derived from it (`AIFire.cpp:238`, `Gideon.cpp:294`, `Naga.cpp:189`, `UndeadGideon.cpp:358`); per-shot `std::minstd_rand random(request.seed)` local (`Weapon.cpp:57`).
- Per-object RNGs: `CPlayerObj::idle_random_` (`PlayerObj.h:268`), `COrbProjectile::cosmetic_random_` (`OrbProjectile.h:32`), `CWeapon::impact_random` (`Weapon.h:95`), `CViewWeapon::random_` (`ViewWeapon.h:47`), plus FX-instance RNGs inside `original/*FX.h` (BloodSplatFX, ExplosionFlameFX, ExplosionFragFX, ExplosionModelFX, LightningFX, ObjectParticleFX, ParticleExplosionFX, ParticleStreamFX, SparksFX, WarpGateFX, WeaponFX).
- FX collection RNGs (§4.3), VoiceMgr `random_`, music `ImaSectionProgram::random_state` (`ima_program.h:12`), menu audio random (`frontend.cpp:70`).
- `std::minstd_rand` is streamable (`operator<<`/`>>`), so exact state capture is cheap.

### 4.7 Process-wide identity counters (must never rewind)

`next_message_identity` (`server_adapter.cpp:15`), audio `next_sound` (`audio.cpp:22`), `DlsSampler::next_id_`. Per-adapter FX `next_id_` (survive unload, "IDs deliberately survive world changes" e.g. `blood_spurts.cpp:114`).

### 4.8 Console variables

`variables_` map (`server_adapter.h:376-377`): `GAME_DIFFICULTY`, `GAME_GAMETYPE` (set by `set_game_settings` `server_adapter.cpp:468-477`), `GAME_WORLDNAME` (`session.cpp:82`), `GAME_GOTOSTARTPOINT` (`BloodShells.cpp:130,176`), world properties (MusicDirectory, fog, sky fog, cutscenes … `WorldProperties.cpp:57-74`). Original `SAVEOBJECTS_SAVEGAMECONSOLE` saved console state.

### 4.9 Original globals / shared tables ("seven list/count pairs", AI table owners)

Defined at: `AI_Mgr::m_CabalHead/m_MonsterHead` + counts `AIPerception.cpp:11-12`; `AmmoPickup::m_PickupHead/m_dwNumPickups` `AmmoPickups.cpp:2-3`; `CameraObj::m_CameraHead/m_dwNumCameras` + `g_hActiveCamera` `CameraObj.cpp:7-9`; `CDestructable::m_DestructableHead/m_dwNumDestructables` `Destructable.cpp:11-12`; `ExitHint::m_ExitHead/m_dwNumExits` `ExitHint.cpp:10-11`; `SmellHint::m_SmellHead/m_dwNumSmells` `SmellHint.cpp:11-12`; `g_ctf` `FlagObjects.cpp:6`; AI runtime tables (anonymous-namespace `tables`, `AIRuntime.cpp:13`, `ExchangeRuntimeTables` :87-90, `ResetRuntimeTables` :91-94; 30 slots `AI_Mgr.h:133-136` covering Cultist m/f, Civilian m/f, Gideon, Fanatic, Prophet, SoulDrudge, Shikari, … Ishmael/REV). Tables hold `CAnim_Sound` with live `m_hVoxSound` voices. Swapped as a unit by `OriginalGlobals` (§1.5); reset at unload (`session.cpp:668-672`). Any new mutable global must join this audit (destination_trial_contracts).

### 4.10 Path manager and smell trail

- `CBloodServerShell::paths` (`PathMgr`, `BloodShells.h:66`) cleared in `PreStartWorld` and rebuilt from PathPoint objects in `PostStartWorld` (`BloodShells.cpp:14-33`); owned snapshots ("Save/load remains unavailable" `PathMgr.h:19`) → re-derivable after load (requires PathPoint objects to exist).
- Smell trail: `SmellHint` engine objects (B2BaseClass, `MAX_SMELL_LIFE 20` s, intrusive list, `SmellHint.h:9-44`), player `m_fSmellTime/m_fSmellDelay` (saved as offset in `PlayerState`), AI `smell_cursor` HOBJECT (`AI_Mgr.h:255-256`), player `initial_smell_/last_smell` handles (`PlayerObj.cpp:104-109`). List order matters for `LastLive/NextLive`.

### 4.11 Server shell / session-level state

`CBloodServerShell` (`BloodShells.h:16-75`): `start_point_name`, `world_started`, `inspection_start`, `updates`, captures. `GameSession` (`server_adapter.h:432-515`): `world_path`, `world_revision` (presentation cache key; bumped on promotion `level_change.cpp:176`, `new_game.cpp:77`), `entries` (source accounting, required by `check_world_initialization` `src/game/world_validation.cpp:57`), `local_client`, `state`, reports.

### 4.12 Handle-bearing game fields needing remap (approximate grep counts in `src/game/original/*.h`)

≥88 `HOBJECT` member declarations (e.g. `AI_Mgr::m_hTarget/m_hTrackObject` `AI_Mgr.h:604-605`, `searched_ammo_target` :253, `last_corpse` :325, `heard_sender` :504), 13 `HSTRING`, 34 `SoundId`, 6 `HATTACHMENT`. `AI_Mgr` alone has ~172 member declarations (port adds diagnostics beyond the original Save subset).

---

## 5. Save/Load menu screens (UI-M5) and demo checkpoint hook

### 5.1 Model

- `MenuPage::Save`, `MenuPage::Load` (`src/game/menu.h:19-20`). `MenuAction` has **no Save/Load action** (`menu.h:23-31`: None, Resume, NewGame, Credits, Quit, Settings); `MenuCommand {action, character, difficulty, story}` (:38-43) has no slot field.
- Build (`src/game/original/MainMenus.cpp`): Solo page adds `IDS_MENU_SINGLEPLAYER_LOADGAME` (529) → `MENU_CMD_LOAD_GAME_MENU` (102) and `IDS_MENU_SINGLEPLAYER_SAVEGAME` (530) → `MENU_CMD_SAVE_GAME_MENU` (103) (:81-82; IDs `MenuIds.h:25-26,42-43,54-55`); demo omits them (:79-80) and redirects Save/Load pages to Main (:57-58).
  - Load page :128-131: `IDS_MENU_LOADGAME_RESTART` (547) and `IDS_MENU_LOADGAME_QUICKLOAD` (548) with command -1, then **falls through** to Save page :132-135: 8 literal rows `"Slot N - unavailable"`, `enabled=false`, command -1.
  - Disabling: post-pass :142-160 — `command == -1` → `enabled=false` (:151-156); `MENU_CMD_SAVE_GAME_MENU` row enabled only `in_world && alive` (:158-159). Load menu row stays enabled (navigates to a fully disabled page).
  - Activation: `MENU_CMD_SAVE_GAME_MENU` → `build(Save)` :331-333; `MENU_CMD_LOAD_GAME_MENU` → `build(Load)` :334-336. Title art `savegame.pcx`/`loadgame.pcx` :470-473 (`menu_resources.cpp:47,61`, `menu_canvas.cpp:67`).
- Rendering (`src/game/menu_canvas.cpp`): Load page replaces cres.dll `%1!s!` insertion in Restart/Quickload strings with `"unavailable"` (:149-158, explicit error on unsupported syntax); footer `"Saving and loading are not available yet."` on both pages (:215-216).
- Hook points to wire save/load:
  1. Give slot rows real commands/labels (MainMenus.cpp:128-135, enable logic :143-160) and add a `MenuAction::SaveGame/LoadGame` + slot in `MenuCommand`.
  2. Consume in `src/app/frontend.cpp` menu-command block :244-300 (outside SDL event dispatch; mirrors NewGame handling :271-300: `start_menu_world` → reset selection/controlled/inspector/input, `previous = SDL_GetTicksNS()`).
  3. Save must run at a quiescent boundary (after `step_app_session`, same place as `process_level_change` :467) or while paused (menu open pauses simulation: `input.paused(menu)` gate :424).
  4. `src/app/main.cpp` (direct/smoke path) has its own loop and level-change point `main.cpp:765`.

### 5.2 Demo "future checkpoint hook"

- Contract: `Documentation/contracts/demo_contracts.md:96-116` ("Temporary death retry and future saves") and `demo_menu_contracts.md:111-116`.
- Code: Enter-after-death sets `MenuInput::demo.restart` (`src/app/menu_input.h:134-138`, comment "future checkpoint restore replaces the app handler") or `DemoFlow::restart` (`src/app/demo_flow.h:17,72-75`); consumed in `frontend.cpp:224-243` → `GameSession::start_new_game(current world, difficulty, Caleb)`; failure retains the dead session.
- Rules for the replacement: checkpoint selection + transactional restoration; no direct writes to `m_bDead`/inventory/aggregates; capture only after successful entry/transfer; prepare objects, original globals, client state, clocks, audio before promotion; keep source on failure; reset input once on success; keep a fresh-world fallback.

### 5.3 Inspector hooks (diagnostic)

`Inspector::capture_player_request/restore_player_request/keep_player_request` (`src/app/inspector.h:111`, handled `src/app/inspector.cpp:355-405`, buttons ~:1603-1608). App smoke `--restore-smoke`/`--keepalive-smoke` `src/app/main.cpp:152-170`.

### 5.4 Existing persistence precedent

`preferences.cfg` under `SDL_GetPrefPath("OpenBlood2","Blood2")` (`src/game/menu_preferences.cpp:21-31`), versioned keys, transactional parse, **atomic write via unique sibling temp + `SDL_RenamePath`** (:115-150). Natural home/pattern for save files.

---

## 6. Session structure and the owning frame boundary

- `GameSession` members (`server_adapter.h:432-515`): `resources`, `models`, `sprites`, `collision`, `objects`, `server` (ServerAdapter, binds `g_pServerDE` in `bind()`), `server_shell`, `music_catalog`, `client_shell`, `local_client`, `world`, `entries`, `world_path`, `world_revision`, reports, `pending_level_change`. Non-movable in practice (stable addresses; contracts forbid moving a session). `initialize()` :503-513 wires stores and registers 192 original classes (`src/game/registration.cpp:1295-1407`).
- **Load world** `session.cpp:58-366`: busy guard; `decode_world_request` + `prepare_world` (CPU DAT+collision, `src/game/world_preparation.h:7-12`); `unload_world`; install collision/world; `PreStartWorld` (:83); `set_game_settings`; create every DAT object with `Creation::World` (:131) and classify into `EntryState` (`session.h:13-25`); `finish_frame` (:307); NPC initial overlap resolution (:310-319); `PostStartWorld` (:320, path list); music catalog (:329-333). **There is no "geometry only / skip world objects" mode** (original `LOADWORLD_LOADWORLDOBJECTS` omitted on restore).
- **Player entry** `enter_player` :427-498 (connect client → `OnClientEnterWorld` → `CreatePlayer`/`Respawn`/`GoToStartPoint` → client `OnEnterWorld` → pump/update/sync weapons; rollback of new objects on failure).
- **Startup check** `run_world` :568-654 (session_start_contracts): bound + idle; no pending level change; `check_world_initialization` (requires one entry per DAT object `world_validation.cpp:57`); NPC failures; enter player; `check_world_presentation`; music failures; `state = Ready`.
- **Unload** :655-696 (leave player, `objects.clear()`, reset AI tables/g_ctf if bound, `reset_world_time`, clear caches, `++world_revision`).
- **Step** :697-778 order: `server_shell.Update` → `advance_audio` → `objects.advance` → `advance_physics` → `advance_projectiles` → `advance_models` → client pump/movement/update/fade/credits/zoom/use/weapons → `damage_containers` → `finish_frame` → `refresh_attachments` → `advance_lights` → flashlight → all FX `advance(objects.time())`.
- **Owning frame boundary** (where a load would replace the session): `src/app/frontend.cpp:424-482` — after `step_app_session` (:424) and demo-end handling, `GameSession::process_level_change(game, error)` (:467) swaps `unique_ptr<GameSession>`; app then clears `selection`, `controlled`, inspector world, input and resets `previous` (:468-479). Menu-driven replacements (`start_new_game`) happen earlier in the same frame (:224-300). Direct path: `main.cpp:765`. Preconditions used everywhere: `!objects.busy()`, `ServerAdapter::active_ == &server`, `g_pServerDE == &server`, `server.bound_`.
- `waiting_for_player` gate: no simulation until a player is in world (`src/app/session_playback.h:35-44`); frame delta capped at 100 ms (`app_frame_delta` :27-31).

---

## 7. Test infrastructure for round-trip tests

- Layout: one executable per area under `tests/`, case name as argv[1] and a fixture/data root as argv[2]; `std::map` dispatch in `main` (e.g. `tests/player_restore_tests.cpp` tail), `CHECK` macro printing `__func__:__LINE__`. Registered in `tests/CMakeLists.txt` with labels.
- Relevant suites: `objects.*` (`tests/CMakeLists.txt:24-30`, incl. `unsupported_teardown`, `message_churn`, `message_limit`, `scheduling`, `deferred_removal`, `links_lists`); `object_snapshots.*` (:32-38); `inventory_state.{roundtrip,malformed,rollback,capture}` (:1048-1057); `destructable_state.{roundtrip,behavior,sentinel,failures,capture}` (:1060-1068); `player_state.{roundtrip,clocks,initialization,failures,capture}` (:1072-1081); `character_nodes.*` (:1084-1092); `player_restore.{roundtrip,failures,lifetime}` + `.private` + `gpu.player_restore` (`--restore-smoke`) (:1096-1111); `player_keepalive.{named,fallback,failures}` + `.private` + `gpu.player_keepalive` (:1113-1128); `world.{session_start,level_change,destination_trials,destination_lifetime,session_lifecycle,…}` (:121-127) and `world.level_change_private`/`destination_trial_private` (:129-137); `train_session.audit` + `private_30/60` (:2127-2134, continuous route through NEW_LEVEL); `menus.*` (:2532-2556); demo label tests.
- Fixtures: synthetic REZ trees built in code (`tests/player_fixture.h:19-90`, `setup()` :66+, synthetic Caleb ABC/skins/weapons), synthetic DAT writers (`tests/world_tests.cpp:32`, `tests/model_fixture.h:8`). No commercial data committed.
- Labels/presets: `headless`, `private` (needs `-DBLOOD2_TEST_DATA_DIR`), `gpu` (needs `-DBLOOD2_GPU_TESTS=ON`, `RUN_SERIAL`), `demo` (needs `-DBLOOD2_DEMO_DATA_DIR`). `ctest --preset dev -L headless`; CI test presets include `headless` and exclude `gpu|private` (`CMakePresets.json:189-226`). Configure presets `debug`/`dev` (inspector), `release` (no inspector) (`CMakePresets.json:9-41`).
- Sanitizers: option `BLOOD2_SANITIZERS` (`CMakeLists.txt:13`) → `-fsanitize=address,undefined` per target (`cmake/ProjectOptions.cmake:10-13`); recipe in `Documentation/testing.md` "Sanitizers" (Clang, `-DBLOOD2_INSPECTOR=OFF`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`). Only a launcher sanitize preset exists (`CMakePresets.json:68-83`). M2-GPU-001 NVIDIA LeakSanitizer failure remains open. AGENTS.md requires object-contract + Clang sanitizer runs for lifetime/dispatch changes and an inspector-free Release build for app lifetime changes.
- `--start-game` headless smoke (load, entry, one update, unload; exit codes 0/5/6) — `Documentation/testing.md` "Session startup".

---

## 8. Contract summaries

- **inventory_state_contracts.md** — Typed port of `CInventoryMgr::Save/Load`; aggregate checkpoint, not KeepAlive/retail format. Raw ammo after weapon recreation (no clamps/grants), selection last; KeepAlive (1) drops keys. Weapon transients (last shot, counters, held identities) restart; hand children recreated. Load needs live **unassociated** owner, stages Init/ObtainWeapon and swap-commits; failure leaves old inventory intact. Unported fields (inv god mode, active item, fire-rate multipliers, dropped eye) have no placeholder.
- **damage_state_contracts.md** — `CDestructable` values incl. 1e6 CanDamage sentinel (Load infers `hp < 1e6`), signed death health, **remaining** death delay (no clock offset), nigh-invulnerability (×0.05 for characters). Location byte initialized to 0. Load keeps destination owner/list binding, inserts intrusive link once, refuses associated player/active damage callback, validates SpawnObject first; clears attacker/raw packet memory.
- **player_state_contracts.md** — `SaveFields/LoadFields` cover listed CPlayerObj values; clocks stored as offsets from `GetTime()`; null vs empty strings preserved; Load assigns raw attributes (no ValidateAttributes/Respawn), zeroes stat caches. `INITIALUPDATE_SAVEGAME` binds aggregates/lists then returns early. Refuses active powers (Stealth dropped per powerup contracts). Remaining unsaved: power timers/sound stack, regen/slow-death/burning, gun/camera refs, swimming, scores, client saved data.
- **player_restore_contracts.md** — In-memory snapshot of the supported living local player in the same world. Ordered restore: no client → `Creation::Restore` → engine fields → inventory Restore/damage/fields → `RestoreModelState` (overrides flags/velocity) → relative scheduling → new client token, saved-client shell branch (no GoToStartPoint), suppressed fresh init; first update derives speeds **without** ValidateAttributes; ForcePos clears motion. Failure removes only the new player/children (not world rollback). No retail/SaveObjects claim; dead players, power timers, client data, whole world unavailable.
- **level_change_contracts.md** — NEW_LEVEL as replacement transaction: consume once at the app frame boundary; capture player; retain difficulty; suspend source globals (7 heads/counts, active camera, AI table owners, binding); build destination session (resources index copied only), full init, KeepAlive entry, strict presentation preflight; destroy failed candidate under its own adapter; output transfer last; destroy source under its own adapter; promote `unique_ptr`; bump revision; app clears selections/input. Rejection retains everything; audio IDs not rewound; source objectives/cameras/FX/world audio discarded on success.
- **player_keepalive_contracts.md** — `restore_player(KeepAlive, start)` into an already loaded (possibly different) world: keys dropped, raw stats kept, named start (case-insensitive, solo markers, last-solo fallback), trigger sent **before** teleport, final placement checked with restored body; failure discards staged player only, never undoes start-trigger effects.
- **destination_trial_contracts.md** — Rehearsal builds and destroys a temporary destination with original callbacks and KeepAlive entry, isolating exactly the listed globals (fixed sentinel addresses; restore saved head values to reconnect source chains); temporary objects/tables/voices/adapter destroyed before rebinding source. Every new mutable game global must join this audit. Process-wide audio ID allocator never rewinds. Not a transfer.
- **message_lifetime_contracts.md** — `HMESSAGEWRITE/READ` are 64-bit non-reused value identities from a process-wide atomic; adapter keeps only live payloads (≤65,536 simultaneous); callbacks borrow readers (can't release/write); 64 KiB packet bound; nested owned reads unavailable; not an ABI or serialization.
- **object_contracts.md (identity/lifetime)** — Creation: schema → record+handle → nested props → `MID_PRECREATE` → consume create struct → `MID_INITIALUPDATE` with reason 0/1/2/3 ("restore reason is tested; save-file restoration is not implemented"). Engine owns `unique_ptr<BaseClass>`; `SetNextUpdate` is a one-shot relative delay; step snapshots worklist, updates due objects once, then drains pending destruction; removal idempotent and deferred to end of frame; link notifications before reclamation; nested step/destroy/clear rejected. IDs = slot+generation; HOBJECT tokens never reused in a session; 65,536 token caps. `SaveObjects/RestoreObjects` return `DE_ERROR`. Single bound session per thread. (object_snapshot_contracts: index published before constructors, erased after destructors; snapshots are fresh copies.)
- **menu_contracts.md (UI-M0/UI-M5)** — Eight source save slots retained but every Save/Load row disabled; Load adds Restart/Quickload; screen says saving/loading unavailable; "No file snapshot or invented successful save is produced." Preferences file pattern (versioned, transactional parse, temp+rename). Menu ownership changes clear input latches without simulation.
- **demo_contracts.md (checkpoint hook)** — §5.2 above: Enter-after-death fresh restart is the designated integration point; replace with checkpoint selection + transactional restore, all prepared before promotion, source kept on failure, distinct fresh-world fallback; persistence/schema/versioning still required.
- **session_start_contracts.md** (context) — `run_world` = bounded startup check; every DAT row needs a disposition; unavailable services reject; failed fresh attempt removes only its new player (no world rollback); engine clock monotone across reloads within a session.

---

## 9. Gap list implied by the code (for the design)

1. Byte primitives for Save/Load (Byte/Word/Float/Vector/Rotation/Object/LoadSaveObject/nested messages) or a typed per-class state struct approach (the path the port already chose for 3 classes).
2. Object reference remapping (HOBJECT → saved index → new HOBJECT) incl. links, attachments, sky ObjectIds, client view/camera handles, AI targets/cursors, SoundId fields.
3. Restorable engine clock (or systematic rebasing) + `due`/`deactivate_at` restoration; float `GetTime` precision.
4. Restore-mode world load: geometry/collision/BSP binding from DAT **without** creating DAT objects, plus source-accounting (`entries`) semantics compatible with `check_world_initialization`.
5. Per-class `PRECREATE_SAVEGAME` acceptance (≈26 explicit refusals today) and Load bodies; SFX publishers must republish client FX after load.
6. Original globals: intrusive list order reproduction; AI tables; `g_ctf`; active camera.
7. Client save data (fade, music level, objectives, goggles/blind, game/load type) and HUD republish.
8. Audio policy: restart loops/ambient from object state (retail behavior) vs. cursor-accurate voices; world-owned transferred loops have no object.
9. RNG states (gameplay `random_`, per-object, FX) if determinism after load is desired.
10. File format/versioning/atomic write (reuse preferences pattern), slot metadata, menu actions, frame-boundary promotion via the NEW_LEVEL transaction pattern.
