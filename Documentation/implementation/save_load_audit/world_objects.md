# Blood II (1998) world-object Save/Load audit — original ObjectDLL

Source root: `lithtech/Blood2/ObjectDLL` (paths below are relative to it unless prefixed).
Characters/AI/inventory/player/CDestructable/movement are out of scope (audited separately); they appear here only where they sit in a world object's stream (aggregates).

Write-type legend: `B` WriteToMessageByte, `W` Word, `DW` DWord, `F` Float, `V` Vector, `R` Rotation, `HS` HString,
`OBJ` WriteToLoadSaveMessageObject / ReadFromLoadSaveMessageObject. `Rel` = written as `t - GetTime()`, read as `v + GetTime()`.
`Abs` = absolute GetTime()-based value written raw. "Order" in each table is the stream order inside that class's block.

---

## 0. Engine / shell context (applies to every class)

### 0.1 Who is saved, and how restore rebuilds the world
- Full save: `CBloodServerShell::SaveGame` (BloodServerShell.cpp:1004-1065) puts **only objects whose user flags contain `USRFLG_SAVEABLE` (bit 3, Shared/ClientServerShared.h:18)** (active + inactive lists) in the list, then `SaveObjects(path, list, dwParam=0, SAVEOBJECTS_SAVEGAMECONSOLE|SAVEOBJECTS_SAVEPORTALS)` (:1058).
- Full restore: `LoadWorld` sets `LOADWORLD_LOADWORLDOBJECTS` **only when not restoring** (BloodServerShell.cpp:942-943), then `RestoreGame` → `RestoreObjects(path, dwParam=0, RESTOREOBJECTS_RESTORETIME)` (:1090). So after a restore the world contains exactly: `CF_ALWAYSLOAD` classes re-read fresh from the .dat (AppHeaders/serverobj_de.h:60-61: "always loaded from the level file and can't be saved") **plus** the saved objects. Every other world-file object that never set `USRFLG_SAVEABLE` is gone. (Later-LithTech corroboration: runtime/server/src/s_object.cpp:394-397 skips non-ALWAYSLOAD classes; game_serialize.cpp:1124 skips ALWAYSLOAD objects at save.)
- KeepAlive (level change): `KeepAliveSave` saves **players only** with `dwParam=1, flags=0` (BloodServerShell.cpp:1139-1180); `KeepAliveLoad` restores with `dwParam=1, flags=0` (no time restore) (:1190-1206). No world object ever receives fData=1.
- `fData` of MID_SAVEOBJECT/MID_LOADOBJECT = that dwParam (serverobj_de.h:113-118). Every audited world class forwards it as `(DDWORD)fData` into `Save(hWrite, dwSaveFlags)`/`Load(hRead, dwLoadFlags)` and **none inspects it** (only forwarded to `KeyData::Save/Load`). No keepalive-vs-full branching exists in any world class.

### 0.2 CF_ALWAYSLOAD classes (never saved, re-created from world file on restore)
Game: CameraSpot.cpp:15, ExitHint.cpp:9, Key.cpp:23, LightFX.cpp:73/75, PolyGrid.cpp:39, ParticleSystem.cpp:32, SoccerObjects.cpp:51 (SoccerGoal), GameStartPoint.cpp:22, SparksObj.cpp:30, Script.cpp:41, SmellHint.cpp:11, WreckingBall.cpp:24, WorldProperties.cpp:65, pathpoint.cpp:20.
Engine built-ins (AppHeaders/engineobjects.c:176-855): Model, WorldModel, Container, Sprite, Sound, StartPoint, World, Brush, DemoSkyWorldModel, SkyPointer, FogSphere.
Consequences: runtime state of these is reset to world-file values; instances removed/destroyed during play reappear; instances created at runtime (VolumeBrush PolyGrid surfaces, player SmellHints, dynamic LightFX from ObjectUtilities.cpp:188) are neither saved nor reloaded; OBJ references/interlinks pointing at them restore as NULL.

### 0.3 Restore sequence and engine-owned state (later-LithTech runtime, runtime/server/src/game_serialize.cpp; consistent with Blood2 comments)
Per object the engine writes (sm_SaveObjectData :355-600): class name, object type, Flags/Flags2, position, scale, rotation, **user flags**, name, filename (world-model BSP name / file), skins, light radius, container code, **remaining NextUpdate**, blocking priority, **the object's persistent special-effect message (StartSpecialEffectMessage)**, model animation trackers, color/alpha, dims, mass, velocity, friction, acceleration, force-ignore limit, internal (active/inactive/sky) flags, client ref; then the game's MID_SAVEOBJECT blob; after all objects: attachments and **owner-side interlinks** (:946-1027).
Blood2 corroboration that the SFX message is engine-persisted: CameraObj.cpp:615-616, ClientParticleStreamSFX.cpp:268-269, ClientLightFX.cpp:637-638 ("all the data in the special fx message is saved/loaded for us").
Restore order (sm_RestoreObjects :1231-1348):
1. game time restored first when `RESTOREOBJECTS_RESTORETIME` (full restore only) → `GetTime()` inside PRECREATE/INITIALUPDATE/LOAD equals the saved clock.
2. For each object in save order: `MID_PRECREATE(PRECREATE_SAVEGAME)` with a create struct holding saved type/flags/pos/rot/scale/name/filename/skins/NextUpdate (m_UserData = 0) → object added → `MID_INITIALUPDATE(INITIALUPDATE_SAVEGAME)` → engine then **overwrites** user flags, blocking priority, SFX message, anims, color, dims, mass, velocity, friction, acceleration, force-ignore limit, and teleports to the saved position. Hence: Flags and NextUpdate changed in PRECREATE/INITIALUPDATE(SAVEGAME) **stick**; user flags/dims/velocity changed there are **overwritten**.
3. After *all* objects exist: `MID_LOADOBJECT` for each in the same order (OBJ reads resolve via serialize id; objects outside the save list → NULL, :190-216/:282-291).
4. Attachments + interlinks re-created (:1029-1089) — after every Load; games never need to call `CreateInterObjectLink` on load.
Instant SFX (`StartInstantSpecialEffectMessage`) are never persisted.

### 0.4 Dispatch shape common to all world classes
- Derived `EngineMessageFn` handles MID_SAVEOBJECT/MID_LOADOBJECT then calls its base → **stream is derived-first** (e.g. ToggleTrigger block, then Trigger block).
- `B2BaseClass` (B2BaseClass.cpp:52-79) has no Save/Load; ReadProp only for WORLDFILE/STRINGPROP, difficulty/gametype filter only for INITIALUPDATE_WORLDFILE; `m_dwOptFlags` not saved (init-only, safe). `CClientSFX` (ClientSFX.cpp:38-48) only ORs `FLAG_FORCECLIENTUPDATE` in PRECREATE (all types, incl. SAVEGAME).
- Aggregates run from `bc_EngineMessageFn` (AppHeaders/engineobjects.c:13-126) **after** the whole class chain, in reverse `AddAggregate` order (head insertion, :6-10). So aggregate blocks come last, newest-added first.

---

## 1. CameraObj (CameraObj.cpp / .h)
Dispatch EngineMessageFn :95-165. PRECREATE :120-130: base first; ReadProp only WORLDFILE/STRINGPROP; `PostPropRead` (:205-232) **always**, incl. SAVEGAME → overwrites `m_Flags = FORCECLIENTUPDATE|FULLPOSITIONRES|GOTHRUWORLD` (+OT_MODEL/VISIBLE if `DebugCutScene`) and `m_NextUpdate = 0.001`. INITIALUPDATE :133-145: `InitialUpdate` (:288-335: SetActive(start), SFX_CAMERA_ID message, SAVEABLE :332) skipped on SAVEGAME; insertion into static camera list always. SAVE :148 / LOAD :154 before base.

| Order | Field | Type | OBJ | Notes |
|---|---|---|---|---|
| 1 | m_bActive | B | | |
| 2 | m_bStartActive | B | | |
| 3 | m_bIsListener | B | | |
| 4 | m_bPlayerMovement | B | | |
| 5 | m_nType | B | | |
| 6 | m_fActiveTime | F | | duration |
| 7 | m_fDeactivateTime | F Rel | | see bug |
| 8 | m_bHidePlayer | B | | |
| 9 | m_hLinkObject | OBJ | yes | interlink via SetLinkObject (:521-545) → engine restores; MID_LINKBROKEN clears (:110-118) |
Save :610-631, Load :640-658. Load tail calls `SetActive(m_bActive)` (:657): re-applies USRFLG_VISIBLE, NextUpdate (0.01/0), recomputes `g_hActiveCamera`; **bug**: if active and `m_fActiveTime>0` it sets `m_fDeactivateTime = now + m_fActiveTime`, discarding the loaded remaining time (timed cameras restart full duration).
Not saved: `m_hRay` (debug line), `m_Link` (re-inserted in INITIALUPDATE). SFX message engine-persisted.

