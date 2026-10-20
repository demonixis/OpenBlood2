# Blood II (1998) original Save/Load audit — characters, AI, inventory

Source root: `lithtech/Blood2` in the LithTech reference tree. Paths below are relative to
`Blood2/ObjectDLL` unless prefixed. "Later engine" = `lithtech/runtime/server/src/game_serialize.cpp`
(LithTech Talon/Jupiter era), used only as corroboration — not a verified 1998 trace.

Write-type abbreviations: `B`=WriteToMessageByte, `W`=Word, `DW`=DWord, `F`=Float, `V`=Vector,
`R`=Rotation, `HS`=HString, `S`=String, `LSO`=WriteToLoadSaveMessageObject, `MO`=WriteToMessageObject
(plain, documented as *invalid* inside MID_SAVEOBJECT: `AppHeaders/CSBase.h:49-52`), `HMR`=WriteToMessageHMessageRead.

---------------------------------------------------------------------------------------------------

## 0. Framework

### 0.1 Who saves what, and with which dwParam (`BloodServerShell.cpp`)

| Path | Call | dwParam (→ fData) | Flags | Object set |
|---|---|---|---|---|
| Quick/auto save (CMSG_SAVEGAME, :401-429) | `SaveGame(type, bSavePlayers=TRUE, bSaveConsole=TRUE)` :1004 → `SaveObjects(path, list, 0, SAVEGAMECONSOLE\|SAVEPORTALS)` :1058 | **0** | console + portals | every active **and inactive** object with `USRFLG_SAVEABLE` (:1022-1050) |
| Restore save | `RestoreGame` :1075 → `RestoreObjects(path, 0, RESTOREOBJECTS_RESTORETIME)` :1090 | **0** | **restores game time** | — |
| Level change (LOADTYPE_NEW_LEVEL :904-916) | `KeepAliveSave` :1142 → `SaveObjects("Keep.sav", list, 1, 0)` :1174 | **1** | none | `IsPlayer(hObj)` only, active list only (:1158-1170) |
| Level change restore | `KeepAliveLoad` :1191 → `RestoreObjects("Keep.sav", 1, 0)` :1204 | **1** | **no time restore** | — |

* `LoadWorld` (:828) loads with `LOADWORLD_NORELOADGEOMETRY`, and `LOADWORLD_LOADWORLDOBJECTS` only when *not* restoring (:931-941); restore therefore recreates only `CF_ALWAYSLOAD` world objects (PathPoint, SmellHint, ExitHint, engine Model/Sound/...) from the DAT and everything else from the save.
* `GAME_GOTOSTARTPOINT` = `(!bRestoring || bRestoringKeepAlive)` (:975) → keep-alive players are teleported by `RespawnPlayer → GoToStartPoint` (:782-786); full restores keep the saved position and send `SMSG_FORCEROTATION`/`SMSG_EYELEVEL` (:788-791).
* Restored player ↔ client re-association is by engine **client refs** (`OnClientEnterWorld` :192-238 → `RespawnPlayer` → `CPlayerObj::SetClient` → `CInventoryMgr::SetClient`).
* `CultistAI::ResetStatics()` is called on every LoadWorld (:835) — only resets `m_bMaleAnims`.

### 0.2 Message dispatch order (stream layout)

* C++ classes handle `MID_SAVEOBJECT`/`MID_LOADOBJECT` in their own `EngineMessageFn` **before** forwarding to the parent; the chain ends in `BaseClass::EngineMessageFn` → `bc_EngineMessageFn`, which calls the **aggregates last** (`AppHeaders/engineobjects.c:117-123`).
* `bc_AddAggregate` **prepends** (`engineobjects.c:6-10`), so aggregates run in **reverse AddAggregate order**. `CBaseCharacter` adds `m_damage, m_InventoryMgr, m_MoveObj` (`BaseCharacter.cpp:34-36`) ⇒ aggregate order is **CMovement → CInventoryMgr → CDestructable**.
* Resulting per-object stream for every AI: `[MostDerived::Save][AI_Mgr::Save][CBaseCharacter::Save = 0 bytes][CMovement][CInventoryMgr][CDestructable]`. Player: `[CPlayerObj][—][CMovement][CInventoryMgr][CDestructable]`. CCorpse/SeeingEye: `[class][CDestructable]`.
* `fData` is the raw dwParam (0 full / 1 keep-alive) cast to DDWORD everywhere.
* Exceptions: Thief/BoneLeech/TheHandAI call `AI_Mgr::EngineMessageFn` explicitly inside `MID_LOADOBJECT` and post-process after the aggregates (same byte order).

### 0.3 Restore lifecycle

1. `MID_PRECREATE` with `PRECREATE_SAVEGAME (3.0)` — engine has already filled the create struct from the save (later engine `game_serialize.cpp:705-712`). Base `bc_EngineMessageFn` reads props only for 1.0/2.0 (`engineobjects.c:22-24`); `B2BaseClass`, `AI_Mgr`, `CDestructable` likewise skip `ReadProp` (`B2BaseClass.cpp:56-61`, `AI_Mgr.cpp:696-701`, `Destructable.cpp:157-162`).
2. `MID_INITIALUPDATE` with `INITIALUPDATE_SAVEGAME (3.0)`.
3. After **all** objects exist: `MID_LOADOBJECT` for each object in save order (later engine :1294-1312), so `ReadFromLoadSaveMessageObject` can resolve forward references.
4. Later engine then restores **attachments and inter-object links** between saved objects (`sm_RestoreAttachments/sm_RestoreInterlinks`, :1316-1333), and has already restored `m_NextUpdate` (:487/:705), flags, user flags, pos/rot/scale, model+skins, anim trackers, color/alpha, dims, mass, velocity, acceleration, friction, internal (active/inactive) flags, client ref.

Original-code evidence that INITIALUPDATE_SAVEGAME precedes LOADOBJECT in the 1998 engine:
`CInventoryMgr::Load` "Term(); // Get rid of any data that was added in initial update" (`InventoryMgr.cpp:2934-2935`);
`CultistAI::Load`/`CivilianAI::Load` dereference `m_pServerDE`, which is only assigned in their `InitialUpdate` (`CultistAI.cpp:212`, `CivilianAI.cpp:303`);
`CDestructable::Load` "don't reload m_hObject if it is already set" (`Destructable.cpp:1196-1202`) — set by `Init()` during INITIALUPDATE.

Engine game time: later engine `LoadWorld` sets `m_GameTime = 0` (`servermgr.cpp:2576,2673`); `RESTOREOBJECTS_RESTORETIME` reloads the saved time (`game_serialize.cpp:1249-1266`). ⇒ **Full restore: clock continues from the save time** (absolute deadlines saved raw stay valid). **Keep-alive restore: clock restarts near 0 in the new world**; only relative/duration encodings are meaningful.

### 0.4 Object references

* `LSO`/`ReadFromLoadSaveMessageObject`: serialize-ID of an object in the save list; an object **not** in the list (non-saveable, `CF_ALWAYSLOAD`, going away, or anything but players in a keep-alive save) reads back as **NULL** (later engine `WriteObject`/`ReadObject` :277-288, :190-217).
* `MO`/`ReadFromMessageObject` inside a save is unsupported per SDK; used by Naga (`m_hSpike`) and CPlayerObj (`m_hCameraObj`). Write/read calls are paired, so the stream stays aligned, but the restored handle is meaningless (and in the 1998 engine its exact behaviour is unverified).

---------------------------------------------------------------------------------------------------

## 1. CBaseCharacter (`BaseCharacter.cpp/.h`)