## 2. CinematicTrigger (CinematicTrigger.cpp / .h, MAX_CT_MESSAGES=10)
Dispatch :266-318. PRECREATE ReadProp WORLDFILE/STRINGPROP only. INITIALUPDATE: non-SAVEGAME sets SAVEABLE (:295) + NextUpdate 0; SAVEGAME does nothing (engine NextUpdate kept). SAVE :304 / LOAD :308 before base.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_nCurMessage | B | |
| 2..51 | for i=0..9: m_fDelay[i] F, m_hstrSound[i] HS, m_hstrWhoPlaysSound[i] HS, m_hstrTargetName[i] HS, m_hstrMessageName[i] HS | | |
Save :463-480, Load :489-505. Symmetric.
Not saved: `m_hCurSound` (HSOUNDDE). Effect: if saved while a cue's sound plays, the restored NextUpdate (0.001) runs `Update` (:209-253) with no handle → `StartSound` (:373-418) for the same index → **re-sends that cue's trigger message** (SendMessage inside StartSound) and replays the sound from 0. If saved during an inter-cue delay, resumes correctly via the engine NextUpdate. Speaker object (`PlayedBy`) re-resolved by name each cue.

## 3. Trigger family (Trigger.cpp / .h, MAX_TRIGGER_MESSAGES=10)
### 3.1 Trigger
Dispatch :595-645. PRECREATE ReadProp WORLDFILE/STRINGPROP. INITIALUPDATE: `InitialUpdate` (:501-518: dims, FLAG_TOUCH_NOTIFY, SAVEABLE :513, NextUpdate 0) skipped on SAVEGAME. SAVE :632 / LOAD :636 before base.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_bActive | B | |
| 2 | m_vDims | V | |
| 3 | m_fTriggerResetTime | F | |
| 4 | m_hstrActivationSound | HS | |
| 5 | m_fSoundRadius | F | |
| 6..45 | for i=0..9: m_hstrTargetName[i] HS, m_hstrMessageName[i] HS, m_fTriggerMessageDelay[i] F, m_bDelay[i] B | | pending delayed sends |
| 46 | m_fMaxTriggerMessageDelay | F | |
| 47 | m_fDelayStartTime | F Rel | |
| 48 | m_bTouchActivate | B | |
| 49 | m_bPlayerActivate | B | |
| 50 | m_bAIActivate | B | |
| 51 | m_bObjectActivate | B | |
| 52 | m_bTriggerRelayActivate | B | |
| 53 | m_bNamedObjectActivate | B | |
| 54 | m_bLocked | B | |
| 55 | m_nActivationCount | DW | |
| 56 | m_nCurrentActivation | DW | |
| 57 | m_hstrLockedMsg | HS | |
| 58 | m_hstrLockedSound | HS | |
| 59 | m_hstrUnlockedMsg | HS | |
| 60 | m_hstrUnlockedSound | HS | |
| 61 | m_hstrKeyName | HS | |
| 62 | m_hstrActivationObjectName | HS | |
Save :1046-1090, Load :1099-1141. Symmetric. Delay/reset progression continues from engine NextUpdate.
Not saved: `m_hLastSender` (HOBJECT, written but never read — only a commented use in ConversationTrigger.cpp:222), `m_bSending` (reentrancy guard; false at rest). Both safe.

### 3.2 BigTrigger — no fields, no handler (Trigger.cpp:1145-1146) → pure Trigger stream.
### 3.3 ToggleTrigger — dispatch :1244-1270 (Save/Load then Trigger::EngineMessageFn). Stream: ToggleTrigger block, then Trigger block.
| Order | Field | Type |
|---|---|---|
| 1..10 | m_hstrOffMessageName[i] | HS |
| 11 | m_bOn | B |
Save :1305-1316, Load :1327-1337.

### 3.4 ExitTrigger (ExitTrigger.cpp:111-136, Save :213-221, Load :232-240) — then Trigger block.
| 1 m_hstrNextWorld HS | 2 m_hstrStartPointName HS | 3 m_nExitType B |

### 3.5 ConversationTrigger (ConversationTrigger.cpp:127-155, Save :315-330, Load :341-356; CT_MAXMESSAGES=8) — then Trigger block.
| 1..8 m_hstrSoundFile[i] HS | 9 m_nCharacterType B | 10 m_nNumMessages B | 11 m_nCurrentMessage B | 12 m_bRepeatLastMessage B | 13 m_bLoopMessages B |
All members saved.

### 3.6 ObjectivesTrigger (ObjectivesTrigger.cpp:113-140, Save :220-228, Load :239-247) — then Trigger block.
| 1 m_nResource DW (long) | 2 m_hstrTitle HS | 3 m_hstrText HS |
**Original bug**: `m_hstrSound` (read at :176-180, played at :206-207) is not saved → objective sound silent after restore.

## 4. Door (Door.cpp / .h)
Dispatch :809-858. PRECREATE: base first; ReadProp **WORLDFILE only**; `PostPropRead` (:354-368) always (SAVEGAME too): `m_Filename = m_Name` (BSP bind by name), skin cleared, ORs GOTHRUWORLD|FULLPOSITIONRES|MODELGOURAUDSHADE|DONTFOLLOWSTANDING (+BOXPHYSICS only if `m_bBoxPhysics`, which is still the ctor default at SAVEGAME — saved flags already contain it), deactivation time; defaults for m_fClosingSpeed/m_fOpenDelay (overwritten by Load). INITIALUPDATE: `InitialUpdate(fData)` returns at once for SAVEGAME (:425-426: no SpawnTrigger, no open/closed recompute, no blocking priority); `CacheFiles` always. SAVE :842 / LOAD :848 before base.

| Order | Field | Type | OBJ | Notes |
|---|---|---|---|---|
| 1 | m_hTriggerObj | OBJ | yes | self-spawned Trigger (SpawnTrigger :485-~540); no interlink ever created |
| 2 | m_dwTriggerFlags | DW | | |
| 3 | m_fSpeed | F | | |
| 4 | m_fMoveDist | F | | |
| 5 | m_vMoveDir | V | | |
| 6 | m_hstrOpenBusySound | HS | | |
| 7 | m_hstrOpenStopSound | HS | | |
| 8 | m_hstrCloseBusySound | HS | | |
| 9 | m_hstrCloseStopSound | HS | | |
| 10 | m_fWaitTime | F | | |
| 11 | m_vTriggerDims | V | | |
| 12 | m_fClosingSpeed | F | | |
| 13 | m_bLocked | B | | |
| 14 | m_bBoxPhysics | B | | |
| 15 | m_vOpenPos | V | | |
| 16 | m_vClosedPos | V | | |
| 17 | m_dwDoorState | DW | | OPEN/OPENING/CLOSED/CLOSING |
| 18 | m_fDoorOpenTime | F | | countdown decremented by frame time (Open() :608-623) → inherently relative |
| 19 | m_fSoundRadius | F | | |
| 20 | m_hstrPortal | HS | | |
| 21 | m_dwFlags | DW | | |
| 22 | m_dwWaveform | DW | | |
| 23 | m_bStartedMoving | B | | |
| 24 | m_fOpenDelay | F | | |
| 25 | m_bFireThrough | B | | |
Save :1110-1144, Load :1155-1189. Symmetric.
Not saved: `m_sndLastSound` → **busy loop not restarted**: a door saved mid-OPENING (m_bStartedMoving=TRUE) or mid-CLOSING (SetClosing starts the loop only once) moves silently after load; `m_bFirstUpdate`, `m_bDoorBlocked` (per-frame); trigger-option fields `m_bTrig*` (7 bools), `m_hstrTrigActivationObjectName`, `m_hstrTrigLockedMsg/LockedSound/UnlockedSound/KeyName` — consumed only by SpawnTrigger at init; the spawned Trigger stores its own copies (Trigger::Setup CopyString) → safe. Position: engine. Portal open state: engine (SAVEOBJECTS_SAVEPORTALS); Door does not re-call SetPortalState on load. State machine resumes from engine NextUpdate.

## 5. RotatingDoor (RotatingDoor.cpp / .h) : Door
Dispatch :61-104. PRECREATE: `Door::EngineMessageFn` first, ReadProp WORLDFILE/STRINGPROP, then **unconditionally** converts m_fSpeed/m_fClosingSpeed degrees→radians (on SAVEGAME this converts ctor values; Door::Load later overwrites with the saved radians → harmless). INITIALUPDATE: `InitialUpdate` (:157-237: angles, original pos, open dirs, SetForceIgnoreLimit) skipped on SAVEGAME (force limit is engine-saved). SAVE/LOAD before Door → stream: RotatingDoor block, then Door block.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_vRotationAngles | V | |
| 2 | m_vRotationPoint | V | |
| 3 | m_vOpenAngles | V | |
| 4 | m_vClosedAngles | V | |
| 5 | m_vOriginalPos | V | |
| 6 | m_nOpenDirX | DW | short, read back via `(short)` |
| 7 | m_nOpenDirY | DW | short |
| 8 | m_nOpenDirZ | DW | short |
| 9 | m_fPitch | F | **current** angle (CalcAngle mutates it) |
| 10 | m_fYaw | F | current |
| 11 | m_fRoll | F | current |
| 12 | m_bPushPlayerBack | B | |
Save :473-493, Load :504-524. All members saved.

## 6. Rotating aggregate (Rotating.cpp / .h)
Aggregate EngineMessageFn :96-140 (runs after owner chain). PRECREATE ReadProp WORLDFILE only. INITIALUPDATE `InitialUpdate` (:258-269: SetNormalRotation if not OFF, copy save-velocity) skipped on SAVEGAME.

| Order | Field | Type | OBJ | Notes |
|---|---|---|---|---|
| 1 | m_hObject | OBJ | yes | owner self-handle |
| 2 | m_vVelocity | V | | |
| 3 | m_vSaveVelocity | V | | |
| 4 | m_vSign | V | | |
| 5 | m_vSpinUpTime | V | | |
| 6 | m_vSpinDownTime | V | | |
| 7 | m_vSpinTimeLeft | V | | countdown (frame-time) |
| 8 | m_fLastTime | F Rel | | |
| 9 | m_fStartTime | F Rel | | |
| 10 | m_fPitch | F | | current angle |
| 11 | m_fYaw | F | | |
| 12 | m_fRoll | F | | |
| 13 | m_eState | DW | | RWM_OFF/NORMAL/SPINUP/SPINDOWN |
| 14 | m_hstrBusySound | HS | | |
| 15 | m_hstrSpinUpSound | HS | | |
| 16 | m_hstrSpinDownSound | HS | | |
| 17 | m_fSoundRadius | F | | |
Save :685-712, Load :723-750. Symmetric.
Not saved: `m_sndLastSound` → NORMAL-state **busy loop not restarted** (silent rotation after load); spin one-shots not replayed.
Owners (aggregate added in ctor; INITIALUPDATE `m_Rotating.Init(m_hObject)` unconditional for the first three):
- RotatingBrush (RotatingBrush.cpp:34, :60-77) stream: CDestructableBrush block → Rotating → CDebris → CDestructable.
- RotatingModel (RotatingModel.cpp:33, :58-75) stream: CDestructableModel block → Rotating → CDebris → CDestructable.
- RotatingSprite (RotatingSprite.cpp:32, :44-58) stream: DetailSprite block → Rotating.
- WorldModelDebris (WorldModelDebris.cpp:37; Init only non-SAVEGAME) — see §10.
RotatingBrush.h:26-27 and RotatingModel.h:26-27 declare Save/Load that are never defined nor dispatched (dead).

## 7. KeyFramer + KeyData (KeyFramer.cpp/.h, KeyData.cpp/.h)
Dispatch :381-449. PRECREATE ReadProp WORLDFILE only (sets m_NextUpdate 0.001). INITIALUPDATE `InitialUpdate` (:529-544: SAVEABLE :536, NextUpdate 0.001) skipped on SAVEGAME. MID_LINKBROKEN (:409-427) removes the object from `m_pObjectList` (offset arrays not compacted → later objects use wrong offsets; pre-existing bug). SAVE :429 / LOAD :435 before base.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_fCurTime | F | elapsed along timeline (relative by nature) |
| 2 | m_hstrObjectName | HS | |
| 3 | m_hstrBaseKeyName | HS | |
| 4 | m_bStartActive | B | |
| 5 | m_bLooping | B | |
| 6 | m_bActive | B | |
| 7 | m_bFirstUpdate | B | |
| 8 | m_nNumKeys | B | int truncated to 8 bits |
| 9.. | m_nNumKeys × KeyData block | | Save loops with full int, Load with the byte → **stream desync if >255 keys** |
| .. | nCurKeyIndex | F | pointer→index, float-encoded, -1 = none |
| .. | nPosition1Index | F | |
| .. | nPosition2Index | F | |
| .. | for i < m_nNumKeys: m_pOffsets[i] V, m_pRotations[i] R | | **bug**: arrays are indexed by *controlled object*, loop bound is *key count* → offsets for objects ≥ m_nNumKeys lost (symmetric, no desync) |
Save :714-770, Load :780-845.
KeyData block (KeyData.cpp Save :121-139, Load :148-165): 1 m_rRot R, 2 m_vPos V, 3 m_nKeyType DW, 4 m_fTimeStamp F, 5 m_fRealTime F, 6 m_fSoundRadius F, 7 m_bLoopSound B, 8 m_hstrSoundName HS, 9 m_hstrMessageTarget HS, 10 m_hstrMessageName HS, 11 m_hstrBPrintMessage HS.
Not saved:
- `m_pObjectList` → rebuilt lazily in `Update` (:559-564) by `BuildObjectList` (:274-298) = FindNamedObjects(m_hstrObjectName) + `CreateInterObjectLink` per object, **on top of** the engine-restored interlinks (duplicates); list order may differ from the original → offsets mis-assigned when several objects share the name.
- `m_hsndKeySound` → looping key sound (ProcessKey :349-358) not restarted.
- Key objects: `Key` is CF_ALWAYSLOAD → they reappear in a restored world; KeyFramer (m_bFirstUpdate=FALSE) never consumes them again (inert). Save before first update → CreateKeyList runs on reloaded keys (OK).
- Controlled objects: their transforms are engine-saved only if they are themselves saveable; plain WorldModel/Model targets are ALWAYSLOAD → snap back to world-file pose; an *active* KeyFramer re-places them at the next interpolation step, an *inactive* (finished) one leaves them at the world-file pose (data loss).
Timeline resumes via engine NextUpdate (0.001 when active).

## 8. PathListData / PathList (PathListData.cpp, PathList.h)
PathListData::Save :85-95 / Load :105-115: 1 m_vPos V, 2 m_hstrName HS, 3 m_hstrActionTarget HS, 4 m_hstrActionMessage HS.
PathList::Save/Load (PathList.h:167-200): count as F (float-encoded int), m_bDeleteElements B, then each element — **NULL elements are skipped on save but counted** → desync if any slot is NULL.
Only caller: AI_Mgr (m_AIPathList, AI_Mgr.cpp:6842/6937). `PathMgr::Save/Load` (PathMgr.h:29-37) never called; PathMgr is rebuilt in `PostStartWorld` (BloodServerShell.cpp:581-585) from CF_ALWAYSLOAD PathPoints → safe.

## 9. CDestructableBrush (DestructableBrush.cpp / .h) — aggregates m_damage (CDestructable), m_Debris (CDebris) added in that order (:49-50)
Dispatch :170-255. PRECREATE: base first; ReadProp WORLDFILE/STRINGPROP; `PostPropRead` always (`m_Filename=m_Name`, flags OR with ctor m_bBoxPhysics/m_bPushable → no-op on SAVEGAME). INITIALUPDATE: everything only non-SAVEGAME (:208-240: damage Init/HP/mass, debris Init, NextUpdate 0, damage physics, blocking priority, USRFLG_MOVEABLE, SAVEABLE :236 + surface type `<<24`) → on SAVEGAME aggregates rely on their own Load for `m_hObject`. SAVE :243 / LOAD :247 before base.