* Dispatch: `MID_SAVEOBJECT → Save(pData,(DDWORD)fData)`, `MID_LOADOBJECT → Load(...)` (:69-75), then `B2BaseClass::EngineMessageFn` (→ aggregates).
* `Save`/`Load` are **empty** (:439-445, :455-461). Writes 0 bytes.
* INITIALUPDATE (:77-91): only when `fData != INITIALUPDATE_SAVEGAME` — adds `FLAG_MODELGOURAUDSHADE|FLAG_ANIMTRANSITION`, **overwrites** user flags with `USERFLG_NIGHTGOGGLESGLOW`, then ORs `(m_dwSurfType<<24) + USRFLG_SAVEABLE | USRFLG_SINGULARITY_ATTRACT`. On restore those flags come from the engine-saved user flags.
* Not saved (regenerated as noted):

| Field | Fate after restore |
|---|---|
| `m_eContainerCode`, `m_eLastContainerCode`, `m_bBodyInLiquid`, `m_bBodyOnLadder`, `m_bSwimmingOnSurface` | ctor defaults; recomputed by container physics next frames |
| `m_dwSurfType` | ctor `SURFTYPE_FLESH`; surface also lives in saved user flags |
| `m_fSwimVel`, `m_fLadderVel` | ctor defaults |
| **`m_nTrapped`, `m_bTrappedGrav`** | **lost** → reset to 0/TRUE (a trapped/imprisoned character is freed on restore; player `m_bImprisoned` is saved separately) |
| `m_Anim_Sound` (instance) | player: rebuilt by `SetCharacter` in `CPlayerObj::Load`; AIs use class statics |

---------------------------------------------------------------------------------------------------

## 2. CDestructable aggregate (`Destructable.cpp/.h`)

* Dispatch (aggregate `EngineMessageFn` :151-199): SAVE/LOAD → Save/Load(fData); dwParam ignored.
* Save :1134-1176 / Load :1189-1245.

| # | Field | Type | HOBJECT | Notes |
|---|---|---|---|---|
| 0 | marker `0xcececece` | DW | | read & ignored |
| 1 | `m_hObject` | LSO | self | Load: only stored if `m_hObject` still NULL ("gross hack because PlayerObj hObject doesn't seem to be loaded", :1196-1202); normally `Init()` set it during INITIALUPDATE |
| 2 | `m_bApplyDamagePhysics` | B | | |
| 3 | `m_fMass` | F | | engine mass not re-applied by Load (`SetMass` not called; engine mass is engine-saved) |
| 4 | `m_fMaxHitPoints` | F | | |
| 5 | `m_fMaxMegaHitPoints` | F | | |
| 6 | `m_fHitPoints` **or 1.0e6** | F | | if `!m_bDestructable` writes sentinel 1e6 (real HP lost) |
| 7 | `m_fDeathHitPoints` | F | | |
| 8 | `m_fMaxArmorPoints` | F | | |
| 9 | `m_fMaxNecroArmorPoints` | F | | |
| 10 | `m_fArmorPoints` | F | | |
| 11 | `m_fResistance` | F | | |
| 12 | `m_bDead` | B | | |
| 13 | `m_bNighInvulnerable` | B | | |
| 14 | `m_nLastDamageType` | B | | |
| 15 | `m_nLastDamageLocation` | B | | **never initialized or assigned anywhere** (ctor :45-98 omits it) → garbage byte round-trips |
| 16 | `m_vLastDamageDirection` | V | | |
| 17 | `m_fLastDamageAmount` | F | | reset each MID_UPDATE anyway (:174-176) |
| 18 | `m_fLastDamagePercent` | F | | idem |
| 19-22 | `m_hstrDamageTriggerTarget/Message`, `m_hstrDeathTriggerTarget/Message` | HS×4 | | Load overwrites without freeing (NULL on restore, so no leak) |
| 23 | `m_hstrSpawnObject` | HS | | |
| 24 | `m_vSpawnObjectVel` | V | | |
| 25 | `m_bGodMode` | B | | |
| 26 | `m_bTriggerOnly` | B | | |
| 27 | `m_fDeathDelay` | F | | **countdown duration** (decremented by frame time :177-178) → time-base independent |
| 28 | marker `0xcececece` | DW | | |
| (derived) | `m_bDestructable = (m_fHitPoints < 1e6)` | | | any real HP ≥ 1e6 also becomes non-destructable |
| (post) | `m_Link` inserted into `m_DestructableHead` if not yet | | | :1239-1244 |

* Not saved: `m_hWhoKilledMeLast` (no link), **`m_hLastDamager`** (has a link :478-483; MID_LINKBROKEN clears it :188-196) → NULL after restore; `m_nSideHit`, `m_nNodeHit`, `m_bAddVelocity`, `m_vAddVelocity` (pending damage impulse lost); `m_pInventoryMgr`, `m_pAnim_Sound` (set by `Init` in INITIALUPDATE — see Cultist/Civilian bug §6/§7); `m_bDestructable` (derived).

---------------------------------------------------------------------------------------------------

## 3. CMovement aggregate (`movement.cpp/.h`)

* Dispatch :37-43. Save :584-606 / Load :617-638. dwParam ignored.

| # | Field | Type | HOBJECT | Notes |
|---|---|---|---|---|
| 1 | `m_hObject` | LSO | self | NULL for the player (player never calls `m_MoveObj.Init`) |
| 2 | `m_rRot` | R | | cached transform (refreshed next MID_UPDATE :103-118) |
| 3-5 | `m_vRight`, `m_vUp`, `m_vForward` | V | | |
| 6 | `m_vPos` | V | | |
| 7 | `m_vLastPos` | V | | |
| 8 | path count `m_PathList.m_nElements` | DW | | |
| 9.. | each path point | V | | Load re-adds via `AddPosToPathList` (also bumps `m_nNumPoints`); list not cleared first (empty on restore) |

* Not saved: `m_pOwner` (stays NULL after restore because `AI_Mgr::InitialUpdate` returns before `m_MoveObj.Init` on SAVEGAME, `AI_Mgr.cpp:2030-2033,2096`; field otherwise unused), `m_nNumPoints` (rebuilt), `m_nWidthPoints` (uninitialized scratch, reset inside `FindShortestTurn`).

---------------------------------------------------------------------------------------------------

## 4. Inventory: CInventoryMgr, CInvItem, game items, CWeapon

### 4.1 CInventoryMgr aggregate (`InventoryMgr.cpp/.h`)

* Dispatch :2534-2564: SAVE/LOAD → Save/Load(fData). MID_LINKBROKEN only clears `m_hMuzzleFlash`.
* Save :2843-2917 / Load :2929-3064. **dwParam used**: `Load` drops `INV_KEY` items when `dwLoadType == 1` (keep-alive) (:3024-3032) after fully reading them.

| # | Field | Type | Notes |
|---|---|---|---|
| 0 | marker | DW | |
| 1 | `m_nAttribStrength` | B | |
| 2 | `m_nAttribMagic` | B | |
| 3 | `m_bGodMode` | B | |
| 4 | `m_bInfiniteAmmo` | B | |
| 5 | for slot 0..9: `m_Weapons[i]->GetType()` or `WEAP_NONE`, then `m_LWeapons[i]` type | B×20 | Load: `ObtainWeapon(type, i)` twice; 2nd call becomes left hand if `IsDualHanded()` (:442-448) — **weapons are re-created, no CWeapon state saved** |
| 6 | for inv slot 0..2: `type + INV_BASEINVWEAPON - WEAP_PROXIMITYBOMB` or `INV_NONE` | B×3 | Load: `AddInventoryWeapon(invType, i)` — **2nd arg is a count, receives the slot index** (0/1/2); masked because ammo is overwritten next |
| 7 | `m_fAmmo[0..AMMO_MAXAMMOTYPES]` (13) | F×13 | |
| 8 | item count `m_InvItemList.m_nElements` | DW | NULL entries would be skipped while count is not → desync (latent) |
| 9.. | per item: `GetType()` | B | Load `switch` → `new CInv*`; `Init(m_hOwner)` **then** `Load()` (Init-created HSTRINGs leak, overwritten) |
| 9.. | per item: `CInvItem::Save` (+subclass) | | see 4.2 |
| 10 | `m_nCurWeaponSlot` | DW | |
| 11 | `m_nCurWeapon` | DW | Load sets `m_pCurWeapon = m_Weapons[slot]` (no bounds check) |
| 12 | current item type or `INV_NONE` | DW | Load `SelectItem(type)` (first item of that type) |
| 13 | `m_WeapCount` | DW | **never initialized/used** → garbage round-trips |
| 14 | `m_fDamageMultiplier` | F | overwrites multipliers applied during INITIALUPDATE_SAVEGAME (Cultist 0.5, Zealot divine 1.5) |
| 15 | `m_fFireRateMultiplier` | F | |
| 16 | `m_fMeleeMultiplier` | F | **never initialized/used** → garbage round-trips |
| 17 | `m_bDropEye` | B | |
| 18 | marker | DW | |

Load prologue: `Term()` (deletes weapons/items, removes view models and **muzzle-flash object**) then `Init(m_hOwner, m_hClient)` (recreates muzzle flash + `CreateInterObjectLink(owner, flash)`) (:2934-2935, :173-200). `m_hOwner` comes from INITIALUPDATE_SAVEGAME `Init`; `m_hClient` is NULL at restore (client attached later via `SetClient`).

Quirks: during Load, `Term()` reset `m_nCurWeaponSlot = 0`, so only the slot-0 weapon gets `Draw()`/`SetupMuzzleFlash()` inside `ObtainWeapon` (:741-750); the real current weapon (if another slot) is installed by pointer only. `ObtainWeapon`'s `default: return DFALSE` (=CHWEAP_OK) silently ignores unknown types (:671).

Not saved: `m_hClient`; `m_pViewModel/m_pLViewModel`, `m_hViewModelAttach/m_hLViewModelAttach`, `m_hMuzzleFlash` (recreated); `m_pCurWeapon` (derived); `m_fManaRechargeTime` (unused); `m_hLastPickupObject`, `m_nLastPickupResult`; `m_hLastDroppedItem`, `m_fLastDroppedTime` (absolute time); **`m_bSwitchingWeapons`, `m_nNextWeaponSlot`** (a weapon switch in progress is lost); `m_pLast*Item`, `m_nLast*Charge` (reset to force client refresh :3055-3061); `m_bShowItems`.

### 4.2 CInvItem base (`InvItem.h` inline :186-238)

| # | Field | Type | HOBJECT | Notes |
|---|---|---|---|---|
| 1 | `m_hOwner` | LSO | owner | read into temp, discarded (owner comes from `Init`) |
| 2 | `m_hstrItemName` | HS | | |
| 3 | `m_hstrDisplayName` | HS | | |
| 4 | `m_hstrPic` | HS | | |
| 5 | `m_hstrPicH` | HS | | |
| 6 | `m_bInitialized` | B | | |
| 7 | `m_bDropped` | B | | **not initialized by CInvItem ctor** (only TheEye::Init sets it) → garbage for other items |
| 8 | `m_nType` | B | | overwrites ctor type |
| 9 | `m_bIsActive` | B | | **forced FALSE after read on every load** (comment says keep-alive only; code unconditional) |
| 10 | `m_nCount` | B | | charges |

### 4.3 Item subclasses (`GameInvItems.cpp`)

| Class | Extra Save (after base) | Load extras / notes |
|---|---|---|
| CInvFlashlight :253-286 | `m_fDischargeTime` F (**remaining duration**) | Load always `CreateLight()` (new BaseClass light + SFX) even if never used; "deactivate on keepalive" code computes `dwUsrFlags &= ~USRFLG_VISIBLE` but **never calls SetObjectUserFlags** (dead code; new light is invisible anyway). `m_hLight` not saved. |
| CInvMedkit | — | Init strings leak; count from save |
| CInvNightGoggles :550-578 | `m_fDischargeTime` F (duration) | inactive after load (base) |
| CInvBinoculars, CInvKey, CInvProxBomb, CInvRemoteBomb, CInvTimeBomb | — | keys dropped on keep-alive load |
| CInvTheEye :932-972 | `m_hSeeingObject` **LSO** | Load: if non-NULL, `HandleToObject(m_hOwner)` cast to **CPlayerObj without IsPlayer check**, `pEye->Init(owner, pObj->GetClient())` (client is NULL at restore) → eye's `m_hClient` stays NULL; else `m_bDropped = FALSE`. Keep-alive: SeeingEye not in Keep.sav → NULL → eye considered picked up. |

### 4.4 CWeapon / GameWeapons (`Weapon.cpp/.h`, `GameWeapons.cpp/.h`)

No Save/Load anywhere. Weapons are rebuilt from type bytes by `ObtainWeapon`/`AddInventoryWeapon` → `new` + `Init(owner, inv, bLeftHand, muzzleFlash)`. Lost per weapon: `m_eState` (firing/holster state), `m_fLastShotTime`, `m_fFireTime`, `m_fIdleStartTime/m_fIdleDelay`, `m_bLastFiring`, `m_bLastFireAlt`, `m_fFlashStartTime/m_bFlashShowing`, `m_hCurFireSounds[]`+end times (looping fire sounds), `m_nUpdateWait`, `m_fChromeValue`, hand model/attachment (recreated lazily by `ShowHandModel/Draw`), view model binding (via `SetClient`). Subclass state lost: CShotgun `m_dwCurBarrel`, `m_nOrigFireAnim`, `m_bPlayImpactSound`; CVoodooDoll `m_nHitType`, `m_nHitFX`; CDeathRay `m_fLastDamageTime`.

---------------------------------------------------------------------------------------------------

## 5. AI_Mgr (`AI_Mgr.cpp/.h`) — base of every NPC

### 5.1 Dispatch

* `MID_PRECREATE`: `ReadProp` only for WORLDFILE/STRINGPROP (:696-701).
* `MID_INITIALUPDATE`: `if (fData != WORLDFILE) m_bMoveToGround = FALSE; InitialUpdate((int)fData)` (:687-694). `InitialUpdate` (:2022-2141) always does `m_InventoryMgr.Init(m_hObject)` (creates muzzle flash + link) and `m_damage.Init(m_hObject, &m_InventoryMgr, m_pAnim_Sound)`, then **returns for SAVEGAME** (:2030-2033): no weapons/ammo/HP/flags/list insertion/scale/SetAnimation/SetNextUpdate/MoveObjectToGround/`m_fLastUpdate`.
* `MID_SAVEOBJECT`: `Save(pData,(DDWORD)fData)` (:739-741). `MID_LOADOBJECT`: `Load(...)` then **`InsertMe()`** (Cabal/Monster list, :743-746, :1999-2014) — runs before the aggregates load.
* Then `CBaseCharacter::EngineMessageFn`.
* `MID_LINKBROKEN` (:716-737): clears `m_hTarget`, `m_hTrackObject`; if `m_hCurSmell` matches, clears `m_hTrackObject` and `SetNewState(STATE_Idle)`.
* dwParam ignored.

### 5.2 Save (:6759-6842) / Load (:6854-6938); `fTime = GetTime()` both sides