| Order | Field | Type |
|---|---|---|
| 1 | m_fInitHitPoints | F |
| 2 | m_fMass | F |
| 3 | m_bDestructable | B |
| 4 | m_bPushable | B |
| 5 | m_bBoxPhysics | B |
| 6 | m_bAllowMarks | B |
| 7 | m_hstrDebrisSkin | HS |
| 8 | m_eSurfType | DW |
| 9 | m_bFireThrough | B |
Save :385-400, Load :410-424. All members saved. Stream: [subclass] → this block → CDebris block → CDestructable block (separate audit).
Destruction: `CreateWorldModelDebris` (:297-~350) starts pre-placed named `WorldModelDebris` objects; debris gibs are instant SFX (not persisted).

### 9.1 CDebris aggregate (Debris.cpp / .h)
EngineMessageFn :105-145: PRECREATE ReadProp WORLDFILE/STRINGPROP; INITIALUPDATE type derivation skipped on SAVEGAME.
| Order | Field | Type | OBJ |
|---|---|---|---|
| 1 | m_hObject | OBJ | yes (self) |
| 2 | m_hstrSound | HS | |
| 3 | m_hstrTexture1 | HS | |
| 4 | m_hstrTexture2 | HS | |
| 5 | m_hstrModel1 | HS | |
| 6 | m_hstrModel2 | HS | |
| 7 | m_nAmount | B | |
| 8 | m_fScale | F | |
| 9 | m_bExploding | B | |
| 10..19 | m_bStone, m_bMetal, m_bWood, m_bEnergy, m_bGlass, m_bTerrain, m_bPlastic, m_bFlesh, m_bLiquid, m_bCustom | B each | |
| 20 | m_eType | DW | |
| 21 | m_fDamageRadius | F | |
| 22 | m_fExplodeDamage | F | |
Save :462-490, Load :501-532. Symmetric; all members saved.

## 10. WorldModelDebris (WorldModelDebris.cpp / .h) : CDestructableBrush, + Rotating aggregate (:37)
Dispatch :94-137. UPDATE handled here then also by CDestructableBrush. PRECREATE: CDestructableBrush first, ReadProp WORLDFILE/STRINGPROP. INITIALUPDATE non-SAVEGAME: `m_Rotating.Init` + `SetOff`. SAVE/LOAD before base.
| 1 m_vInitialVelocity V | 2 m_bOn B | 3 m_fGravityMultiplier F |
Save :207-216, Load :226-235. Stream: this → CDestructableBrush block → Rotating → CDebris → CDestructable. Acceleration re-applied every update while m_bOn; velocity/acceleration engine-saved.

### 10.1 NagaCeilingDebris (NagaCeilingDebris.cpp:9, .h:7) — registered with parent CDestructableBrush but C++ parent is B2BaseClass; no Save/Load; never SAVEABLE → dropped on restore.

## 11. CDestructableModel (DestructableModel.cpp / .h) — aggregates m_damage, m_Debris (:65-66)
Dispatch :296-367. PRECREATE: B2BaseClass first; ReadProp WORLDFILE/STRINGPROP; ORs ENVIRONMENTMAP (m_bChrome), TOUCH|GRAVITY (m_bPushable) — ctor defaults on SAVEGAME → no-op. INITIALUPDATE: B2BaseClass first, then `InitialUpdate(fData)` (:379-482) which does everything only if `!= SAVEGAME` (color/alpha, damage+debris init, blocking/MOVEABLE, ScaleObject, dims, shadow trim, SAVEABLE|SINGULARITY_ATTRACT|surf<<24 :450-459, deactivation, sliding-sound string, death delay, persistent SFX_DESTRUCTABLEMODEL); MoveObjectToGround only WORLDFILE. SAVE :353 / LOAD :359 before base.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_hstrDamagedFilename | HS | |
| 2 | m_hstrDamagedSkinName | HS | |
| 3 | m_hstrDestroyFilename | HS | |
| 4 | m_hstrDestroySkinName | HS | |
| 5 | m_InitDims | V | |
| 6 | m_DamageDims | V | |
| 7 | m_DestroyDims | V | |
| 8 | m_dwDestroyFlags | DW | |
| 9 | m_bDeadState | B | |
| 10 | m_fInitHitPoints | F | |
| 11 | m_fDamagedHitPoints | F | |
| 12 | m_fMass | F | |
| 13 | m_fAlpha | F | |
| 14 | m_fYaw | F | |
| 15 | m_bDestroyVisible | B | |
| 16 | m_bDestroySolid | B | |
| 17 | m_bDestroyGravity | B | |
| 18 | m_bDestructable | B | |
| 19 | m_bPushable | B | |
| 20 | m_dwSurfType | DW | |
| 21 | m_bChrome | B | |
| 22 | m_fScale | F | |
| 23 | m_vTintColor | V | |
| 24 | m_fTargetAlpha | F | |
| 25 | m_fAlphaFadeRate | F | |
| 26 | m_fLastTime | F **Abs** | valid only because full restore restores game time |
| 27 | m_hstrSlidingSound | HS | |
Save :969-1006, Load :1016-1050. Symmetric. Stream: this → CDebris → CDestructable.
Not saved (transient): `m_bSliding`, `m_hSlidingSound` (sliding loop lost), `m_vLastPos`, `m_nSlidingFrameCounter`, `m_bStandingOn`, `m_nStandingOnFrameCounter`. Damaged/destroyed model+skin swap, scale, color, dims: engine-saved.

## 12. DetailSprite (DetailSprite.cpp / .h); RotatingSprite adds Rotating aggregate
Dispatch :85-148. PRECREATE ReadProp + struct edits only WORLDFILE/STRINGPROP. INITIALUPDATE (:196-~275: SAVEABLE :205, flush alignment, repeat creation) skipped on SAVEGAME.
| 1 m_vScale V | 2 m_hstrFilename HS | 3 m_bRotatable B | 4 m_bChromaKey B | 5 m_bAlignedToWorld B | 6 m_bFlushWithWorld B |
Save :322-334, Load :344-356.
Not saved: `m_bRepeat`, `m_dwRepeatX`, `m_dwRepeatY`, `m_fSpacingX`, `m_fSpacingY` — init-only; repeat copies are independent SAVEABLE DetailSprites (CreateAnotherSprite :288-311, their m_hstrFilename is NULL but the engine filename restores the sprite) → safe.

## 13. CClientParticleStreamSFX (ClientParticleStreamSFX.cpp / .h)
Dispatch :103-170. PRECREATE ReadProp WORLDFILE only. INITIALUPDATE non-SAVEGAME: clear GRAVITY, FORCECLIENTUPDATE, SAVEABLE (:130, unconditional of bOn), VISIBLE per bOn, `SetupFX` (persistent SFX message); CacheFiles always.
| 1 bOn B |
Save :261-271, Load :281-288.
Not saved (all live in the engine-persisted SFX message or are init-only): fRadius, fPosRadius, fMinVel, fMaxVel, nNumParticles, fSpread, vColor1/2, fAlpha, fMinLife, fMaxLife, fRampTime, fDelay, fGravity, bRampFlags, hstrTexture, fSoundRadius, hstrSound1/2/3 → safe (HSTRINGs NULL after load; CacheFiles becomes a no-op). Visibility = USRFLG_VISIBLE (engine) + bOn.

## 14. CClientLightningSFX (ClientLightningSFX.cpp / .h)
Dispatch :110-153. PRECREATE: **ReadProp unconditionally** (also SAVEGAME/NORMAL: GetProp calls fail → uninitialised `tempLong` copied into cl.nShape/nForm/nType, `hstrSound = CreateString(uninitialised buffer)` → leaked garbage HSTRING overwritten by Load). INITIALUPDATE unconditional: flags, SAVEABLE (:129), NextUpdate 0.1 (overrides saved; update is a no-op keepalive).
| 1 cl.vSource V | 2 cl.vDest V | 3 cl.nShape B | 4 cl.nForm B | 5 cl.nType B | 6 fDamage F | 7 fDamageRadius F | 8 hstrSound HS | 9 fSoundRadius F | 10 bTriggered B |
Save :203-222, Load :232-251. All members saved. Bolts are instant SFX on FIRE.

## 15. CClientLaserBeamSFX (ClientLaserBeamSFX.cpp / .h)
Same pattern as §14 (PRECREATE ReadProp unconditional with the same garbage/leak quirk; INITIALUPDATE unconditional, SAVEABLE :121, NextUpdate 0.1).
| 1 clb.vSource V | 2 clb.vDest V | 3 clb.nType B | 4 fDamage F | 5 fDamageRadius F | 6 hstrSound HS | 7 fSoundRadius F |
Save :196-210, Load :220-234. All members saved.