| # | Field | Type | HOBJECT | Time handling / notes |
|---|---|---|---|---|
| 1 | `m_hTarget` | LSO | ✓ | link from `SetNewTarget` (:1295-1311) **not re-created by game code** on load |
| 2 | `m_vTargetPos` | V | | |
| 3 | `m_hTrackObject` | LSO | ✓ | no link; SmellHint targets are `CF_ALWAYSLOAD` → NULL after restore |
| 4 | `m_vTrackObjPos` | V | | |
| 5 | `m_vGuardPos` | V | | |
| 6 | `m_vDestPos` | V | | |
| 7 | `m_nState` | DW | | |
| 8 | `m_nLastState` | DW | | |
| 9 | `m_fStimuli[0..5]` (NUM_STIMULI=6) | F×6 | | |
| 10 | `m_nDodgeFlags` | DW | | |
| 11-13 | `m_hstrSpotTriggerTarget`, `m_hstrSpotTriggerMessage`, `m_hstrTriggerRelayTarget` | HS×3 | | |
| 14-17 | `m_fHearingDist`, **`m_fSmellingDist`, `m_fSensingDist`**, `m_fSeeingDist` | F×4 | | order Hearing, Smelling, Sensing, Seeing (matches Load) |
| 18-21 | `m_fWalkSpeed`, `m_fRunSpeed`, `m_fJumpSpeed`, `m_fRollSpeed` | F×4 | | |
| 22 | `m_fTimeStart - fTime` | F | | **relative**; Load `+ fTime` ✓ (0 sentinel survives under RESTORETIME) |
| 23 | `m_fLoadTimeStart - fTime` | F | | **relative** ✓ |
| 24 | `m_nCorpseType` | DW | | |
| 25 | `m_fAttackLoadTime` | F | | duration (weapon reload time) |
| 26 | `m_bAnimating` | B | | |
| 27 | `m_bJumping` | B | | |
| 28 | `Metacmd` | DW | | |
| 29 | `m_nCurMetacmd` | DW | | |
| 30 | `m_dwFlags` | DW | | |
| 31 | `m_nBlockFlags` | DW | | |
| 32 | `m_bSetShutdown` | B | | |
| 33 | `m_fLastUpdate` | F **raw absolute** | | **BUG: Load adds `+ fTime`** (:6912) → ≈2×time under RESTORETIME. Effect: first `AI_Update` desync check `GetTime()-m_fLastUpdate >= 1` (:2154) is false once, then reassigned — benign |
| 34 | `m_nInjuredLeg` | DW | | |
| 35 | `m_hEnemyAttach` | LSO | ✓ | no link; Thief/Leech/Hand re-point it to `g_pPlayerObj` on load |
| 36 | `m_vScale` | V | | engine scale is engine-saved; field kept for later use |
| 37 | `m_fLastLedgeCheck` | F **raw absolute** | | **BUG: Load adds `+ fTime`** (:6920). `IsLedge` returns FALSE while `GetTime()-m_fLastLedgeCheck < 0.05` and does not refresh it (:3665-3668) ⇒ **ledge detection disabled for ≈ (saved game time) seconds after a full restore** |
| 38 | `m_nCurAnimation` | DW | | |
| 39 | `m_vDims` | V | | |
| 40 | `m_bStartFire` | B | | |
| 41 | `m_nScriptCmdIndex` | **F** | | int stored as float (exact up to 2^24) |
| 42 | `m_fScriptWaitEnd` | F **raw absolute** | | read raw ✓ valid only with RESTORETIME |
| 43 | `m_dwScriptFlags` | DW | | |
| 44 | `m_eScriptMovement` | B | | |
| 45 | `m_bUpdateScriptCmd` | B | | |
| 46 | `m_curScriptCmd.Save` | B command + HS args | | `AIScriptList.h:52-82`; empty args written as NULL HSTRING; Load keeps previous args when NULL |
| 47 | `m_scriptCmdList.Save` | F count + N×(B+HS) | | `AIScriptList.h:211-241`; count = `m_nNumItems` but NULL entries skipped (latent desync); Load appends |
| 48 | `m_AIPathList.Save` | F count, B `m_bDeleteElements`, N×PathListData | | `PathList.h:167-201`; PathListData = V pos, HS name, HS actionTarget, HS actionMessage (`PathListData.cpp:85-115`). Load restores `m_bDeleteElements=FALSE` (ctor `Init(DFALSE)`) and allocates new PathListData → **leaked** (never deleted) |

### 5.3 AI_Mgr state NOT saved

| Field | Fate |
|---|---|
| `m_pAnim_Sound` | re-set by subclass `InitStatics` during INITIALUPDATE (and by Cultist/Civilian Load) |
| `m_bCabal`, `m_Link` | ctor constant; `InsertMe` on load |
| `m_pServerDE` | re-set in each subclass `InitialUpdate` |
| `m_hCurSound` (HSOUNDDE) | lost; NULL |
| **`m_pCurSmell`, `m_hCurSmell`, `m_vSmellPos`** | lost → SearchSmellTarget restarts; link to SmellHint (:6004) not restorable (SmellHint is `CF_ALWAYSLOAD`) |
| `m_hFireSource` (burning-death BaseClass object, no link) | lost (object is not saveable either) |
| `m_bProceedAttach` | FALSE → attachers wait for client ack again |
| **`m_fWaitForIdleTime`** (absolute), **`m_bHasTargeted`** | reset to 0/FALSE → the 120 s "don't go idle" guard (:1919-1933) is dropped |
| `m_bMoveToGround`, `m_bRemoveMe` | FALSE |
| `m_fAIMass/HitPoints/RandomHP/ArmorPoints`, `m_szAIState`, `m_szAIWeapon[5]`, `m_fAI*` ammo, `m_fProxBombs`, `m_nAIStrength` | init-only properties; their effects live in CDestructable/CInventoryMgr saves |
| `AIShared` (`m_bTurnR/L`, `m_vLastStrafePos`, `m_bStrafing`, `m_bJumping`; `AI_Shared.h:85-91`) | ctor defaults |
| `m_fLastFrame` (static) | n/a |

---------------------------------------------------------------------------------------------------

## 6. CultistAI (`CultistAI.cpp`)

* PRECREATE (:96-115): base first; props `Male/ClownSkin/RobeSkin` only when `fData == 1.0`; `PostPropRead` skipped for SAVEGAME. INITIALUPDATE (:117-122): `InitialUpdate` + `CacheFiles` on **all** paths, then AI_Mgr. SAVE/LOAD (:124-130) before AI_Mgr. dwParam ignored.
* Save :589-592 / Load :594-647:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_bMale` | B | Load (`_ADDON`, defined together with `_ADD_ON` in `ObjectDLL.dsp:165,192`) derives `m_bClownSkin` (skin M_CULTIST4/5/6) and `m_bRobeSkin` (OLDSCHOOL) from engine model filenames; then sets anim statics and `InitStatics` |

* Not saved: `m_bClownSkin`, `m_bRobeSkin` (derived on load).
* **Original bug (restore-order dependent)**: during INITIALUPDATE_SAVEGAME `m_bMale` is still the ctor default TRUE (Load not yet run), so `InitialUpdate` (:216-251) (a) sets `m_Male_Anim_Sound` indexes/hit spheres **from this object's (possibly female) model** if `m_bMaleAnims` is still TRUE (it is reset by `ResetStatics` each LoadWorld), poisoning the shared male table for later male cultists; (b) `AI_Mgr::InitialUpdate` calls `m_damage.Init(..., m_pAnim_Sound=&male)` — `CDestructable::m_pAnim_Sound` is **never corrected** by Load, so a restored female cultist uses male hit-sphere radii in `CalculateHitLimb` (`Destructable.cpp:776,830`).
* `AddDamageMultiplier(0.5)` in INITIALUPDATE_SAVEGAME is overwritten by the inventory Load.

## 7. CivilianAI (`CivilianAI.cpp`)

* Dispatch identical to Cultist (:95-138; props only for fData==1.0).
* Save :657-664 / Load :666-699:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | packed `m_bMale \| m_bLabTech<<1 \| m_bScared<<2` | B | Load unpacks, re-runs anim static setup + `InitStatics` |

* Not saved: `m_bSororitySkin` (model choice only; skin is engine-saved).
* Same INITIALUPDATE_SAVEGAME/`m_damage.m_pAnim_Sound` issue as Cultist. Precedence quirk `m_bMale || m_bLabTech && m_bMaleAnims` (:307, :677) → every male (and default-male at INITIALUPDATE_SAVEGAME) re-writes the shared male table from its own model; there is no `ResetStatics` for civilians.

## 8. Fanatic (`Fanatic.cpp`)

* PRECREATE (:108-120, PostPropRead skipped on SAVEGAME); INITIALUPDATE `InitialUpdate`+`CacheFiles` (:122-127); MID_UPDATE drives smoke SFX/attachment (:129-165); SAVE/LOAD :167-173.
* Save :743-749 / Load :751-779:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_fStartTime` | F **raw absolute** | kamikaze fuse start (set :609; 30 s fuse :634); also used as a boolean. Valid only with RESTORETIME. Load: if non-zero, **recreates** smoke-source BaseClass object at node `torso_extra1` and sets `m_bStartSmoke = TRUE` (SFX + attachment re-sent next update) |