## 16. CClientExplosionSFX (ClientExplosionSFX.cpp / .h)
Dispatch :321-360. PRECREATE ReadProp unconditional (m_nType from uninitialised temp until Load; WaveFile guarded). INITIALUPDATE unconditional: NextUpdate 0.1, `m_vPos = current pos` (overwritten by Load), SAVEABLE (:338). UPDATE removes itself once m_bTriggered.
| 1 m_vPos V | 2 m_vNormal V | 3 m_fOffset F | 4 m_fDamage F | 5 m_fDamageRadius F | 6 m_nType DW | 7 m_hstrSound HS | 8 m_bTriggered B |
Save :414-426, Load :438-450. All members saved. Explosion visuals are instant SFX.

## 17. ClientLightFX (+ SquareWave/Saw/RampUp/RampDown/Sine/Flicker1-4/Strobe/Search WaveLightFX, FlickerLight, GlowingLight — ClientLightFX.cpp:49-140, no own code) — aggregate m_damage (:155)
Dispatch :223-280; note it always returns `B2BaseClass::EngineMessageFn` (skips CClientSFX). PRECREATE: B2BaseClass first; ReadProp WORLDFILE/STRINGPROP (m_bDynamic=FALSE); `PostPropRead` (:474-~530) **always**: on SAVEGAME (ctor m_bOn=TRUE) **overwrites** `pStruct->m_Flags = FLAG_VISIBLE` (:483) `|GOTHRUWORLD` (saved flags discarded) and `m_NextUpdate=0.01`, resets all SFX params to defaults. INITIALUPDATE: `InitialUpdate` (damage init, randomized NextUpdate, `Init`, SendEffectMessage) only non-SAVEGAME; CacheFiles always.
| 1 m_bOn B | 2 m_bDynamic B | 3 m_fLifeTime F | 4 m_fStartTime F **Abs** |
Save :632-641, Load :655-663. Then CDestructable aggregate block.
**Original data-loss bug**: USRFLG_SAVEABLE is set only in `Init()` when `m_bOn` (:551-556) → lights with LightSwitch=FALSE at level start are never saved and vanish on restore even if later switched on.
Not saved: m_vColor, intensity/radius min/max/phase/freq/waveforms, ramp sounds, m_dwLightFlags (all inside the engine-persisted SFX_LIGHT message), m_fHitPts (init-only) → safe.

## 18. CClientWarpGateSFX (ClientWarpGateSFX.cpp / .h)
Dispatch :281-334. PRECREATE ReadProp WORLDFILE only. INITIALUPDATE **unconditional**: NextUpdate 1.0 (overrides saved), flags, VISIBLE per ctor bOn (user flags then overwritten by the engine), SAVEABLE (:306), `bFirstUpdate = 1` (:308; later overwritten by Load). UPDATE: if bFirstUpdate → SetupFX; syncs VISIBLE from bOn; NextUpdate 0.1.

| Order | Field | Type |
|---|---|---|
| 1 | fRampUpTime | F |
| 2 | fRampDownTime | F |
| 3 | bInitiallyOn | B |
| 4-10 | wSpr: fMinScale F, fMaxScale F, fAlpha F, nRampUpType DW, nRampDownType DW, bAlign B, hstrSprite HS | |
| 11-29 | wPS1: fSystemRadius F, fPosRadius F, vOffset V, vRotations V, fMinVelocity F, fMaxVelocity F, nNumParticles DW, nEmitType DW, vMinColor V, vMaxColor V, fAlpha F, fMinLifetime F, fMaxLifetime F, fAddDelay F, fGravity F, nRampUpType DW, nRampDownType DW, bAlign B, hstrParticle HS | |
| 30-48 | wPS2: same 19 fields | |
| 49 | fSoundRadius | F |
| 50 | hstrSound1 | HS |
| 51 | hstrSound2 | HS |
| 52 | hstrSound3 | HS |
| 53 | bOn | B |
| 54 | bFirstUpdate | B |
Save :468-535, Load :545-612. All members saved.
**Original bug**: `SetupFX` (:169-~250) uses three **instant** SFX messages (:179/:198/:229) that reference m_hObject; they are not persisted, and bFirstUpdate is restored as 0 → client warp sprite/particle effects are never recreated after a restore. (HandleTrigger also writes back the unmodified dwUsrFlags; visibility only syncs in Update.)

## 19. Rain (Rain.cpp / .h)
Dispatch :108-139. PRECREATE ReadProp WORLDFILE only. INITIALUPDATE `InitialUpdate` (:208-224: SAVEABLE :218, SendEffectMessage if !m_bTriggered) skipped on SAVEGAME. MID_PARENTATTACHMENTREMOVED → remove (attachment engine-restored).
| 1 m_dwFlags DW | 2 m_fDensity F | 3 m_fLifetime F | 4 m_bGravity B | 5 m_fParticleScale F | 6 m_fSpread F | 7 m_bTriggered B | 8 m_vDims V | 9 m_vDirection V | 10 m_vColor1 V | 11 m_vColor2 V | 12 m_fTimeLimit F | 13 m_fPulse F |
Save :293-309, Load :322-344. Load tail **re-sends** the persistent SFX_RAIN message (:343) if !m_bTriggered (idempotent with the engine copy). Triggered-and-started rain relies on the engine-persisted message (m_bTriggered is never cleared on ON).

## 20. Spawner (Spawner.cpp / .h)
Dispatch :84-127. PRECREATE ReadProp only fData==1.0 (WORLDFILE). INITIALUPDATE **unconditional** (:97-110): `NextUpdate = m_bStartActive ? m_fRespawnRate : 0` evaluated with **ctor defaults** on SAVEGAME (m_bStartActive=FALSE) → **original bug**: overrides the engine-saved NextUpdate, so a spawner running at save time (StartActive or "START") stops after restore; SAVEABLE (:108).
| 1 m_hstrSound HS | 2 m_hstrSpawnObject HS | 3 m_vMinVelocity V | 4 m_vMaxVelocity V | 5 m_fSoundRadius F | 6 m_fRespawnRate F | 7 m_bUseTriggerObjPos B | 8 m_bStartActive B | 9 m_bCreateRiftEffect B | 10 m_dwRespawnCount DW |
Save :429-447, Load :458-476. All members saved. Spawned objects are independent (saved if their own class is SAVEABLE); no handle kept.

## 21. RandomSoundFX (RandomSoundFX.cpp / .h, RS_NUMSOUNDS=10)
Dispatch :113-163; PRECREATE ReadProp WORLDFILE/STRINGPROP; INITIALUPDATE skip SAVEGAME; Save/Load before base.
| 1 m_bOn B | 2 m_bPositional B | 3 m_bSequentialPlay B | 4 m_bLoopSequence B | 5 m_fPosRangeRadius F | 6 m_fSoundRadius F | 7 m_fMinWaitTime F | 8 m_fMaxWaitTime F | 9 m_nVolume B | 10 m_nNumSounds B | 11 m_nCurrentSound B | 12-21 m_hstrSoundFile[i] HS |
Save :452-481, Load :490-520. Not saved: m_hsndSound.
**Original data-loss bug**: the class never sets USRFLG_SAVEABLE (InitialUpdate :310-320 only schedules) → never in the save list; Save/Load is dead code; every RandomSoundFX disappears on restore.