* Not saved: `m_hAttach`, `m_hSmokeSource` (recreated), `m_bStartSmoke`; hidden node `torso_extra1` (`SetModelNodeHideStatus` :605) is not restored by game code (engine-dependent; later engine does not serialize node hide state).

## 9. Naga (`Naga.cpp`)

* PRECREATE :89-101; INITIALUPDATE (:103-121): `InitialUpdate` → explicit `AI_Mgr::EngineMessageFn` → (non-SAVEGAME only) clear SHADOW/SOLID → `CacheFiles` → **return** (no double base call). MID_UPDATE boss-health (:123-148). SAVE/LOAD :150-156.
* `Naga::InitialUpdate` (:216-262) on **every** path creates a new spike `Model`, attaches it (`m_hSpikeAttach`), sets alpha 0, `m_bCreateHealth = TRUE`.
* Save :514-527 / Load :529-541:

| # | Field | Save type | Load type | Notes |
|---|---|---|---|---|
| 1 | `m_iCeilingWhomps` | **DW** | **F** | **type mismatch**: int bits read as float, cast to int → small counts become 0 (denormal). Same size, no desync |
| 2 | `m_hSpike` | **MO** | ReadFromMessageObject | **invalid in save**; also the spike is an engine `Model` (`CF_ALWAYSLOAD`, unsaveable). Load overwrites the freshly created spike handle with a meaningless handle → the new spike stays attached but orphaned; later hide/show (:326-336, :467-469) target the wrong/NULL object |
| 3 | `m_fFullHealth` | F | F | |
| 4 | object alpha (`GetObjectColor`) | F | F | Load re-applies with current RGB |

* Not saved: `m_hSpikeAttach` (recreated), `m_bTurning`, `m_fRadsLeft`, `m_fRadsTurned`, `m_bCreateHealth` (re-armed TRUE).

## 10. Gideon (`Gideon.cpp`)

* PRECREATE :94-106; INITIALUPDATE `InitialUpdate`+`CacheFiles` (:108-113); MID_UPDATE boss bar; SAVE/LOAD :138-144; after base call, every INITIALUPDATE clears `FLAG_GRAVITY` (:149-161).
* `InitialUpdate` (:221-241) on every path: anim statics, `m_fFullHealth = m_damage.GetHitPoints()` (1.0 default at SAVEGAME time — overwritten by Load), `m_bCreateHealth = TRUE`.
* Save :811-820 / Load :822-831:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_fShieldDuration` | F | constant 60 |
| 2 | `m_fLastShield` | F **raw absolute** | valid only with RESTORETIME |
| 3 | `m_fFullHealth` | F | |
| 4 | `m_fLastPanic` | F **raw absolute** | idem |

* Not saved: `m_szFireNode[64]` (set by model key `extra`, :778) → reset to ctor `"rr_gun"`; `m_bCreateHealth` (re-armed).

## 11. EnhancedGideon (`EnhancedGideon.cpp`)

* Same dispatch shape as Gideon (:96-166, gravity strip after base). `InitialUpdate` (:218-236) does **not** set `m_fFullHealth` (only `m_bCreateHealth = TRUE`).
* Save :835-844 / Load :846-855 — identical 4 floats: `m_fShieldDuration`, `m_fLastShield` (abs), `m_fFullHealth`, `m_fLastPanic` (abs).
* Not saved: `m_szFireNode` (→ "rr_gun"), `m_bNoFire` (re-asserted every `AI_STATE_Script` tick :1007, cleared at script DONE :1050), `m_bCreateHealth`.

## 12. UndeadGideon (`UndeadGideon.cpp`)

* Dispatch :81-133 (no gravity strip). Save :524-527 / Load :529-532:

| # | Field | Type |
|---|---|---|
| 1 | `m_fFullHealth` | F |

* Not saved: `m_bCreateHealth` (ctor TRUE), `m_bClimbing` (ctor only, unused).

## 13. AncientOne (`AncientOne.cpp`)

* Dispatch :90-141. `InitialUpdate` (:199-245) on every path sets `m_vScale = 5`, `m_bMoveToGround = TRUE` (then AI_Mgr's INITIALUPDATE sets it FALSE for non-worldfile), statics.
* Save :418-425 / Load :427-434: `m_fLastFire` F (raw; field otherwise unused), `m_fFullHealth` F.
* Not saved: `m_bCreateHealth` (ctor TRUE). (`m_hLoopSound` code is commented out.)

## 14. AncientOneTentacle (`AncientOneTentacle.cpp`)

* Dispatch :87-116. Save :569-578 / Load :580-589:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_fTimeOfDeath` | F **raw absolute** | **uninitialized in ctor** (:34-70) until death (:564) → garbage when alive |
| 2 | `m_fLastIdle` | F raw absolute | |
| 3 | `m_fScaleBy` | F | current hide/emerge scale |
| 4 | `m_bHiding` | B | |

## 15. DrudgePriest (`drudgepriest.cpp`)

* INITIALUPDATE (:99-112): `InitialUpdate` → explicit AI_Mgr call → clear GRAVITY → CacheFiles → return. `InitialUpdate` (:156-191) on every path **restarts the looping ambient sound** (`m_hLoopSound`, PLAYSOUND_GETHANDLE).
* Save :466-469 / Load :471-474: `m_fLastSpawn` F (raw absolute; 30 s / 1.5 s spawn gates :205-252).
* Not saved: `m_hLoopSound` (regenerated).

## 16. Thief / BoneLeech / TheHandAI (`thief.cpp`, `boneleech.cpp`, `TheHandAI.cpp`)

* Each declares its **own `m_bJumping`**, shadowing `AI_Mgr::m_bJumping`.
* MID_LOADOBJECT (thief :122-139, leech :133-150, hand :124-141): `Load` → explicit `AI_Mgr::EngineMessageFn` (AI_Mgr Load + InsertMe + aggregates) → if `m_hEnemyAttach`: **`m_hEnemyAttach = g_pPlayerObj->m_hObject`** (global, set in `CPlayerObj` ctor `PlayerObj.cpp:387`; assumes the single player), `SetNewState(STATE_EnemyAttach)`, `FLAG_REALLYCLOSE`, clear GRAVITY. The attach handshake (`SMSG_THIEF_ATTACH`, client ack → `m_bProceedAttach`) is replayed by `AI_STATE_EnemyAttach` Metacmd 1 (thief :371-386).
* Save/Load (thief :482-494, leech :495-507, hand :483-495):

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_bJumping` (subclass) | B | |
| 2 | `m_fLastDmgTime` | F raw absolute | 1 s damage cadence |
| 3 | `m_fLastDetachTime` | F raw absolute | 4 s re-attach gate |

## 17. ZealotAI (`ZealotAI.cpp`)

* PRECREATE: `Divine` only for fData==1.0 (:96-106). `InitialUpdate` (:165-189) on every path: if `m_bDivine` (FALSE at SAVEGAME time) doubles HP props and adds 1.5 damage multiplier.
* Save :863-874 / Load :876-890:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | packed `m_bFloat \| m_bStartStaffEffect<<1 \| m_bDivine<<2` | B | |
| 2 | `m_fMaxHeight` | F | |
| — | `m_hStaffEffect` | (commented out) | |

* **Bug**: `m_hStaffEffect`/`m_hAttach` not saved (the BaseClass staff object is unsaveable). If saved with `m_bStartStaffEffect` TRUE, the next tick sends an ObjectFX SFX for a NULL object and `CreateAttachment(..., NULL, ...)` (:506-530); if saved mid-cast, PLAYDONE cleanup calls `RemoveAttachment/RemoveObject` on NULL (:533-540). Floating (`m_bFloat`) relies on the engine-saved gravity flag.

---------------------------------------------------------------------------------------------------

## 18. CCorpse (`corpse.cpp/.h`) — B2BaseClass + own `m_damage` aggregate

* PRECREATE (:122-134): base, then `ReadProp` (WORLDFILE/STRINGPROP), then **`PostPropRead` unconditionally** → on SAVEGAME overwrites engine-restored `m_Flags` with `VISIBLE|RAYHIT|GRAVITY|MODELKEYS|REMOVEIFOUTSIDE` (:241-247) and copies `m_UserData` (NULL).
* INITIALUPDATE (:136-147): `InitialUpdate` **skipped** for SAVEGAME → no `m_damage.Init` (CDestructable gets `m_hObject` from its own Load "hack" path and `m_pInventoryMgr/m_pAnim_Sound` stay NULL), no `GenerateHitSpheres`, no `SetNextUpdate` (relies on engine NextUpdate).
* Save :703-716 / Load :727-741 (then `[CDestructable]`):

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_fHitSpheres[NUM_STD_NODES]` | F×N | |
| 2 | `m_fMass` | F | |
| 3 | `m_fHitPoints` | F | (live HP is in CDestructable) |
| 4 | `m_szSoundDir` | **S** | Load `_mbscpy` into 256 buffer (no bound check) |
| 5 | `m_bLimbLoss` | B | |

* Not saved: `m_bDead`; **`m_hFireSource`, `m_fBurnStart`, `m_hSmokeSource`, `m_fSmokeStart`, `m_hAttach`** (burning/smoking corpse loses its effects; BaseClass FX objects unsaveable); `m_fRemoveTime` (multiplayer removal deadline, absolute); `m_bSetShutdown`; `m_hCurSound`, `m_hLoopSound` (fire loop). Hidden (dismembered) nodes not restored by game code. Destructor kills `m_hCurSound` when `m_hLoopSound` is set (handle mix-up, `corpse.h:45-51`). `m_pServerDE` never assigned.

## 19. Tchernotronic (`Tchernotronic.cpp/.h`) — B2BaseClass, no aggregates

* PRECREATE: `ReadProp` **unconditional** (:113-117; no props on SAVEGAME, strings stay NULL). INITIALUPDATE: skipped for SAVEGAME (:118-124) → flags/anim indexes/dims/blocking priority/NextUpdate from engine + Load.
* Save :434-446 / Load :457-480:

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | `m_hstrTriggerTarget` | HS | Load frees previous first ✓ |
| 2 | `m_hstrTriggerMessage` | HS | idem |
| 3 | `m_nRestAnim` | DW | |
| 4 | `m_nMovingAnim` | DW | |
| 5 | `m_bTimer` | B | |
| 6 | `m_fRemoveTime - fTime` | F | **relative** ✓ (+fTime on load) |

## 20. SeeingEye (`SeeingEye.cpp/.h`) — BaseClass + `m_damage` aggregate

* PRECREATE: `PostPropRead` unless `fData != INITIALUPDATE_SAVEGAME` (wrong constant name, same value 3.0) (:113-121). INITIALUPDATE skipped for SAVEGAME (no velocity/friction/user-flag reset, no `m_damage.Init`).
* MID_LINKBROKEN: stuck object → `Pickup()`; owner → `Remove(TRUE)` (:133-145).
* Save :488-496 / Load :506-516 (then `[CDestructable]`):

| # | Field | Type | HOBJECT | Notes |
|---|---|---|---|---|
| 1 | `m_hOwner` | LSO | ✓ | Load **explicitly re-creates** `CreateInterObjectLink(this, owner)` |
| 2 | `m_hStuckObject` | LSO | ✓ | if non-NULL Load calls `Attach()` → re-link + sends `MID_ATTACH` to the stuck object (player re-adds it to `m_AttachedObjectsList`, `PlayerObj.cpp:4423-4435`) |
| 3 | `m_bDropped` | B | | |