## 22. SoundFX (SoundFX.cpp / .h)
Dispatch :104-163. PRECREATE: base first; ReadProp WORLDFILE/STRINGPROP; `PostPropRead` (:307-314) always (NextUpdate from ctor m_bStartOn on SAVEGAME). INITIALUPDATE `InitialUpdate` (:325-345: SAVEABLE :332, start if m_bStartOn, NextUpdate 0) skipped on SAVEGAME; CacheFiles always.
| 1 m_byState B | 2 m_hstrRampUpSound HS | 3 m_hstrRampDownSound HS | 4 m_hstrSound HS | 5 m_fOuterRadius F | 6 m_fInnerRadius F | 7 m_nVolume B | 8 m_bAmbient B | 9 m_bFileStream B | 10 m_nPriority B |
Save :511-525, Load :538-561. Not saved: m_bStartOn (init-only), m_hsndSound.
Load tail: `m_hsndSound=NULL; UpdateSound();` (:558, restarts the current state's sound at offset 0; ON+ambient loops) then `SetNextUpdate(0)` (:560) — **original bug**: clobbers the duration-based NextUpdate UpdateSound (:391-~500) just set, so RAMPUP never reaches ON (loop never starts), RAMPDOWN never reaches OFF, non-ambient ON never ramps down.

## 23. TripLaser (TripLaser.cpp / .h)
Dispatch :131-181. PRECREATE: base first, ReadProp WORLDFILE/STRINGPROP, always forces m_Filename/m_SkinName to `Models\default.abc`/`Skins\ammo\beamred.dtx`. INITIALUPDATE `InitialUpdate` (:292-362: SAVEABLE :299, scale, color, endpoints, dims, flags, sound) skipped on SAVEGAME.
| 1 m_vDims V | 2 m_vColor V | 3 m_fAlpha F | 4 m_fDamage F | 5 m_fDamageTime F Rel | 6 m_hstrTriggerTarget HS | 7 m_hstrTriggerMessage HS | 8 m_hstrSound HS | 9 m_hstrTouchSound HS | 10 m_vEndPoints[0] V | 11 m_vEndPoints[1] V | 12 m_dwFlags DW | 13 m_bTriggered B | 14 m_bRemove B |
Save :488-511, Load :523-563. Not saved: m_hSound — **restarted in Load** (looping attached sound, :551-562) when m_bTriggered. Flicker-off/kill sequence resumes via engine NextUpdate.

## 24. VolumeBrush (VolumeBrush.cpp / .h) and VolumeBrushTypes
Dispatch :147-213. PRECREATE: base first; ReadProp WORLDFILE only; always sets `m_Flags |= m_dwFlags` (ctor), `m_ObjectType=OT_CONTAINER`, `m_Filename=m_Name`, `m_ContainerCode = m_eContainerCode` (subclass ctor constant). INITIALUPDATE: always calls `InitialUpdate(fData)` which returns immediately for SAVEGAME (:397) → no surface, no fog SFX resend (engine copy kept), **no `m_hPlayerClass`**; CacheFiles always.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_fViscosity | F | already inverted (1 = free) |
| 2 | m_eContainerCode | B | |
| 3 | m_vCurrent | V | |
| 4 | m_fDamage | F | |
| 5 | m_nDamageType | B | |
| 6 | m_fSurfaceHeight | F | |
| 7 | m_bShowSurface | B | |
| 8 | m_vSurfaceColor1 | V | |
| 9 | m_vSurfaceColor2 | V | |
| 10 | m_vLastPos | V | |
| 11 | m_bHidden | B | |
| 12 | m_fGravity | F | |
| 13 | m_fLastDamageTime | F Rel | |
| 14 | m_fXScaleMin | F | |
| 15 | m_fXScaleMax | F | |
| 16 | m_fYScaleMin | F | |
| 17 | m_fYScaleMax | F | |
| 18 | m_fXScaleDuration | F | |
| 19 | m_fYScaleDuration | F | |
| 20 | m_hstrSurfaceSprite | HS | |
| 21 | m_dwFlags | DW | |
| 22 | m_dwSaveFlags | DW | |
| 23 | m_bLocked | B | |
| 24 | m_bUnlockKeyRemove | B | |
| 25 | m_hstrKeyName | HS | |
| 26 | m_bFogEnable | B | |
| 27 | m_fFogFarZ | F | |
| 28 | m_fFogNearZ | F | |
| 29 | m_vFogColor | V | |
| 30 | m_fSurfaceAlpha | F | |
| 31 | m_dwNumSurfacePolies | DW | |
Save :758-806 (OBJ write of m_hSurfaceObj commented out :765), Load :815-878. Load tail: `CreateSurface()` (:866; new PolyGrid — PolyGrid is ALWAYSLOAD so the old runtime surface was never saved) and user flags = `(hidden?0:VISIBLE)|SAVEABLE` on self and surface (:868-877; drops any other user bits).
Not saved: m_hSurfaceObj (recreated); **m_hPlayerClass** — set only in non-SAVEGAME InitialUpdate (:405) → NULL after restore → `UpdatePhysics` "not the player" test (:569) uses IsKindOf(cls, NULL) → original bug: after load the player is routed through the non-player path (viscosity/current/key-query) of every volume brush.
Subclasses (VolumeBrushTypes.h): Water/Blood/Acid/Ladder/Conveyor/Damage/Minefield/FreeFall — ctor constants only, no extra state. **CraneControl**: `m_hCameraObj`, `m_hCrane`, `m_hBall` (HOBJECT) and `m_fCamYaw` unsaved (VolumeBrushTypes.cpp:237-361) → after load a new "cranecam" CameraObj is created while the old SAVEABLE one stays orphaned; yaw resets to 0 though the crane rotation (engine) persists; the WreckingBall is ALWAYSLOAD (reset to world-file state).

## 25. PickupObject (PickupObject.cpp / .h) and pickups
Dispatch :115-188. PRECREATE: base first; ReadProp WORLDFILE/STRINGPROP; `PostPropRead` (:265-~330) always: ORs TOUCH_NOTIFY|VISIBLE|GRAVITY, clears SHADOW, `m_dwFlags = flags` (:271, overwritten by Load), `m_hstrObjectName = CreateString(m_szObjectName)` if NULL (:274; leaked when Load overwrites), model/skin filenames from ctor `m_szFile`. On SAVEGAME the re-added VISIBLE|TOUCH_NOTIFY **re-show a pickup hidden while awaiting multiplayer respawn**. INITIALUPDATE: `InitialUpdate` (:339-385: NextUpdate, SAVEABLE|NIGHTGOGGLESGLOW :352, rotate flag, dims, persistent SFX_POWERUP, display name) only non-SAVEGAME; MoveObjectToGround only WORLDFILE; SpawnItem only STRINGPROP; CacheFiles always.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_bFirstUpdate | B | |
| 2 | m_nType | B | |
| 3 | m_fValue | F | |
| 4 | m_fRespawnTime | F | |
| 5 | m_bRotate | B | |
| 6 | m_fYaw | F | |
| 7 | m_fLastTime | F Rel | |
| 8 | m_dwFlags | DW | |
| 9 | m_fBounce | F | |
| 10 | m_bBouncing | B | |
| 11 | m_vRestPos | V | |
| 12 | m_hstrPickupTriggerTarget | HS | |
| 13 | m_hstrPickupTriggerMessage | HS | |
| 14 | m_hstrObjectName | HS | |
| 15 | m_hstrDisplayName | HS | |
Save :617-640, Load :651-674. Not saved: m_bBounce, m_szFile, m_szObjectName, m_nNameID, m_fValueMult, m_szPickupSound, m_szRespawnSound — ctor constants or multiplayer multipliers recomputed by subclass INITIALUPDATE handlers that run **unconditionally** (AmmoPickups.cpp:45-63 also re-inserts into the static `m_PUHead` list used by AI_Mgr; ItemPickups.cpp:83-97/229-251; PowerupPickups.cpp:82/176/270; WeaponPickups.cpp:43) → safe. Respawn countdown = engine NextUpdate.
**KeyPickup** (ItemPickups.cpp:350-445): PRECREATE PickupObject first; props on WORLDFILE/STRINGPROP; filename/skin from strings only when set. Save/Load before PickupObject → stream: KeyPickup block then PickupObject block.
| 1 m_hstrFilename HS | 2 m_hstrSkinName HS | 3 m_hstrIconFile HS | 4 m_hstrIconFileH HS | 5 m_byUseCount B |
Save :480-491, Load :501-511.
(SpellPickups.cpp is not in ObjectDLL.dsp — not built.)

## 26. CProjectile (Projectile.cpp / .h; derives BaseClass, not B2BaseClass) — aggregate m_damage (:44)
Dispatch :134-197. PRECREATE: `m_dwClientID = ocs->m_UserData` (:153; on SAVEGAME m_UserData=0 → owner client id lost), BaseClass first, `PostPropRead` skipped on SAVEGAME (:158). INITIALUPDATE `InitialUpdate` (:293-333: NextUpdate, m_fStartTime, dims, force limit, friction, scale, damage init, SAVEABLE|SINGULARITY_ATTRACT :314, persistent SFX_OBJECTFX trail) skipped on SAVEGAME. MID_LINKBROKEN → BreakLink (:205-211). SAVE :184 / LOAD :188 before base.

| Order | Field | Type | Notes |
|---|---|---|---|
| 1 | m_vDir | V | |
| 2 | m_fVelocity | F | |
| 3 | m_dwFlags | DW | |
| 4 | m_nDamageType | B | |
| 5 | m_nRadius | DW | int |
| 6 | m_fStartTime | F Rel | CProjectile's member only |
| 7 | m_bRemoveFromWorld | B | |
| 8 | m_fDamage | F | |
| 9 | m_fLifeTime | F | |
| 10 | m_nDamageMsgID | DW | |
| 11 | m_bShockwave | B | |
| 12 | m_bExplosion | B | |
| 13 | m_bSparks | B | |
| 14 | m_vShockwaveScaleMin | V | |
| 15 | m_vShockwaveScaleMax | V | |
| 16 | m_vLightColor | V | |
| 17 | m_fShockwaveDuration | F | |
| 18 | m_hstrShockwaveFilename | HS | |
| 19 | m_vDims | V | |
| 20 | m_bExplode | B | |
| 21 | m_LastPos | V | |
| 22 | m_bArmed | B | |
| 23 | m_fPitchVel | F | |
| 24 | m_fYawVel | F | |
| 25 | m_fPitch | F | |
| 26 | m_fYaw | F | |
Save :830-870, Load :882-920. Then CDestructable block.
Not saved:
- `m_hFiredFrom` (comment "Can't tell" :839) — interlink (Setup :237) restored by engine but member NULL → shooter no longer excluded in HandleTouch (:401), damage attributed to NULL.
- `m_hHitObject` — if saved with m_bExplode=TRUE, non-radius projectiles explode with no direct target.
- `m_nWeaponType` — never initialised by the ctor → garbage after load; used when spawning sub-projectiles (GameProjectiles.cpp:1438/1819/2974).
- `m_hSmokeTrail` (CClientSmokeTrail, not SAVEABLE), `m_hLight` (LightFX, ALWAYSLOAD) → children gone, members NULL; `m_hSound` → projectile loop not restarted.
- ctor/init constants: m_pProjectileFilename/Skin, m_vInitScale, m_bSmokeTrail, m_bClientFX, m_fImpactScaleMin/Max, m_fImpactDuration, trail offset/scale/flags/FX id.
- `m_dwClientID` (see PRECREATE).
- The engine-persisted trail SFX embeds `WriteToMessageObject(m_hObject)` (:323) as raw bytes → carries the pre-save object id (risk of a stale reference after restore; engine-dependent).
**GameProjectiles subclasses**: no MID_SAVEOBJECT anywhere in GameProjectiles.cpp; all SAVEABLE (overriding InitialUpdates call the base: :2389, :3504, :4689, :6027, :6600, :6943). Unsaved subclass state (GameProjectiles.h):
- bounce counters: CGrenade/CGasGrenade/CRockProjectile m_nBounceCount, CNapalmFireball m_nBounces.
- CGasGrenade m_bSmoking, m_fSmokeTime (Abs), m_fLastWonkyTime (Abs). CProximityBomb m_fGetAwayTime (raised to 2.0 at runtime), m_bHitCharacter, m_bPlayArmSound, m_bPlayDetectSound. CRemoteBomb m_bFalling.
- **Shadowed `m_fStartTime`** (own member hiding the saved CProjectile one) in Napalm, NapalmAlt, BugSpray, BugSprayAlt, DeathRay, LeechPrime, Flare, FlareAlt, FlareBurst, Tesla, TeslaBolt, TeslaBall, Singularity, SingularityAlt, EnergyBlast, Fireball, GroundStrike, ShikariLoogie, Vomit, Goo → re-initialised by the ctor (0 or construction-time GetTime()) → delay/attract/burst/bounce phases restart or expire immediately; also their m_fDelay.
- DeathRay m_fBounceDist, m_vBeamDir, m_bFirstUpdate, m_bFirstShot; LeechPrime m_vColor, m_fCurve, m_fAmp; Flare m_vObjOffset, m_bTrackObj, m_fLastDamageTime, m_fDamageTime, m_nColor; FlareAlt m_fBurstTime, m_vVel, m_nExpType, m_bGetVelocity; EnergyBlast m_bZealot, m_bDivine; TeslaBall m_fAttractRadius, m_fLastFireTime, m_hLastObj.
- Trackers/attachers: COrbProjectile & CSkullProjectile m_hTrackObj, m_fRotateAmount, m_fHitTime, m_fLastDamageTime, m_bState; COrbAltProjectile m_fHitTime, m_fLastDamageTime, m_bState, m_bSwitchView, m_bCreateCamera; CFlayerChain m_hTrackObj, m_fHitTime, m_fLastDamageTime, m_fRetractTime, m_bState, m_vHookPos1/2, m_byNumLinks, m_fScale, m_fStretchedLength, m_hGibAttachment, m_hGibObj, m_bFirstUpdate; CFlayerPortal m_fSearchTime, m_fLastChainTime; CRock m_fFadeTime; CNightmareFireball m_hTrackObj, m_vHitLocation. Engine restores their interlinks/attachments but the handles/states are lost → tracking/attached behaviour resets to ctor state.

---

## 27. Cross-cutting summary

### 27.1 Absolute vs relative times
- Relative (`t - now` / `v + now`): CameraObj.m_fDeactivateTime (then clobbered by SetActive), Trigger.m_fDelayStartTime, Rotating.m_fLastTime/m_fStartTime, TripLaser.m_fDamageTime, VolumeBrush.m_fLastDamageTime, PickupObject.m_fLastTime, CProjectile.m_fStartTime.
- Absolute raw: ClientLightFX.m_fStartTime, CDestructableModel.m_fLastTime. Correct only because full restore uses `RESTOREOBJECTS_RESTORETIME`; KeepAlive (no time restore) never carries world objects.
- Inherently relative (countdowns/elapsed): Door.m_fDoorOpenTime, KeyFramer.m_fCurTime + KeyData times, Rotating.m_vSpinTimeLeft.
- Engine NextUpdate is saved as remaining time and drives most timers (CinematicTrigger delays, Trigger reset, Spawner rate, SoundFX ramps, pickup respawn) — unless the class overrides it during restore: Spawner (INITIALUPDATE → 0, bug), SoundFX (Load → 0, bug), CameraObj/ClientLightFX/SoundFX PostPropRead (PRECREATE), Lightning/LaserBeam/Explosion (0.1), WarpGate (1.0), CameraObj Load (SetActive).
- Unsaved absolute times: GameProjectiles shadowed m_fStartTime and hit/damage/fade/search timers.

### 27.2 PRECREATE / INITIALUPDATE on restore
- Properties are never re-read on PRECREATE_SAVEGAME, except the unconditional ReadProp in CClientLightningSFX / CClientLaserBeamSFX / CClientExplosionSFX (harmless garbage + leaked HSTRING, overwritten by Load).
- PRECREATE post-processing that still runs on SAVEGAME: CameraObj PostPropRead (overwrites Flags, NextUpdate), ClientLightFX PostPropRead (overwrites Flags to VISIBLE|GOTHRUWORLD, NextUpdate), SoundFX PostPropRead (NextUpdate), Door/CDestructableBrush PostPropRead (Filename=Name, ORed flags), VolumeBrush (type/filename/container code/flags OR), TripLaser (filename/skin), PickupObject PostPropRead (re-adds VISIBLE|TOUCH_NOTIFY, leaks a name string), RotatingDoor deg→rad (overwritten), CProjectile m_dwClientID=0.
- INITIALUPDATE_SAVEGAME is skipped (body not executed) by almost every class; unconditional INITIALUPDATE work: CameraObj list insert, Spawner (NextUpdate bug), WarpGate, Lightning/LaserBeam/Explosion, Rotating*Init, AmmoPickup list insert, pickup MP multipliers, CDestructableModel (B2BaseClass call only), CacheFiles in many.

### 27.3 Object references / links
- Saved with OBJ: CameraObj.m_hLinkObject, Door.m_hTriggerObj, Rotating.m_hObject (self), CDebris.m_hObject (self) (+ CDestructable, separate). OBJ targets outside the save list (unsaveable or ALWAYSLOAD) restore as NULL.
- Interlinks are restored by the engine after all Loads; no world class re-creates links on load except KeyFramer, which rebuilds its list and calls CreateInterObjectLink again (duplicate links).
- Not saved: Trigger.m_hLastSender (unused), KeyFramer.m_pObjectList (rebuilt), VolumeBrush.m_hSurfaceObj (recreated), CraneControl handles, CProjectile m_hFiredFrom/m_hHitObject/m_hSmokeTrail/m_hLight, projectile tracker handles, CameraObj.m_hRay, HCLASS VolumeBrush.m_hPlayerClass (bug).
- MID_LINKBROKEN handled by CameraObj, KeyFramer (list removal without offset compaction), CProjectile (+ CFlayerChain/CNightmareFireball BreakLink), FireFX/Explosion/Script (not saved anyway).

### 27.4 Sounds (no HSOUNDDE is ever saved)
- Restarted in Load: SoundFX (from offset 0; progression then broken), TripLaser (if on).
- Implicitly restarted: CinematicTrigger (replays the current cue + re-sends its trigger message).
- Lost: Door busy loops, Rotating busy loops, KeyFramer key sounds, CDestructableModel sliding loop, CProjectile flight loop, RandomSoundFX (object dropped), ObjectivesTrigger sound string. Built-in `Sound` objects are ALWAYSLOAD → restart fresh.

### 27.5 Client SFX
- Persistent object SFX messages are engine-saved and replayed (Camera, ParticleStream, ClientLight, DestructableModel, Pickup, VolumeBrush fog, Rain, Projectile trail); classes do not re-send them, except Rain::Load (redundant re-send).
- Instant SFX are never persisted: WarpGate's three setup messages (effects lost after restore — bug); Lightning/LaserBeam/Explosion/Debris/impacts are one-shots (acceptable).
- Persistent messages that embed `WriteToMessageObject` (CProjectile trail) are saved as raw bytes with the old object id (engine-dependent stale-reference risk).

### 27.6 Transforms / positions
World-model and object positions/rotations, dims, scale, velocity, acceleration, flags, user flags, color, filenames/skins and model animation are engine-saved; classes save only their own path/state data (Door open/closed endpoints + state, RotatingDoor and Rotating current Euler angles, KeyFramer timeline + key list + offsets). ALWAYSLOAD objects (plain WorldModel/Model/Sprite) are *not* saved → anything that moved them (KeyFramer, CraneControl) loses that pose unless it re-applies it.

### 27.7 Known original bugs / quirks (save-related)
1. RandomSoundFX never SAVEABLE → all random ambient sounds vanish on restore (Save/Load dead code).
2. ClientLightFX SAVEABLE only if initially on → lights placed OFF vanish on restore.
3. CClientWarpGateSFX setup uses instant SFX + restored bFirstUpdate=0 → warp gate visuals not recreated.
4. Spawner INITIALUPDATE resets NextUpdate from ctor defaults → running spawners stop after restore.
5. SoundFX Load: SetNextUpdate(0) after UpdateSound → ramp/one-shot state machine frozen.
6. VolumeBrush.m_hPlayerClass not restored → player treated as non-player in container physics after restore.
7. ObjectivesTrigger.m_hstrSound not saved.
8. CameraObj Load→SetActive restarts timed-camera duration.
9. KeyFramer: offsets saved by key count instead of object count; key count as byte (desync >255); object list rebuilt with duplicate links/possible reordering; key sounds lost; ALWAYSLOAD Keys reappear.
10. CinematicTrigger replays the current cue and re-fires its trigger message.
11. Door/Rotating busy loops and DestructableModel sliding loop not restarted.
12. CProjectile: m_hFiredFrom/m_hHitObject/m_nWeaponType/m_dwClientID lost; GameProjectiles subclass state (incl. shadowed m_fStartTime) unsaved.
13. CraneControl handles/yaw unsaved (duplicate crane camera after restore).
14. PathList save skips NULL elements but writes the full count; PickupObject/ClientLightning/LaserBeam leak HSTRINGs on restore; PickupObject PostPropRead re-shows hidden MP pickups.
15. Dead declarations: RotatingBrush.h/RotatingModel.h Save/Load; PathMgr::Save/Load unused.

---

## 28. Registered world classes with no Save/Load of their own or in any ancestor beyond BaseClass/B2BaseClass/CClientSFX
(Only classes in ObjectDLL.dsp; characters/AI, CPlayerObj, corpse, SeeingEye, Tchernotronic excluded. None of these sets USRFLG_SAVEABLE.)

### 28.1 CF_ALWAYSLOAD — reloaded fresh from the world file, never saved
| Class | File | Verdict |
|---|---|---|
| GameStartPoint (StartPoint) | GameStartPoint.cpp:22 | safe (static) |
| PathPoint | pathpoint.cpp:20 | safe (PathMgr rebuilt in PostStartWorld) |
| ExitHint | ExitHint.cpp:9 | safe (static AI hint) |
| WorldProperties | WorldProperties.cpp:65 | safe (re-applies world settings; console vars also saved) |
| CameraSpot | CameraSpot.cpp:15 | safe (static) |
| Key | Key.cpp:23 | safe-ish: reappears after KeyFramer consumed it; inert |
| SmellHint | SmellHint.cpp:11 | world-file ones safe; player-dropped runtime hints lost (AI smell trail reset) |
| PolyGrid | PolyGrid.cpp:39 | world-file ones reset; VolumeBrush surfaces recreated by VolumeBrush::Load |
| ParticleSystem | ParticleSystem.cpp:32 | data loss: triggered on/off state reset |
| SparksObj | SparksObj.cpp:30 | data loss: on/off + timers reset |
| LightFX | LightFX.cpp:73/75 | data loss: on/off/damage reset, destroyed lights reappear, runtime-created LightFX (ObjectUtilities.cpp:188) lost |
| Script | Script.cpp:41 | data loss: progress lost; scripts with a start time auto-run again (InitialUpdate :148-158) |
| WreckingBall | WreckingBall.cpp:24 | data loss: momentum/position reset |
| SoccerGoal | SoccerObjects.cpp:51 | multiplayer only |
| Engine Model/WorldModel/Container/Sprite/Sound/Brush/sky/FogSphere | AppHeaders/engineobjects.c | reset to world file (poses set at runtime by KeyFramer etc. lost) |

### 28.2 Neither SAVEABLE nor ALWAYSLOAD — removed entirely by a full restore
| Class | File | Verdict |
|---|---|---|
| FireFX | FireFX.cpp:25 | data loss (world-placed fire, link/children/timers) |
| Explosion | Explosion.cpp:49 | data loss (world-placed, pending trigger/delay lost) |
| RandomSoundFX | RandomSoundFX.cpp:44 | data loss (see bug 1; has dead Save/Load) |
| ClientLightFX family placed OFF | ClientLightFX.cpp:49-140 | data loss (see bug 2) |
| FlagObject, FlagStand | FlagObjects.cpp:39/483 | CTF only |
| SoccerBall | SoccerObjects.cpp:452 | multiplayer only |
| CGib (+CDestructable aggregate) | Gib.cpp:98 | transient gore, acceptable |
| NagaCeilingDebris | NagaCeilingDebris.cpp:9 | transient boss debris, acceptable |
| CModelObject | ModelObject.cpp:18 | transient helper, acceptable |
| CImpact, CImpactRicochet | Impacts.cpp:21/379 | transient, acceptable |
| CShellCasing | ShellCasing.cpp:29 | transient, acceptable |
| CHandWeaponModel, CViewWeaponModel | HandWeaponModel.cpp:18, ViewWeaponModel.cpp:27 | owner-created; recreated by character/player restore (separate audit) |
| CBulletImpactSFX, CClientCastLineSFX, CClientMarkSFX, CClientLaserFX, CClientSparksSFX, CClientGibFX, CClientSplashSFX, CClientWeaponSFX, CClientSmokeTrail, CClientTracer, CClientSplatFX, CClientDebugLine, CClientBloodTrail | respective .cpp:17-20 | transient client FX; decal-like CClientMarkSFX / blood trails are lost (cosmetic) |

### 28.3 SAVEABLE but relying on ancestors' Save/Load (no own block)
BigTrigger; GlowingLight & all *WaveLightFX/FlickerLight; RotatingBrush/RotatingModel/RotatingSprite (Rotating aggregate saved); Water/Blood/Acid/Ladder/Conveyor/Damage/Minefield/FreeFall (safe) and CraneControl (data loss, §24); all Ammo/Powerup/Item/Weapon pickups (safe; KeyPickup has its own block); all GameProjectiles subclasses (data loss, §26).