* Not saved: `m_hClient` (set only via TheEye Load → NULL at restore, so `Remove()` won't send un-eye/blind messages), `m_fNextPingTime` (→ 0, immediate ping).

---------------------------------------------------------------------------------------------------

## 21. CPlayerObj (`PlayerObj.cpp/.h`)

### 21.1 Dispatch / lifecycle

* PRECREATE (:4144-4150): `CBaseCharacter` first, then **`PostPropRead` unconditionally** → on restore forces filename `Caleb.abc`/`Caleb1.dtx`, name `"player"`, `OT_MODEL` (:574-580); corrected by `SetCharacter(m_nCharacter)` in Load.
* INITIALUPDATE (:4152-4153 → :590-711): always: `playerpos` var, net flags, `m_InventoryMgr.Init(m_hObject, NULL)`, `m_damage.Init(...)`, `UpdateTeamID`, insert into AI Monster+Cabal lists, `m_bFirstUpdate = TRUE`, zero speed vars, `m_bDoorPush=FALSE`, `m_fTeleportTime=0`, LeashLen track; **returns for SAVEGAME** (:641-644) — no HP/armor defaults, no SmellHint, no `m_hstrTrigger`, no `SetNextUpdate`, no dims.
* SAVE/LOAD (:4226-4232) before `CBaseCharacter` (→ `[CMovement(NULL obj)][CInventoryMgr][CDestructable]`).
* dwParam: unused by CPlayerObj (`Load` comment "Flags set means keep alive restore" but the related `GoToStartPoint` is commented out :6870-6872). `_ADDON` level-change character switch: `Save` first calls `TweakSaveValuesForCharacterChange` (:6549-6555, :7542-7608) which mutates the live player (character, attributes, `ValidateAttributes`, `ClearPowerupValues`, `m_damage.SetStartingCharacterValues`) — the following CDestructable aggregate save therefore stores the topped-up values.

### 21.2 Save (:6541-6683) / Load (:6694-6873); `fTime = GetTime()`

| # | Field | Type | Time / notes |
|---|---|---|---|
| — | `m_bFirstUpdate` | (commented out) | INITIALUPDATE sets TRUE → first Update re-sends physics/stats |
| 1 | `m_bDemoPlayback` | B | |
| 2 | `m_dwControlFlags` | DW | |
| 3 | `m_bFiringWeapon` | B | |
| 4 | `m_bFalling` | B | |
| 5 | `m_bServerFalling` | B | |
| 6 | `m_startFall - fTime` | F | relative; **`-1` "not falling" sentinel (:315,:1114) passes through the conversion** (stays ≈-1 under RESTORETIME; becomes `-1 - t_old + ~0` on keep-alive, still negative) |
| 7 | `m_startIdle - fTime` | F | relative |
| 8 | `m_fMinFallPos` | F | |
| 9 | `m_fMaxFallPos` | F | |
| — | `m_nLastHitPoints/Damage/Ammo/Armor/Focus` | not saved | Load zeroes them to force client refresh |
| — | `m_fSlowDeathSafeTimer/StayTimer` | not saved | Load zeroes |
| 10 | `m_fBindingRegenTime` | F | countdown (:3772-3775) — time-base independent |
| 11 | `m_fServerRegenTime` | F | countdown (:3761-3764) |
| 12 | `m_bSpectatorMode` | B | |
| 13 | `m_fSmellTime - fTime` | F | relative |
| 14 | `m_fSmellDelay` | F | |
| 15 | `m_bDead` | B | |
| 16 | `m_bMovementBlocked` | B | |
| 17-21 | `m_bPowerupActivated[INVULNERABLE, INVISIBLE, TRIPLEDAMAGE, INCORPOREAL, SILENT]` | B×5 | other PU slots (e.g. ANGER/WILLPOWER glows) not saved |
| 22-26 | `m_fNighInvulnerableTime`, `m_fInvisibleTime`, `m_fTripleDamageTime`, `m_fIncorporealTime`, `m_fSilentTime` (each `- fTime`) | F×5 | relative absolute-deadlines ✓ |
| 27 | `m_nPowerupSoundStackPos` | B | |
| 28.. | `m_ePowerupSoundStack[0..pos-1]` | B×pos | sound handles themselves lost |
| 29 | `m_hstrTrigger` | HS | not created by INITIALUPDATE_SAVEGAME, so no leak |
| 30 | `m_vLastPos` | V | |
| 31 | `m_vLastVel` | V | |
| 32 | `m_vScale` | V | |
| 33 | `m_fPitch` | F | |
| 34 | `m_fYaw` | F | |
| 35 | `m_rRotation` | R | view rotation |
| 36 | `m_vForward` | V | |
| 37 | `m_vGunMuzzlePos` | V | |
| 38 | `m_vlGunMuzzlePos` | V | |
| 39 | `m_hstrPlayerName` | HS | |
| 40 | `m_byFlags` | B | |
| 41 | `m_fMouseAxis0` | F | |
| 42 | `m_fMouseAxis1` | F | |
| 43 | `m_bZoomView` | B | Load then forces FALSE |
| 44 | `m_nZoomType` | B | Load then forces 0 |
| 45 | `m_hCameraObj` | **MO** (plain) | invalid-in-save API; field effectively unused (crane code commented out :4094-4125) |
| — | `m_CurGun`, `m_CurGunSlot`, `m_fEyeLevel` | (commented out) | |
| 46 | `m_bLastCrouchCommand` | B | |
| 47 | `m_bLastJumpCommand` | B | |
| 48 | `m_bForcedCrouch` | B | |
| 49 | `m_Frags` | DW | |
| 50 | `m_MeleeKills` | DW | |
| 51 | `m_nCharacter` | B | |
| 52 | `m_nSkin` | B | |
| 53-56 | `m_nAttribStrength/Speed/Resistance/Magic` | B×4 | raw, no `ValidateAttributes` on load |
| 57-60 | `m_nBindingStrength/Speed/Resistance/Magic` | B×4 | |
| 61-68 | `m_bBindingConstitution`, `m_bBindingBlending`, `m_bBindingSoulStealing`, `m_bBindingRegeneration`, `m_bServerRegeneration`, `m_bBindingQuickness`, `m_bBindingIncreasedDamage`, `m_bBindingImpactResistance` | B×8 | `m_fBindingRegenPeriod`/`m_fServerRegenPeriod` **not saved** |
| 69 | `m_byMoveState` | B | |
| 70-72 | `m_nInjuredLeg`, `m_nNodeHit`, `m_nSideHit` | **W of `(DBYTE)` cast** | truncated to 8 bits, stored in 16; read as W (consistent size) |
| 73 | `m_fDeathTimer` | F | countdown (:6035) |
| 74 | `m_bInSlowDeath` | B | |
| 75 | `m_fOldAirLevel` | F | |
| 76 | `m_fAirLevel` | F | |
| 77 | `m_bSwimmingJump` | B | |
| 78 | `m_bCanSwimJump` | B | |
| 79 | `m_bImprisoned` | B | |
| 80 | `m_fImprisonStart - fTime` | F | relative |
| 81 | `m_fImprisonLength` | F | duration |
| 82 | `m_bBurning` | B | |
| 83 | `m_fBurningTime` | F | raw; field otherwise unused |
| 84 | `m_hClientSaveData != NULL` | B | |
| 85 | client save blob | HMR | written then `EndHMessageRead` and cleared (one-shot; set by CMSG_SAVEGAME `:408-410`; absent in keep-alive). Load re-broadcasts it to **all** clients as `SMSG_CLIENTSAVEDATA` |

Load epilogue: `SetCharacter(m_nCharacter)` (model filenames from character/skin tables, `SetAnimationIndexes`, `GenerateHitSpheres`, sound root; :2360-2379), `AIShared.ResetNodes` (unhide all nodes), **flags overwritten** with `VISIBLE|MODELKEYS|YROTATION|ANIMTRANSITION|TOUCH_NOTIFY|SOLID|STAIRSTEP` (no GRAVITY/SHADOW; also applied when `m_bDead`), velocity zeroed, zoom reset (:6841-6858).

### 21.3 CPlayerObj state NOT saved

`m_hClient` (re-bound by client ref → `SetClient`), `m_bFirstUpdate`(TRUE), speed/accel/jump vars and multipliers (zeroed in INITIALUPDATE, recomputed in first Update :1584-1600), `m_fLeashLen`, `m_fTeleportTime`, `m_bDoorPush/m_vDoorPush`, `m_bSlowMode`/`m_fSlowTime` (absolute), `m_hOrbObj` (Orb camera), **`m_pBombList`** (remote bombs in the world can no longer be detonated by this player), `m_AttachedObjectsList` (rebuilt by SeeingEye::Load MID_ATTACH), `m_hEnemyAttach`, `m_hFlagObject/m_hFlagAttach`, `m_dwTeamID` (UpdateTeamID), `m_Music`, `m_bActivated`, `m_hCurSound`, `m_hWeapSound`, `m_hPowerup{Start,End,Loop}Sound`, `m_hstrWhoKilledMeLast`, `m_hstrWhoIKilledLast`, `m_eLastSurface`, footstep flags, `m_fIdleTime`, `m_fNextPingTime`, `m_CurContainers[]`, `m_PStateChangeFlags` (set by SetCharacter/first update), `m_byClientMoveCode`, `m_bOnGround/m_bLastOnGround`, `m_byAirborneCount`, `m_bInputMode`, `m_bExternalCamera`, `m_nStatsFlags`, `m_hPlrPosVar`, `m_CabalLink/m_MonsterLink` (re-inserted in INITIALUPDATE).

---------------------------------------------------------------------------------------------------

## 22. NPC classes with no Save/Load of their own

All derive directly from `AI_Mgr`; they persist only the AI_Mgr + aggregate data. Those that are **not compiled** into the shipped ObjectDLL (absent from `ObjectDLL.dsp`): BirdAI, RatAI, BugAI (`bug.cpp`), WurmAI, DefenseGlobeAI/AI_DefenseGlobe, DefenseGlobeLegs.

| Class (compiled) | Extra instance members | Lost on restore / notes |
|---|---|---|
| ShikariAI, SoulDrudge, MadScientistAI, Behemoth, Prophet, DrudgeLord, GremlinAI, CalebAI, GabriellaAI, GabriellaREV, IshmaelAI, IshmaelREV, OpheliaAI, OpheliaREV | none (only statics `m_bLoadAnims`, `m_Anim_Sound`) | nothing class-specific; all AI_Mgr §5.3 losses apply. Caleb/Gabriella/Ishmael/Ophelia(+REV) overwrite filename/skin in every PRECREATE (same values, harmless). Prophet's INITIALUPDATE calls base explicitly then `ObtainWeapon(WEAP_MELEE)` and returns (rebuilt anyway by inventory Load) |
| DeathShroud | `HSOUNDDE m_hLoopSound` | handle lost; `InitialUpdate` (all paths) restarts the loop (:161-175). INITIALUPDATE sets `m_vScale=1.5` after base (overwritten by AI_Mgr Load) |
| Nightmare (`nightmare.cpp`, add-on) | `m_fFullHealth`, `m_bCreateHealth`, `m_bShockwave` | **`m_fFullHealth` lost → 0**, `m_bCreateHealth` → TRUE ⇒ first update takes the *current (damaged)* HP as full health: boss bar resets to 100% (:116-130). `m_bShockwave` ctor-only/unused |
| (not compiled) WurmAI | `m_bFlying` | would be lost |
| (not compiled) DefenseGlobeAI : CBaseCharacter | `AI_DefenseGlobe m_ai` used as an aggregate (`AddAggregate(&m_ai)`) | AI_Mgr Save/Load would run as an aggregate — dead code |

---------------------------------------------------------------------------------------------------

## 23. Cross-cutting findings

### 23.1 Absolute time handling

| Encoding | Fields |
|---|---|
| Relative (`x - GetTime()` / `+ GetTime()`) ✓ | AI_Mgr `m_fTimeStart`, `m_fLoadTimeStart`; Tchernotronic `m_fRemoveTime`; Player `m_startFall`, `m_startIdle`, `m_fSmellTime`, 5 powerup deadlines, `m_fImprisonStart` |
| Durations / countdowns (time-base independent) ✓ | CDestructable `m_fDeathDelay`; Flashlight/Goggles `m_fDischargeTime`; Player regen timers, `m_fDeathTimer`, `m_fImprisonLength`; AI `m_fAttackLoadTime` |
| Raw absolute, read raw (valid only because full restore uses RESTORETIME) | AI_Mgr `m_fScriptWaitEnd`; Fanatic `m_fStartTime`; Gideon/EnhancedGideon `m_fLastShield`, `m_fLastPanic`; AncientOne `m_fLastFire`; Tentacle `m_fTimeOfDeath`, `m_fLastIdle`; DrudgePriest `m_fLastSpawn`; Thief/Leech/Hand `m_fLastDmgTime`, `m_fLastDetachTime` |
| **Raw absolute, read with `+fTime` (bug)** | AI_Mgr **`m_fLastUpdate`** (benign) and **`m_fLastLedgeCheck`** (ledge checks suppressed for ≈ saved-game-time seconds) |
| Absolute, not saved | AI `m_fWaitForIdleTime`; Corpse `m_fBurnStart`, `m_fSmokeStart`, `m_fRemoveTime`; Inventory `m_fLastDroppedTime`; Player `m_fSlowTime`, `m_fTeleportTime`; SeeingEye `m_fNextPingTime`; all CWeapon timers |

Clock: full restore restores the saved game time (clock continues); keep-alive restore does not (later engine resets to 0 at LoadWorld). AIs never travel in keep-alive saves, so the raw-absolute AI fields only ever meet RESTORETIME restores; the player's keep-alive data is relative/duration-encoded except unused `m_fBurningTime`.

### 23.2 Links and references

* Game code re-creates links on load only in: **SeeingEye** (owner + stuck object), **CInventoryMgr** (new muzzle flash), **Thief/Leech/Hand** (re-point `m_hEnemyAttach` to `g_pPlayerObj`, no link). AI_Mgr `m_hTarget` (linked at runtime), CDestructable `m_hLastDamager` (linked, not saved), AI `m_hCurSmell` (linked, not saved) rely on engine interlink serialization (present in later engine `sm_SaveInterlinks/sm_RestoreInterlinks`; unverified for 1998). Calling `CreateInterObjectLink` again is idempotent (`server_de.h:603-606`).
* References that are saved but resolve to NULL after restore: anything pointing at `CF_ALWAYSLOAD` objects (SmellHint, PathPoint, ExitHint, engine `Model` — e.g. Naga spike) or non-saveable BaseClass FX/light objects; in keep-alive saves every non-player reference (TheEye's SeeingEye).
* Plain `WriteToMessageObject` misuse in a save: Naga `m_hSpike`, CPlayerObj `m_hCameraObj`.

### 23.3 Known original bugs / asymmetries to preserve or document

1. AI_Mgr `m_fLastUpdate`, `m_fLastLedgeCheck`: saved absolute, loaded `+fTime` (AI_Mgr.cpp:6817/6912, 6825/6920).
2. Naga `m_iCeilingWhomps` saved DWord, loaded Float (Naga.cpp:522/534); `m_hSpike` via plain object API + new spike created at INITIALUPDATE_SAVEGAME then orphaned.
3. Cultist/Civilian INITIALUPDATE_SAVEGAME runs with ctor-default `m_bMale=TRUE`: may poison the shared male anim/hit-sphere table from a female model, and leaves `CDestructable::m_pAnim_Sound` on the male table permanently.
4. CDestructable `m_nLastDamageLocation`, CInventoryMgr `m_WeapCount`/`m_fMeleeMultiplier`, CInvItem `m_bDropped`, Tentacle `m_fTimeOfDeath`: uninitialized values serialized.
5. CDestructable non-destructable objects save HP as 1e6 sentinel; load infers `m_bDestructable` from HP ≥ 1e6.
6. CInventoryMgr Load: `AddInventoryWeapon(type, slotIndex)` passes the slot index as count; only slot-0 weapon is drawn/muzzle-flash-initialized; Init-then-Load HSTRING leaks; count-vs-NULL-entry latent desync (also CAIScriptList, PathList).
7. CInvItem::Load forces `m_bIsActive = FALSE` on every load (flashlight/goggles/eye deactivate even on a normal save restore); flashlight keep-alive deactivation code is dead.
8. Keys are discarded only on keep-alive loads (dwParam 1) — the only dwParam consumer.
9. Zealot staff-effect handle not saved while its start/cleanup flags are → NULL-object SFX/attachment calls.
10. CCorpse PRECREATE_SAVEGAME overwrites engine-restored object flags; burning/smoking/removal timers lost; corpse damage aggregate never `Init`-ed after restore (no inventory/anim pointers).
11. CPlayerObj Load hard-sets object flags (drops GRAVITY/SHADOW, sets SOLID even if dead), zeroes velocity, disables zoom; bomb list, orb, slow-mode lost.
12. CBaseCharacter `m_nTrapped` not saved (trapped characters freed by restore).
13. AI_Mgr `m_fWaitForIdleTime`/`m_bHasTargeted` not saved (idle guard dropped); smell-tracking cursor lost.
14. Nightmare boss bar full-health reference lost.
15. Thief/Leech/Hand restore the attachment to `g_pPlayerObj` regardless of the saved target (single-player assumption).

### 23.4 State regenerated rather than saved (summary)

Sound handles (all `HSOUNDDE`; DrudgePriest/DeathShroud loops restart in INITIALUPDATE; others silent), client SFX helper objects (muzzle flash recreated; Fanatic smoke recreated from `m_fStartTime`; flashlight light recreated; Zealot/corpse/AI fire-smoke FX lost), weapons (recreated by type), anim indexes/hit spheres (class statics via `InitStatics`/`SetCharacter`), Cabal/Monster/Destructable lists (`InsertMe`, CDestructable Load, player INITIALUPDATE), boss health bars (`m_bCreateHealth` ctor/INITIALUPDATE TRUE → resent on first update), client HUD caches (player `m_nLast*` zeroed, inventory `m_pLast*` reset), client save blob (rebroadcast). Engine-owned: transform, flags/user flags, model/skin, anim trackers, color/alpha, dims, physics, NextUpdate, active state, attachments/interlinks (later-engine evidence).
