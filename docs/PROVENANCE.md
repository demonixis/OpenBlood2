# Source and Dependency Provenance

## Original fragment smoke trails

Selected CObjectParticleFX methods and CObjectGeneralFX's high-detail smoke-trail 1
recipe come from sources/blood2/ClientShellDLL/ObjectFX.{h,cpp}, reference commit
006b4aabd3fb348f6846cad03799cf8df685ea95. Typed IDs, owned particles/colors, bounded
failure and existing SDL_GPU billboards replace client APIs. The zero-count unused
division is omitted; other source timing/count/alpha quirks remain. See
[ledger](M2_OBJECT_PARTICLE_SOURCES.json), [contracts](OBJECT_PARTICLE_CONTRACTS.md)
and [validation](../../Documentation/Implementation/M2_OBJECT_PARTICLES.md).

## Character node visibility and player Load model setup

Selected AI_Shared named/extra reset loops, Destructable hide filtering and PlayerObj
Load/Respawn reset sites are connected through checked typed services. Engine mask,
sparse-ID triangle filtering and inspection are new code; any-hidden-vertex omission
is a replacement presentation rule, not retail parity. OpenBlood2's 64-bit storage
was consulted but not imported. Full player transfer remains unavailable. See
[ledger](M2_CHARACTER_NODES_SOURCES.json), [contracts](CHARACTER_NODE_CONTRACTS.md)
and [validation](../../Documentation/Implementation/M2_CHARACTER_NODES.md).

## Original player fields and restore initialization

Selected PlayerObj Save/Load values retain raw fields, relative smell time,
nullable strings and zeroed stat caches. The savegame InitialUpdate branch now
binds aggregates/factions without fresh grants. Typed ownership/validation and
staged string allocation are adaptations. Full Load/engine/client transfer remains
unavailable. See [source ledger](M2_PLAYER_STATE_SOURCES.json),
[contracts](PLAYER_STATE_CONTRACTS.md) and [validation](../../Documentation/Implementation/M2_PLAYER_STATE.md).

## Project Code

M0 started a new repository and build. No source file, project skeleton, build flags, compatibility table or gameplay implementation has been copied/cherry-picked from OpenBlood2, NoMadEngine or nomad. M1 imports the selected original Blood 2 code below directly from the preserved reference tree. The prior audit supplies behavioral requirements for the contract tests.

The SDL_GPU/ImGui integration follows the API sequencing documented by the [upstream SDL3/SDL_GPU example](https://github.com/ocornut/imgui/blob/c1aaac65e896a45f81f47a4d68f9959e0f86c859/examples/example_sdl3_sdlgpu3/main.cpp). The backends themselves are compiled directly from the pinned dependency. Project configuration, ownership, inspector panels, smoke checks and optional capture are new code.

Future imports should add one concise entry with donor repository/commit/path, purpose, local changes and validation. There are no prototype imports to record for M0.

## M1 Original Imports

Donor: `sources/blood2`, commit **`006b4aabd3fb348f6846cad03799cf8df685ea95`**. All reference files remain unchanged. Imported files retain their original comments; line endings are normalized to LF. [Original/source hashes](ORIGINAL_SOURCES.json) and the [adaptation patch](../../Documentation/Implementation/evidence/m1-original-adaptations.patch) record the selected inputs and edits. No original binary library or commercial resource is copied.

| Original path | New use and adaptations | Validation |
| --- | --- | --- |
| `ObjectDLL/B2BaseClass.{h,cpp}` | Compile original difficulty/game-type filters and parent forwarding. Remove unused shell include; use selected shared enums. Replace old registration macros with explicit typed registration/defaults in `game/registration.cpp`. Function bodies are unchanged. | Original world difficulty rejection and flag initialization |
| `ObjectDLL/CameraSpot.{h,cpp}` | Compile original precreate/initial-update behavior. Remove unused `PathListData.h`; replace registration; omit an unused parameter name. | Original 0.001 precreate delay, zero initial schedule, inherited dispatch |
| `ObjectDLL/Key.{h,cpp}` | Compile original property reads/string ownership. Correct `key.h` include casing to `Key.h`; replace registration; omit an unused parameter name. | Actual fields and strings, destruction, world-versus-string creation |
| `AppHeaders/cpp_engineobjects_de.h` | Single authoritative C++ `BaseClass`. Initialize aggregate/head/handle/internal pointers in the constructor; mark the retained internal field `maybe_unused`. Original dispatch methods retained. | Nested creation, actual virtual calls, aggregate forwarding, sanitizer tests |
| `AppHeaders/cpp_aggregate_de.h` | Retain original aggregate virtual methods and typed thunks. Initialize next pointer; omit names on unused default-handler parameters. | Synthetic aggregate through original callbacks; sanitizer tests |
| `AppHeaders/engineobjects.c` | Copy only `bc_AddAggregate`, `bc_EngineMessageFn`, `bc_ObjectMessageFn` as C++, with the explicit C++ base include. Exclude legacy registrations and unrelated built-in classes; forwarding/property logic is unchanged. | Precreate property transfer; no recursive object-message redispatch |
| `AppHeaders/engineobjects_de.h`, `cpp_server_de.h` | Retained declarations/backward-compatibility include, unchanged except line endings. Only the C++ branch is used. | GCC/Clang builds |
| `basedefs_de.h`, `basetypes_de.h`, `de_codes.h`, `serverobj_de.h`, `server_de.h`, `CSBase.h` | Curated declarations/constants for the supported source interface. Fixed-width scalar types, initialized structs, const input strings, opaque owned handles; array-bounded `SAFE_STRCPY`; real concrete server object. Original DLL/C layout is not supported. APIs outside the subset are absent, with selected deferred services explicitly returning error. | All object contracts; see [service matrix](OBJECT_CONTRACTS.md) |
| `Shared/SharedDefs.h`, `Shared/NetDefs.h`; two globals from `ObjectDLL/BloodServerShell.cpp` | Preserve difficulty/game-type numeric values and console variable names without pulling in the unimplemented shell. | Original filtering tests |

The old macro registration defaults are transcribed in one explicit table. `DifficultyInsane` is accepted as an optional property because original `ReadProp` requests it, although the original class declaration omits it. Property lookup is case-insensitive, covering the original `TypeBloodBath`/`TypeBloodbath` spelling difference. Retail lookup behavior remains to be compared in M2.

No gameplay fix was applied to `B2BaseClass`'s cooperative-mode `OPT_FLAG_CTF` check. `Key` still reads its custom properties only for world creation. The M1 layer does not replace either behavior with a guessed correction. Original `Rotating`, AI, weapons, player and session shells are not imported yet.

## Dependencies

Revisions were resolved on **15 October 2026**. [Dependencies.cmake](../cmake/Dependencies.cmake) is the authoritative manifest and includes SHA-256 checksums of the downloaded source archives.

| Dependency | Revision/source | Use | Upstream notice |
| --- | --- | --- | --- |
| SDL3 | `main` at `6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1` (reports 3.5.0) | Platform, GPU, PNG capture; includes upstream OpenXR support | [zlib notice](https://github.com/libsdl-org/SDL/blob/6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1/LICENSE.txt) |
| Dear ImGui | `master` at `c1aaac65e896a45f81f47a4d68f9959e0f86c859` (reports 1.93.0) | Optional development inspector and SDL3/SDL_GPU backends | [MIT notice](https://github.com/ocornut/imgui/blob/c1aaac65e896a45f81f47a4d68f9959e0f86c859/LICENSE.txt) |
| spdlog | v1.15.3 at `6fa36017cfd5731d617e1a934f0e5ea9c4445b13` | Logging with its bundled fmt configuration | [License and bundled-component notices](https://github.com/gabime/spdlog/blob/6fa36017cfd5731d617e1a934f0e5ea9c4445b13/LICENSE) |

Dependencies are fetched into ignored build/cache directories with their upstream notices. Retain those notices when distributing binaries. EnTT, Jolt, ImGuizmo, SDL_mixer and SDL_shadercross are not M0 dependencies. The ImGui backend carries its own shader assets; custom game shaders belong to later rendering work.

## M2 Imports and Format References

M2 remains in progress. The [M2 input/target ledger](M2_SOURCES.json) and [adaptation patch](../../Documentation/Implementation/evidence/m2-original-adaptations.patch) cover this increment separately from the historical M1 hashes above.

| Original source at Blood 2 commit `006b4aabd3fb348f6846cad03799cf8df685ea95` | Use and adaptations |
| --- | --- |
| `ObjectDLL/GameStartPoint.{h,cpp}` | Whole original class. Replace registration with typed defaults; replace `_mbscpy` with bounded copying; include only the selected trigger helper declaration. Original property/name/trigger/lifetime logic retained. |
| `ObjectDLL/pathpoint.{h,cpp}` | Whole original class, normalized include/file casing. Remove unused PathListData dependency; replace multibyte comparisons/copies with ASCII comparison and bounded copies; const string literals; initialize otherwise uninitialized inspector-visible fields; omit unused parameter name. Original scheduling, SMELL expiration, properties, model requests and dimensions retained. |
| `ObjectDLL/Trigger.cpp` | Only `SendTriggerMsgToObject` and `SendTriggerMsgToObjects`, retaining list/message/removal flow. Replace `_mbsicmp` with ASCII case comparison; use original `MID_TRIGGER=1000`; no full Trigger class imported. |
| `ClientShellDLL/BloodClientShell.cpp::StartNewWorld`, `ObjectDLL/BloodServerShell.cpp::LoadWorld`; `Shared/SharedDefs.h` | Selected CMSG_LOADWORLD command/read sequence adapted to a bounded owned local transport. This is an excerpt port, not complete shell integration or binary/network compatibility. The original UI/audio/save/restore/player paths remain absent; `run_world` returns failure. Unsafe filename handling is replaced by normalized resource lookup. |
| `ObjectDLL/B2BaseClass.cpp` registration; `AppHeaders/engineobjects.c` | Add original `GameSettings` editor group marker to accepted properties; use exact `CF_NORUNTIME` classification for preprocessor lights/OutsideDef. No speculative runtime Light objects. |
| `AppHeaders/server_de.h`, `basedefs_de.h`, `cpp_engineobjects_de.h` | Typed `GetNextObject`, `SetObjectDims`, `SetObjectUserFlags` subset and initialization macros; original StartPoint base was already retained by M1. Dimensions are storage, not an implemented collision solver. |

New REZ/DAT code was written for this project after reviewing these donors, with no prototype build/dependency imports:

- `OpenBlood2` at `195dc9d9c1001547d0a369c73e72ff79b696d053`, `OpenLithTech/src/formats/rez_archive_parser.cpp` and `dat_parser.cpp`: directory/BSP layout reference. Partial-success recovery, heuristic property scanning, unchecked offsets and renderer coordinate conversions are not carried over.
- `nomad` at `58fc2e788856ac8142006a40e45c882fd868f8bd`, `src/Nomad.Formats/DatParser.cs`: independent layout cross-check; no C# code or project dependency imported.
- `sources/lithtech` at `0eab18289bed72879eddb648d3311075b108cf46`: `libs/rezmgr/rezmgr.cpp` supplies packed REZ v1 header/directory definitions. `tools/shared/world/EditObjects.cpp` and `PropList.cpp` explain float-encoded integer types and Euler-plus-reserved rotation storage. `runtime/shared/src/geomroutines.cpp::gr_SetupMatrixEuler` supplies the yaw × pitch × roll convention used by the new quaternion conversion. These are selected format/math references from another engine generation, not a compatibility claim for its complete world format.

Validation includes generated synthetic archives/worlds, every-prefix truncation of both fixtures, structural corruptions, deterministic mutations, original class/trigger/lifetime behavior, and the externally installed patched `01_SubwayA`. [World contracts](WORLD_CONTRACTS.md) enumerate the opaque sections and unsupported session services. No commercial archive, DAT, texture, audio or model payload has been copied into the source tree.

## M2 Service Continuation

The independent [continuation ledger](M2_SERVICES_SOURCES.json) records this batch;
prior M1/M2 ledgers remain historical snapshots. The donor Blood 2 commit remains
`006b4aabd3fb348f6846cad03799cf8df685ea95`.

| Source | Selection and changes |
| --- | --- |
| `ObjectDLL/WorldProperties.cpp` | Whole class implementation. Move its class declaration into a header, add `override`, replace registration macros with typed defaults. Original property processing and destructor body retained. |
| `ObjectDLL/SmellHint.{h,cpp}` | Whole class. Remove unused debug/header dependencies, use selected DLink helpers, typed registration, initialize its intrusive link. Original list/update/expiry behavior retained. |
| `AppHeaders/dlink.h` | `DLink`, `dl_TieOff`, `dl_Insert`, `dl_Remove`; initialize link fields. Exclude unrelated list macros and aliasing casts. |
| `AppHeaders/server_de.h`, `basedefs_de.h`, `basetypes_de.h` | Select typed animation, velocity/acceleration, teleport, global-force, console-variable and ArgList declarations/constants. No legacy binary ABI. |

ABC is new bounded code based on reviewed layout references from OpenBlood2
`195dc9d9c1001547d0a369c73e72ff79b696d053`,
`OpenLithTech/src/formats/abc_parser.cpp` and `src/BinaryReader.cpp`. It retains
animation dimensions and deformation bytes that the donor discards. No donor
parser, exception recovery, invalid-node fallback or build architecture was
imported. Node deformation layout/decompression was cross-checked with
NoMadEngine `6d9107368e3b3208d98298c2c36f97f82cb4bc3f`,
`docs/formats/specs_abc_v6.md`, and verified on the retail Caleb coat. This is a
format reference, not a claim that donor gameplay or rendering is correct.

The new swept-AABB/BVH implementation is not imported LithTech player physics.
`SURF_SOLID` bit 0 was checked against
`sources/lithtech/runtime/world/src/de_world.h` at
`0eab18289bed72879eddb648d3311075b108cf46`, then exercised on the selected v56
world. Full original movement semantics remain an open gate. See
[model/collision contracts](MODEL_AND_COLLISION_CONTRACTS.md).


## M2 Original Player Entry Continuation

[M2_PLAYER_SOURCES.json](M2_PLAYER_SOURCES.json) records source hashes, selected
functions/ranges and current target hashes. These are **partial source ports**,
not whole original class imports. [PLAYER_CONTRACTS.md](PLAYER_CONTRACTS.md)
enumerates omitted branches and the deliberately open gameplay gate.

- `PlayerObj.cpp/.h`: selected constructor fields, PostPropRead, InitialUpdate,
  alive single-player Respawn, single-player GoToStartPoint, ValidateAttributes,
  initialization-message fields, first-update scheduling/synchronization and
  lifecycle callback branches. Original resource names, flags, profile tables,
  health/ammo and start selection are retained. Checked return values, default
  Caleb-only admission, rollback and safe string/list ownership are adaptations.
- `BaseCharacter.cpp/.h`: entry ancestry and damage/inventory aggregate attachment;
  movement, containers, special user flags and character command execution are
  not imported. Unhandled model commands are exposed in inspection.
- `Destructable.cpp/.h`: entry fields/setters, intrusive-list initialization and
  destruction, resistance calculation. Damage processing is not imported.
- `InventoryMgr.cpp/.h`: owner/client, ammo subset and original muzzle-light
  creation/removal. Link teardown removes the child before nulling its handle
  under the new engine's notification contract. No weapon/item facade is added.
- `BloodServerShell.cpp`: fresh single-player creation, association, initialization,
  placement and exit branches; Update's single-player return; path snapshot from
  `PathMgr::BuildPathList` and `PathListData::Copy`. Teams, restored client refs,
  voice, saves and multiplayer are not imported.
- `BloodClientShell.cpp`: original 5/3/3/1 profile, load-command wrapper, selected
  entry/exit state, initialization message/reply and position/physics/stats
  handling. Typed local variants replace legacy packet calls. Autosave and
  GS_PLAYING are intentionally not issued while required services are absent.
- `ObjectUtilities.cpp::MoveObjectToGround`: downward ray, animation-0 dimensions
  and collision-aware movement, adapted to bounded primary-world/object queries.
  An empty/missing start fails instead of teleporting to origin; its original
  early-return list leak is fixed. Body/animation dimensions remain distinct.

No whole prototype, new dependency, retail asset payload, raw vtable or DLL ABI
was imported. [The adaptation patch](../../Documentation/Implementation/evidence/m2-player-original-adaptations.patch)
compares selected source text with the new files. Older ledgers remain historical.

## M2 Inventory and Weapon Continuation

[M2_WEAPON_SOURCES.json](M2_WEAPON_SOURCES.json) is the current selection/hash ledger
for inventory, weapons and affected player/shell code. Earlier ledgers and counts
remain historical. All source comes from the same original Blood 2 commit above.

- `Shared/WeaponDefs.h/.cpp`: complete field schema and the two Beretta/knife
  entries; const strings and fixed-width types, no unrelated weapon table imports.
- `ObjectDLL/InventoryMgr.h/.cpp`: selected slots/dual acquisition/change, ammo
  capacity, strength/magic clipping and firing dispatch, alongside the prior
  owner/muzzle subset. Owned `unique_ptr` slots, bounds/finiteness checks and
  failed-entry cleanup replace manual lifetime hazards. Items/pickups are absent.
- `ObjectDLL/Weapon.cpp` and `GameWeapons.h`: selected common vector firing,
  ammo, spread, eye alignment and normal/melee damage delivery. Immutable source
  definitions replace redundant copied fields. Ray services replace engine
  callbacks; projectile/surface/FX branches remain explicitly unported.
- `ClientShellDLL/ViewWeapon.cpp` and `GameWeapons.cpp`: selected client
  draw/rest/idle/start/fire/end transitions, animation completion/key processing,
  typed fire request, and melee fire/fire2 selection. Owned ABC tracks replace
  renderer objects. Inspector input is not the full original input controller.
- `PlayerObj.cpp`: original initialization obtains weapons; typed fire-message
  routing reaches the inventory. Change-based health/armor/bullet synchronization
  is an inspection subset of UpdateStats, with full-precision values.
- `Destructable.cpp`: selected normal/melee resistance, armor absorption, health
  and dead-state branch, dispatched through the existing original aggregate chain.
  AI hit limbs, impulses, powerups, death reactions and triggers remain pending.
- `AppHeaders/generic_msg_de.h`: verified MID_DAMAGE 1001; typed owned payload
  replaces the packet sequence. No retail wire compatibility is asserted.

Local `minstd_rand` generators replace global CRT `srand/rand`; retail random-bit
parity is unverified. Client firing starts at the inspection camera rather than
ported muzzle/bob offsets; hand model attachments and audio/FX are deferred.
The [contracts](WEAPON_CONTRACTS.md) identify all intentionally incomplete paths.
The [adaptation patch](../../Documentation/Implementation/evidence/m2-weapons-original-adaptations.patch)
compares selected source ranges against current implementations.

## M2 Original MoveMgr Continuation

[M2_MOVEMENT_SOURCES.json](M2_MOVEMENT_SOURCES.json) records current movement
source selections/hashes. Earlier ledgers remain historical snapshots.

- Original `ClientShellDLL/MoveMgr.h/.cpp`: selected lifetime, digital controls,
  grounding, dry-land acceleration/drag/jump, motion, forced-position and
  position-message branches. Owned typed values and a checked local body replace
  the old ClientDE/PhysicsLT duplicate model.
- `ClientShellDLL/ClientUtilities.cpp`: slope projection helper;
  `Shared/SharedMovement.h`: original supported control flag values.
- `ObjectDLL/PlayerObj.cpp`: movement-message dispatch/byte code and forced
  position updates. The shared body resolves collisions once; no prediction
  leash teleport or short-velocity packet compression is emulated.
- Original client/server shells: selected MoveMgr update/physics/force-position
  and position-message routing, fitted into the existing local session.

The bounded constant-acceleration integrator, support probe, 120 Hz substeps,
contact-normal gravity constraint and sleep threshold are explicit new engine
adaptations. Later-generation `sources/lithtech/runtime/.../motion.cpp` was read
for context, not imported. Original animation-driven collision dimensions,
containers, stairs and pushers are not advertised as implemented. See the
[movement contracts](MOVEMENT_CONTRACTS.md) and
[selected-source comparison](../../Documentation/Implementation/evidence/m2-movement-original-adaptations.patch).

## M2 Locomotion Continuation

[M2_LOCOMOTION_SOURCES.json](M2_LOCOMOTION_SOURCES.json) records original
PlayerObj animation selection, Anim_Sound names/CheckAnims, MoveMgr crouch rules
and the SDK stair flag. [LOCOMOTION_CONTRACTS.md](LOCOMOTION_CONTRACTS.md) separates
these game rules from the new atomic shape resize and three-sweep stair solver.
Later LithTech is evidence for the default stair-height rule only, not an engine
import or a Blood II parity guarantee. This supersedes the previous entry's
pending stairs/animation-dimensions statement. Earlier hash ledgers are historical.

Runtime C++ namespaces are now `OpenBlood2`; original global SDK/game
classes keep their source names. The user's `.clang-format` and `.editorconfig`
are versioned. Formatting changes are isolated in commit `9ac915d`; the previous
M2 implementation was checkpointed in `007d5ca` before this continuation.

## M2 Container Continuation

[M2_CONTAINER_SOURCES.json](M2_CONTAINER_SOURCES.json) records selective imports
from VolumeBrush/VolumeBrushTypes, ContainerCodes, MoveMgr, PlayerObj's container
and air branches, and Destructable's suffocation/death conditions. The original
base/aggregate chain remains intact. [CONTAINER_CONTRACTS.md](CONTAINER_CONTRACTS.md)
separates these rules from new closed-shell geometry, typed local queries, slot
ordering and bounded substep timing. Later LithTech informs only world-model
pivot/placement, not compatible BSP parsing. Original DAT base/extra counts are
now retained separately. No prototype engine or dependency was imported.
Earlier ledgers and patches remain historical evidence, not current file hashes.

## M2 Specialized Volume Continuation

[M2_SPECIALIZED_VOLUME_SOURCES.json](M2_SPECIALIZED_VOLUME_SOURCES.json) records
Conveyor/Damage/FreeFall registration and constructors, Conveyor server and
MoveMgr branches, and CPlayerObj's initial volume-damage protection. The new
level clock is a typed service adaptation; the session scheduling clock is
unchanged. [SPECIALIZED_VOLUME_CONTRACTS.md](SPECIALIZED_VOLUME_CONTRACTS.md)
documents source quirks, authored damage identifiers and explicit map failures.
No reference tree, dependency or prototype codebase was modified or imported.

## M2 Sliding Door Continuation

[M2_DOOR_SOURCES.json](M2_DOOR_SOURCES.json) records the selected Door registration,
properties, state machine, waveform and message branches, original SDK flags and
Weapon fire-through branch. [DOOR_CONTRACTS.md](DOOR_CONTRACTS.md) distinguishes
these game rules from the new atomic translation/pusher service and geometry
binding. Trigger spawning, sound, portal visibility and rotating doors are not
claimed implemented. No prototype codebase or dependency was imported.

## M2 Trigger and Key Continuation

[M2_TRIGGER_SOURCES.json](M2_TRIGGER_SOURCES.json) records Trigger/BigTrigger/
ToggleTrigger properties, filters, counters, delays, messages and toggles; Door
SpawnTrigger, child state changes and lock relay; InventoryMgr/CInvKey key rules;
CPlayerObj key request/reply branches; and selected KeyPickup/PickupObject rules.
Owned strings/records and typed key messages replace legacy storage and packet
ABI. The original aggregate-to-BaseClass reply cast is corrected to the player.
B2BaseClass filtering gains a diagnostic marker, without changing the filter.

[TRIGGER_CONTRACTS.md](TRIGGER_CONTRACTS.md) documents the stronger recursion and
child-lifetime guards, nonpositive trigger reset adaptation, explicit localized
name fallback, collision extension and remaining USE/feedback/persistence limits.
Later LithTech timer documentation informs the ResetTime<=0 convention; it is not
claimed to establish retail Blood II engine parity. Earlier ledgers stay historical.
No prototype dependency, reference source or commercial asset was imported.


## M2 original USE and rotating-door continuation

[Source selections](M2_INTERACTION_SOURCES.json) identify the selective original
RotatingDoor properties/angle/pivot/state code and PlayerObj USE/door-push branches.
[Contracts](INTERACTION_CONTRACTS.md) distinguish source rules from typed local
command dispatch, entry-face ray filtering and the bounded replacement rotating
collision solver. No prototype engine dependency or ECS was imported. The old
force/crush protocol, full player session and other movers remain outside this
increment. Source angular quirks and the original direct-door USE exclusion are
retained; original-executable behavior and other platforms remain unverified.

## M2 rotating and destructible brush continuation

[Source selections](M2_BRUSH_SOURCES.json) record RotatingBrush/Rotating and
selected CDestructableBrush/CDestructable callbacks, registration and properties.
CDestructable moved out of PlayerObj without replacing its existing player
health/armor branch. Added named damage/death messages and KILL retain owned
transport and deferred lifetimes. CDebris/WorldModelDebris remain unported;
a bounded diagnostic records removal requests without claiming FX playback.
[Contracts](BRUSH_CONTRACTS.md) distinguish source timing from the existing
replacement rotation solver and explain the supporting later-engine reference.
No reference source, prototype dependency or commercial content was modified.

## M2 explosive brush and debris-request continuation

[Source selections](M2_EXPLOSION_SOURCES.json) record original CDebris properties,
material order, velocity and explosion branches, SetupClientGibFX's repeated
in-place negation, and ObjectUtilities' squared-distance area damage. Owned
inspection records replace unimplemented client FX/audio; they do not simulate
fragments. [Contracts](EXPLOSION_CONTRACTS.md) document fixed effective damage/
radius despite unused authored properties, snapshot/liveness guards and deferred
chains. Earlier source ledgers remain historical. WorldModelDebris, pushable
brushes, impulses, SpawnObject and full gameplay remain pending.

## M2 pushable/gravity brush continuation

[Source selections](M2_PUSHABLE_SOURCES.json) connect CDestructableBrush's pushable
flags/priorities, CDestructable mass/friction and damage impulse rules, and
CPlayerObj ground/air priorities. [Contracts](PUSHABLE_CONTRACTS.md) distinguish
these original rules from the replacement box integrator, atomic movement and
conservative rider/pushing limits. Later LithTech motion is an architectural
reference only. The preceding pushable/impulse omissions are superseded for this
bounded brush subset; dynamic polygon bodies, player impulses, SpawnObject,
WorldModelDebris and the full M2 gate remain open.

## M2 pickup and destruction-spawn continuation

[Source selections](M2_PICKUP_SOURCES.json) record selected PickupObject, AmmoPickup,
PowerupPickup and concrete constructors, player message branches, CDestructable
stat/HandleDestruction rules and the global Spawner.cpp helper. The general Spawner
class is not imported. [Contracts](PICKUP_CONTRACTS.md) explain typed in-process
messages, bounded non-mutating parsing, checked acknowledgement and removal,
resource failure behavior and the intentionally omitted multiplayer/FX branches.
Earlier ledgers remain historical; their blanket SpawnObject/general-pickup
omissions are superseded only for the twelve supported leaf classes. No prototype
dependency, reference modification or commercial asset was introduced.

## M2 mission objectives

[Source selections](M2_OBJECTIVE_SOURCES.json) record ObjectivesTrigger properties,
precreate/SendMessages and the original client SMSG_OBJECTIVES branch and ID bases.
Owned strings, typed local delivery, checked queue failures and bounded IDs replace
legacy message/HSTRING storage. The portable PE string reader is new engine code
based on Microsoft documentation, with no prototype dependency or native DLL
execution. [Contracts](OBJECTIVES_CONTRACTS.md) distinguish resource resolution,
source sound requests and pending original HUD/audio. Previous ledgers stay historical.

## M2 scripted cameras

[Source selections](M2_CAMERA_SOURCES.json) record CameraObj callbacks/properties,
CameraFX metadata, the client external-camera selection/visibility/movement branches
and the server weapon movement guard. [Contracts](CAMERA_CONTRACTS.md) distinguish
source rules from typed descriptors, checked lifetime, look-direction math and
stable client selection. Cinematic rendering and audio remain presentation requests;
KeyFramer/CinematicTrigger and full M2 acceptance are not implemented by this port.
Earlier ledgers remain historical; no dependency pin or reference tree changed.

## M2 KeyFramer / KeyData continuation

[Selection hashes](M2_KEYFRAMER_SOURCES.json) record original ObjectDLL/KeyFramer
and KeyData fields/callbacks/timeline/interpolation/message branches. Owned vectors
and strings replace KEYNODE/CDynArray/HSTRING storage, preserving original component
rotation offsets and endpoint/loop timing quirks. [Contracts](KEYFRAMER_CONTRACTS.md)
record the target-offset removal fix, bounded discovery and reentrant command guard.
The typed engine interpolation/motion helpers and UI are new implementation code.
Sound/BPrint requests, world/container movers, CinematicTrigger and full M2 remain
unimplemented; these diagnostics are not successful audio/physics service stubs.

## M2 dynamic lights

[Source selections](M2_LIGHT_SOURCES.json) record ClientLightFX callbacks/properties,
CClientSFX ancestry, all thirteen presets and original client CLightFX wave tables,
conversion and update rules. Owned typed records replace SFX packet/client object
allocation. [Contracts](LIGHT_CONTRACTS.md) identify safe negative-phase wrapping,
bounded descriptors, local portable jitter and retained source quirks. Registry,
UI/placement plots and test harnesses are new project code. No world-lighting,
shadow/fog, audio or generic SFX-manager service is claimed. Earlier omissions of
GlowingLight are superseded only for this selected server/client subset.

## M2 translating train paths

[Source/engine ledger](M2_WORLDPATH_SOURCES.json) identifies the unchanged original
KeyFramer sequencer and SDK motion/flag contracts. No new source class or prototype
code is imported. [Path contracts](WORLDPATH_CONTRACTS.md) document new typed
unchanged-orientation routing and primary-world swept bounds in the existing atomic
pusher/rider service. The observed two train blocks and all-or-nothing rejection
are replacement physics, not claimed retail behavior. Earlier blanket world-path
omissions are superseded only for static translation; no dependency or reference
repository changed.

## M2 cinematic cue audio

[Original source selections](M2_CINEMATIC_SOURCES.json) identify the CinematicTrigger
properties/callbacks/sequencer and legacy sound lifetime contract. [Cinematic
contracts](CINEMATIC_CONTRACTS.md) separate original timing/messages from owned
strings, checked IDs, a source list-leak fix, malformed-data validation and new
bounded SDL decoding/dry mixing/output. No prototype audio backend or dependency
was imported. Missing audio remains failure; reverb is explicitly unavailable.

## M2 original RandomSoundFX

[Source selections](M2_RANDOM_SOUND_SOURCES.json) record exact original files/ranges
and hashes; [contracts](RANDOM_SOUND_CONTRACTS.md) describe the adaptation boundary.
Original commands, properties, random/sequential scheduling, fixed X/Z placement,
voice replacement and destructor lifetime are ported directly. The duplicate
FreeString is fixed with one owned read release; invalid ranges/restore reject.
Typed fixed-position playback and ambient distance-only mixing are new engine
services. The portable RNG is not the retail sequence. Built-in Sound loops, priority
arbitration, reverb/Doppler, save/restore and the actual M2 gate remain open.

## M2 built-in Sound loops

[Source selections](M2_WORLD_SOUND_SOURCES.json) identify the original built-in
Sound C callback, properties and SDK sound ownership contract. The C++ adaptation
retains unconditional source removal and transfers successful playback to the
world. [Contracts](WORLD_SOUND_CONTRACTS.md) separate original rules from new
bounded full-clip resampling, loader disposition and inspection. Runtime filename
initialization is safe; streaming and region metadata explicitly reject playback.
Later-generation LithTech region-loop code was reviewed only, not imported.
No prototype backend or dependency was added. M2 remains incomplete.

## DetailSprite and SPR continuation

[Selected original files and hashes](M2_SPRITE_SOURCES.json) document DetailSprite
properties, initialization and independent repeats. [Contracts](SPRITE_CONTRACTS.md)
separate original rules from bounded ray/parser/clock adaptations. Later-generation
SPR references were consulted and checked against Blood II manifests; no donor
reader or DTX decoder was imported. Save/restore and sprite rendering remain open.

## Scene explosion continuation

[Original source hashes/selections](M2_SCENE_EXPLOSION_SOURCES.json) document
CClientExplosionSFX's authored server path, shared null-damager guard and
PlaySoundFromPos descriptor. [Contracts](SCENE_EXPLOSION_CONTRACTS.md) distinguish
actual server/audio behavior from unavailable client FX. No donor backend,
dependency or guessed gameplay correction was imported.

## Original character animation/node continuation

CAnim_Sound animation tables, CheckAnims, hit-sphere measurements and ForceAnimation
are selectively imported; CPlayerObj consumes the actual table on character setup
and locomotion. See [source ledger](M2_CHARACTER_ANIMATION_SOURCES.json) and
[contracts](CHARACTER_ANIMATION_CONTRACTS.md). Node transforms use the existing
bounded ABC sampler; later LithTech informs name matching only. OpenBlood2's
false-returning node function was reviewed and not imported. Voice helpers,
AI callers, damage localization, attachments and blending remain unimplemented.

Character audio continuation: [source ledger](M2_CHARACTER_AUDIO_SOURCES.json),
[contracts](CHARACTER_AUDIO_CONTRACTS.md). Original Anim_Sound and PlayerObj sound
rules are integrated selectively; no reference repository is a build dependency.

Character motion continuation: [source ledger](M2_CHARACTER_MOTION_SOURCES.json),
[contracts](CHARACTER_MOTION_CONTRACTS.md). BaseCharacter and Sparam rules are
selectively adapted; existing math services replace SDK calls. Later LithTech
rotation code informs semantics only. No reference repository is a dependency.

AI model-key continuation: [source ledger](M2_AI_MODEL_KEY_SOURCES.json),
[contracts](AI_MODEL_KEY_CONTRACTS.md). Selected AI_Mgr dispatch preserves source
camera/script gates, branch priority, shared voice semantics and the supplied
B2_LITE variant cap. No full AI class or guessed attack behavior is registered.

AI path-manager continuation: [source ledger](M2_PATH_MANAGER_SOURCES.json),
[contracts](PATH_MANAGER_CONTRACTS.md). Original snapshot/prefix/reverse-order rules
are retained with owned values. Later engine code confirms active enumeration only;
no donor container or backend is imported. AI following and action delivery remain open.

## Original AI visibility

Selected AI_Mgr::CanSee and GlobalFilterFn liquid/glass branches now use checked
node/ray services. [Source ranges and hashes](M2_AI_SIGHT_SOURCES.json) and
[contracts](AI_SIGHT_CONTRACTS.md) distinguish source rules from typed failures,
current-position fallback and the existing replacement collision solver. No
additional production AI class or general filter callback ABI is imported.

## M2 held models and attachment services

[Selected source ranges and hashes](M2_ATTACHMENT_SOURCES.json) cover original
CWeapon hand creation/removal, the current inventory hand branch, dead-player
hiding and CHandWeaponModel held initialization/visibility. Checked object handles
replace bidirectional raw pointers; the unsafe precreate fallthrough is omitted.
The original drop/pickup and client SFX branches are not imported.

The engine attachment store and transform refresh are new replacement services.
The later LithTech shared transform code informed offset composition only; no
code or dependency from that engine was copied. See [contracts](ATTACHMENT_CONTRACTS.md)
for bounded visual children, unsupported chains/physics and strengthened lifetime.

## M2 explicit AI targets and facing

[Source selections](M2_AI_TARGET_SOURCES.json) cover original AI_Mgr SetTarget,
SetNewTarget, MC_FaceTarget, MC_FacePos, target link-break handling and the isolated
AI_Update position sample. Current checked transforms replace MoveObj snapshots;
new-link preflight prevents losing an old target on failure. Typed GetObjectPos
outcomes let AI samples reject unresolved attachment poses. Source local-up yaw,
list-first selection, cached targets and Metacmd rules are retained. No production
NPC registration, perception/state machine or script scheduler is imported.
See [contracts](AI_TARGET_CONTRACTS.md) and
[execution evidence](../../Documentation/Implementation/M2_AI_TARGETS.md).

## M2 diagnostic world rasterization

WorldMesh, WorldRenderer, projection and GLSL shaders are new replacement engine
code. They reuse the existing v56 reader, container transform convention and
original CBloodClientShell camera view without importing a donor renderer.
The later-generation `sources/lithtech/runtime/world/src/de_world.h` surface bits
(59–63) informed invisible/transparent/sky diagnostics only; complete material
compatibility is not claimed. SDL's pinned SDL_gpu.h defines NDC, resource binding,
depth clipping and GPU resource lifetime. No game rule changed. See
[contracts](WORLD_VIEW_CONTRACTS.md) and
[evidence](../../Documentation/Implementation/M2_WORLD_VIEW.md).

## M2 DTX world materials

`texture.cpp` is a new bounded reader informed by the C++ DTX parser in
`OpenBlood2/OpenLithTech/src/formats/dtx_parser.cpp` and its NoMadEngine counterpart,
then checked against the complete externally mounted retail corpus. It does not
copy their basename/payload-size guesses, unrestricted allocations or implicit
fallbacks. The older converter's contradictory channel order is not adopted.
The -2 layout is measured data; later `sources/lithtech/runtime/shared/src/dtxmgr.h`
(version -5) is a generation-boundary reference, not a compatible specification.
UV dot products follow the reviewed world surface representation and the donor's
resource-manager calculation, evaluated before subtracting the authored pivot.
Shader sampling, bounded batches, upload lifetime and tests are new engine code;
no gameplay source changed. See [contracts](TEXTURE_CONTRACTS.md).

## M2 animated model presentation

Selected original CViewWeapon::AdjustAngleAndSetPos now places owned client models
using the original WeaponData offsets and left-hand reflection; bob is explicitly
zero. The [source ledger](M2_MODEL_VIEW_SOURCES.json) records reviewed ranges/hashes.
Model snapshotting, bounded skin caching and dynamic SDL_GPU/shared-depth rendering
are new engine code. The ABC reader now conjugates node quaternions for omitted
TransformInfo, matching reviewed legacy defaults and independently checked Beretta
key bounds. Explicit zero/direct and unsupported nonzero flags remain distinct.
See [contracts](MODEL_VIEW_CONTRACTS.md); no gameplay firing/physics rules changed.

The ABC model basis is normalized from file -Z to runtime +Z at decode time, including
S*R*S node rotations and deformation. This is inferred from real Beretta muzzle/hand
geometry and the original fire/camera axes, then checked visually; no pixel-identical
retail claim. Related [ABC v6 importer](https://github.com/haekb/io_scene_lithtech/blob/master/src/reader_abc_v6_pc.py)
and [Godot reader](https://github.com/haekb/godot-abc-reader/blob/master/Models/ABC6.gd)
were reviewed for quaternion inversion and the distinct winding flag; no code was imported.

## M2 AI perception

Selected original AI_Mgr list membership, FindObjectInRadius, ComputeStimuli and
ChooseOpportunityTarget now execute with typed query reports and checked lifetime.
Original player dual-faction membership and the small ExitHint class are imported.
AI attacker memory adapts the original damage identity branch to safe tokens;
local damage/limb behavior and timed-power activation remain incomplete. See
[source ranges/hashes](M2_AI_PERCEPTION_SOURCES.json) and
[contracts and explicit source-UB corrections](AI_PERCEPTION_CONTRACTS.md).

## M2 AI state decisions and stationary actions

Selected original AI_Mgr state parsing, transitions, animation commands, Idle and
Passive handlers, CivilianAI decisions/Idle/Special1..8 and CultistAI decisions
are imported with explicit failure and checked ownership. The diagnostic driver
does not implement AI_Update or register partial NPCs. See
[source ranges and hashes](M2_AI_STATE_SOURCES.json) and
[contracts and dimension-service adaptation](AI_STATE_CONTRACTS.md).

## M2 AI weapon actions

Selected original InventoryMgr selection, AI_Mgr default Fire/CheckClearShot,
MC_Fire_Stand/MC_BestWeapon and key firing are connected with typed failures and
checked lifetimes. CWeapon's original player-only eye-alignment condition is
restored. See [source ranges/hashes](M2_AI_FIRE_SOURCES.json) and
[contracts/adaptations](AI_FIRE_CONTRACTS.md). No production NPC is registered.

## M2 AI commands and hearing

Selected original AI_Mgr HandleTrigger/PlayAnimation/message dispatch and
ObjectUtilities SendSoundTrigger are integrated, including the original player
sound caller. See [source hashes/ranges](M2_AI_TRIGGER_SOURCES.json) and
[bounded contracts/adaptations](AI_TRIGGER_CONTRACTS.md). Scripts and full NPCs
remain unported; checked snapshots replace mutable callback list traversal.

## M2 original movement aggregate and navigation

Selected ObjectDLL movement.cpp/h cache, clearance, obstacle turns and consolidation
are integrated with the original CBaseCharacter aggregate order. [Source selections](M2_AI_NAVIGATION_SOURCES.json)
and [contracts](AI_NAVIGATION_CONTRACTS.md) record initial-sample safety, value
ownership, bounds and explicit omissions. Full NPC initialization, following and
shade queries remain unavailable; collision is the existing replacement solver.

## M2 original script movement helpers

Donor remains `sources/blood2` at `006b4aabd3fb348f6846cad03799cf8df685ea95`.
Selected AI_Mgr movement/following and Trigger named-action rules are recorded in
[M2_AI_FOLLOW_SOURCES.json](M2_AI_FOLLOW_SOURCES.json). Direct typed helpers,
owned snapshots, checked message/lifetime boundaries and explicit failure returns
replace unbounded legacy ownership and ignored failures. No other repository or
commercial asset was imported. The inspector/smoke fixtures are new diagnostic
code, not production NPC or interpreter implementations. See
[contracts](AI_FOLLOW_CONTRACTS.md) and [evidence](../../Documentation/Implementation/M2_AI_FOLLOW.md).

## M2 original AI script interpreter state

Donor remains `sources/blood2` at `006b4aabd3fb348f6846cad03799cf8df685ea95`.
[M2_AI_SCRIPT_SOURCES.json](M2_AI_SCRIPT_SOURCES.json) records the selected
AI_Mgr/AIScriptList parsing, sequencing and dispatch rules. The original trigger's
forced AI_Update is deliberately still unsupported; explicit preparation and a
labelled diagnostic state driver exercise this prerequisite. New owned containers,
bounded parsing, overflow/lifetime guards and explicit service errors replace
legacy unsafe storage/ignored failures. Source quirks are retained and tested.
No other repository dependency or commercial asset was imported. See
[contracts](AI_SCRIPT_CONTRACTS.md) and [evidence](../../Documentation/Implementation/M2_AI_SCRIPT.md).

## M2 autonomous locomotion and Cultist attack states

Donor: `sources/blood2` at `006b4aabd3fb348f6846cad03799cf8df685ea95`.
[Selected source ranges/hashes](M2_AI_LOCOMOTION_SOURCES.json) cover AI_Mgr and CultistAI.
Four parallel command bodies are consolidated with source differences preserved;
checked existing services, owned diagnostics and callback guards are adaptations.
Shade-dependent recovery fails explicitly; no production class or complete AI_Update
is claimed. See [contracts](AI_LOCOMOTION_CONTRACTS.md) and
[evidence](../../Documentation/Implementation/M2_AI_LOCOMOTION.md). No reference
repository, dependency revision or commercial asset was changed/imported.

## M2 AI recovery and destination states

Donor: `sources/blood2` at `006b4aabd3fb348f6846cad03799cf8df685ea95`.
[Source ranges/hashes](M2_AI_RECOVERY_SOURCES.json) select AI_Mgr jump/crouch/crawl
and seven navigation states. Shared plumbing retains source phase differences;
checked failure/lifetime paths and the failed-plan latch are documented adaptations.
See [contracts](AI_RECOVERY_CONTRACTS.md) and
[evidence](../../Documentation/Implementation/M2_AI_RECOVERY.md). Dependencies,
reference trees and commercial assets are unchanged; full NPCs remain unavailable.

## M2 AI escape, hiding and guard

Donor: `sources/blood2` at `006b4aabd3fb348f6846cad03799cf8df685ea95`.
[Source selections](M2_AI_ESCAPE_SOURCES.json) cover Cultist/Civilian escape/hide,
AI_Mgr beg/guard and selected track handling. Shared plumbing preserves the distinct
source rules. [Contracts](AI_ESCAPE_CONTRACTS.md) separate safety adaptations,
diagnostic hint flags and selected defaults from still-missing full NPC initialization.
[Evidence](../../Documentation/Implementation/M2_AI_ESCAPE.md) records Linux checks.
Reference repositories, commercial assets and pinned dependencies are unchanged.

## M2 production NPC lifecycle

Donor: `sources/blood2` at `006b4aabd3fb348f6846cad03799cf8df685ea95`.
[Source ranges/hashes](M2_AI_RUNTIME_SOURCES.json) cover Cultist/Civilian construction,
AI_Mgr initialization/AI_Update, player activation pings, inventory updates and
damage factors. Another LithTech generation supplies only a linear Random reference.
[Contracts](AI_RUNTIME_CONTRACTS.md) distinguish checked-lifetime/error adaptations,
world-scoped tables, replacement visibility/physics and still-missing actions.
No reference tree, commercial asset or dependency pin changed.

## M2 NPC hit localization and recoil

Selected `CDestructable::CalculateHitLimb/SetProperNode/HandleDamage` and
`AI_Mgr::HandleDamage/MC_Recoil/AI_STATE_Recoil`, with the existing Civilian
post-damage override, come from `sources/blood2` commit
`006b4aabd3fb348f6846cad03799cf8df685ea95`. See
[M2_AI_HIT_SOURCES.json](M2_AI_HIT_SOURCES.json) for hashes and exact selections.
Typed node outcomes, receipt/lifetime guards, explicit zero-segment handling and
failure diagnostics are portability/safety adaptations. No prototype, new
dependency, commercial asset or death-service placeholder was imported.
[Contracts](AI_HIT_CONTRACTS.md) and [validation](../../Documentation/Implementation/M2_AI_HIT.md)
define the visible-node subset and remaining M2 gate.

## NPC weapon drops and Beretta pickups

Selective original DropCurrentWeapon/DropHandModel/CHandWeaponModel, WeaponPickup,
BerettaPU, SpawnWeaponPickup and PlayerObj MID_ADDWEAPON rules. See the
[source ledger](M2_WEAPON_DROP_SOURCES.json) and [contracts](WEAPON_DROP_CONTRACTS.md).
No donor engine code/dependency was imported. Original NOSLIDING bit 25 is
implemented by the existing replacement solver; later LithTech moveplayer was
consulted for first-contact meaning only, not treated as retail Blood II proof.

## Inspector folding and keyboard defaults

Application-owned SDL/ImGui usability changes only. No new original-source import
or dependency update. Existing MoveMgr bindings and camera gates remain unchanged;
see [evidence](../../Documentation/Implementation/M2_CONTROLS.md).

## Ordinary NPC death and CCorpse

Selected original MC_Dead/AI_STATE_Dying/CreateCorpse, lethal gates and corpse
initial/update/model-key rules. [Source ledger](M2_CORPSE_SOURCES.json) records
original hashes and selected ranges; [contracts](CORPSE_CONTRACTS.md) distinguish
source behavior, owned lifetime/rollback adaptations and missing fire/gib/FX
branches. No donor engine, new dependency or commercial asset was imported.

## M2 level exits

Direct selective import from original Blood II commit
`006b4aabd3fb348f6846cad03799cf8df685ea95`: ExitTrigger properties/precreate/broadcast,
Trigger virtual delayed-slot dispatch, BloodServerShell start-name copying and
BloodClientShell HandleExitWorld request/dead guard. Typed strings/events, checked
local lifetime and explicit NEW_LEVEL refusal replace unimplemented keep-alive
services. No prototype code, binary library or commercial asset was imported.
See [source selections](M2_EXIT_SOURCES.json), [contracts](EXIT_CONTRACTS.md) and
[validation](../../Documentation/Implementation/M2_EXITS.md).

## Original inventory preservation

Selected CInventoryMgr Save/Load/Term and inherited CInvKey base fields, directly
from the original Blood II source. Typed owned snapshots and staged rollback
replace destructive message reads; no engine save format is claimed. Active
player inventory is captured at exit, but whole-player keep-alive transfer remains
refused. No donor code, dependencies or commercial assets were imported. See
[source ledger](M2_INVENTORY_STATE_SOURCES.json), [contracts](INVENTORY_STATE_CONTRACTS.md)
and [evidence](../../Documentation/Implementation/M2_INVENTORY_STATE.md).

## Original damage state preservation

Selected CDestructable Save/Load fields, original death counter/delay and character
nigh-invulnerability modifier are directly ported from the original Blood II source.
Typed owned snapshots, checked destination binding and preflight replace packet
reads; the source's uninitialized location byte starts at zero. No donor engine,
dependency or commercial asset was imported. See [source ledger](M2_DAMAGE_STATE_SOURCES.json),
[contracts](DAMAGE_STATE_CONTRACTS.md) and
[evidence](../../Documentation/Implementation/M2_DAMAGE_STATE.md).

## M2 player engine restoration and saved-client entry

[Source ledger](M2_PLAYER_RESTORE_SOURCES.json) identifies the original server
saved-client/RespawnPlayer branch, client first-world guard and player first-update
block. `player_restore.cpp` is replacement engine orchestration using owned values,
checked identities, current geometry and staged cleanup. It reuses the selected
original Save/Load and model tail. First update now derives speeds without fresh
attribute validation, matching source. See [contracts](PLAYER_RESTORE_CONTRACTS.md)
for limits: no retail ABI, full client/world save or cross-world transfer claim.

## Named keep-alive player entry

The [source ledger](M2_PLAYER_KEEPALIVE_SOURCES.json) records original shell
RespawnPlayer and CPlayerObj GoToStartPoint selections. Existing aggregate Load
uses KeepAlive type 1 to discard keys. Explicit mode/start ownership, final body
clearance and callback-lifetime checks are engine adaptations. No dependency,
reference repository or commercial resource is imported. See
[contracts](PLAYER_KEEPALIVE_CONTRACTS.md); automatic NEW_LEVEL remains refused.

## Original-sprite presentation

[Source ledger](M2_SPRITE_VIEW_SOURCES.json) records unchanged original DetailSprite
placement and SDK flags, including disabled chromakey. The later LithTech D3D
renderer is a read-only geometry/UV reference. New SpriteView and an explicit
SDL_GPU alpha style reuse existing readers and shaders without importing that
renderer. Actual DTX alpha, bounded owned caches and stable sorting are replacement
presentation; retail size/blending parity remains unverified. See
[contracts](SPRITE_VIEW_CONTRACTS.md). No dependency or commercial asset was added.

## Original explosion sprite effects

[Source ledger](M2_EXPLOSION_SPRITE_SOURCES.json) identifies CExplosionSpriteFX
Init/CreateObject/Update, selected high-detail CExplosionFX sprite recipes, original
CSFXMgr timing and server delivery order. Owned typed state replaces client ABI
handles; bounded capacity, explicit resource failures and initialized scale state
are adaptations. Shared alignment and per-draw SDL_GPU alpha extend the existing
small engine. Composite components and random alignment are not replaced with
placeholders. See [contracts](EXPLOSION_SPRITE_CONTRACTS.md); references/assets stay external.

## Original explosion model effects

[Source ledger](M2_EXPLOSION_MODEL_SOURCES.json) records CExplosionModelFX methods,
high-detail CExplosionFX model recipes, ClientUtilities random formulas and source
update/delivery order. Typed ownership, finite budgets, private portable RNG and
explicit failures adapt engine services; original wave/fade/per-update rotations
remain. Existing ABC/DTX/ModelView and the SDL_GPU alpha shader supply presentation.
No reference engine, dependency or commercial asset is imported. See
[contracts](EXPLOSION_MODEL_CONTRACTS.md) for remaining composite/rendering limits.

## Original explosion point lights

Selected CExplosionLightFX methods and CExplosionFX high-detail light branches come
from sources/blood2/ClientShellDLL/ExplosionFX.cpp/.h. The [source ledger](M2_EXPLOSION_LIGHT_SOURCES.json) records hashes and exact ranges;
[contracts](EXPLOSION_LIGHT_CONTRACTS.md) distinguish source delay/radius quirks
from explicit initialization, bounded values and owned client-light adaptations.
No reference file, external asset, dependency revision or renderer was modified.

## Original explosion flame particles

See [source selections and hashes](M2_EXPLOSION_FLAME_SOURCES.json) and
[contracts](EXPLOSION_FLAME_CONTRACTS.md). CExplosionFlameFX preserves original
three-zone bursts, clocks/fades and unused ramps, with the high-detail Tesla recipe.
Owned typed state, explicit bounds, later-engine six-sample bulk service and existing
particle rendering are documented adaptations, not a retail engine/RNG parity claim.

## Destination preparation

The [source ledger](M2_DESTINATION_SOURCES.json) records the original shell ordering and selected preprocessor-only classes.
The [read-only preparation](DESTINATION_CONTRACTS.md) is new engine code using existing DAT/collision readers and exact property schemas;
no new original method, reference engine dependency or commercial asset is imported. Original NEW_LEVEL execution remains refused.

## Isolated destination initialization rehearsal

New `destination_trial.cpp` exchanges the ported original lists, camera, shared AI tables and adapter binding to execute existing original world/KeepAlive entry callbacks in a disposable session. No new original method bodies or reference-engine code are imported.
See [ledger](M2_DESTINATION_TRIAL_SOURCES.json), [contracts](DESTINATION_TRIAL_CONTRACTS.md) and [evidence](../../Documentation/Implementation/M2_DESTINATION_TRIAL.md).
This is synchronous diagnostic isolation, not recovered retail staging or a completed level transfer.

## Original retained weapon fire sounds

Selected Weapon.cpp PlayFireSound, Update sound cleanup and KillSounds execute with
original InventoryMgr all-slot scheduling and ObjectUtilities attached sound/hearing
ordering. See [ledger](M2_WEAPON_AUDIO_SOURCES.json) and [contracts](WEAPON_AUDIO_CONTRACTS.md).
Typed decoded duration, checked voice IDs, callback-aware final rays and explicit
interrupted shots are preservation adaptations. No reference code/assets or pins changed.

## Original vector impacts and material audio

Selected Weapon.cpp FireVector/AddImpact, WeaponFX.cpp impact sound methods and
BaseCharacter flesh flags are integrated. See [ledger](M2_WEAPON_IMPACT_SOURCES.json)
and [contracts](WEAPON_IMPACT_CONTRACTS.md). Typed bounded continuation, later-engine
interior policy, owned client audio RNG/strings and checked callback lifetime are
explicit adaptations. Reference sources/assets and dependency pins remain unchanged.

## Original weapon impact sprites

Selected CWeaponFX flash/smoke recipes and CSmokeImpactFX lifecycle are integrated
with typed owned settings, bounded detached lifetime and existing SDL_GPU sprites.
See [ledger](M2_WEAPON_SPRITE_SOURCES.json) and [contracts](WEAPON_SPRITE_CONTRACTS.md).
Per-callback motion, first-update/delay quirks and wood tint are retained; absent
composite branches and portable RNG/raster differences stay explicit.

## Keyboard fire and relative mouse

The [application controls](PLAYER_CONTROL_CONTRACTS.md) are new SDL mapping/cursor
ownership code. They feed the already ported original weapon state machine, FIRE_KEY
and player rotation; no new game method or reference engine is imported.
See [ledger](M2_PLAYER_CONTROL_SOURCES.json).

## Interactive NPC entry preservation

The [application playback policy](SESSION_PLAYBACK_CONTRACTS.md) holds a loaded world until original player entry. No new original method is imported or AI/faction rule altered; see [source context](M2_NPC_ENTRY_SOURCES.json).

## Transparent world presentation

New engine code binds existing decoded DTX alpha on authored bit-3 faces, reuses
the packaged sprite shader with a world repeat sampler and sorts current posed
polygon centers. No original game method or donor implementation is imported.
The later LithTech surface definition is a reference, not a retail v56 renderer
specification. See [ledger](M2_WORLD_ALPHA_SOURCES.json),
[contracts](WORLD_ALPHA_CONTRACTS.md) and
[evidence](../../Documentation/Implementation/M2_WORLD_ALPHA.md).

## Naga eye weapon and red beam

Original CNagaEyes, complete Naga Eye Beam definition, AI name/head-node branches and selected CLaserBeamFX are integrated as a Gideon dependency. See [source ledger](M2_NAGA_WEAPON_SOURCES.json) and [contracts](NAGA_WEAPON_CONTRACTS.md). The original eye impact composite and Gideon class remain unavailable; no generic replacement is claimed.

## Energy projectile dependency

Selected original CEnergyBlast/CProjectile/CEnergyBlastProjectile methods and complete Energy Blast definition are integrated with typed checked transport and replacement point flight. See [ledger](M2_ENERGY_PROJECTILE_SOURCES.json) and [contracts](ENERGY_PROJECTILE_CONTRACTS.md). Zealot variants, trail/Tesla-primary composite and Gideon remain unavailable.

## Original Gideon wind and shockwave

Selected original Gideon Wind definition, projectile Update and high-detail model/light recipe are integrated. See [ledger](M2_WIND_WEAPON_SOURCES.json) and [contracts](WIND_WEAPON_CONTRACTS.md). The undefined ray endpoint is repaired, typed callback/lifetime checks are explicit, and production Gideon remains unported.

## Original Gideon shield / spell particles

Selected original CGideonShield, complete WeaponData41, AI name/generic firing branch, and ObjectFX19/36 recipe/lifecycle/ring/sphere emission extend the existing CObjectParticleFX. [Source hashes and adaptations](M2_SHIELD_WEAPON_SOURCES.json), [contracts](SHIELD_WEAPON_CONTRACTS.md) and [verification](../../Documentation/Implementation/M2_SHIELD_WEAPON.md) distinguish source gameplay from the typed lifetime, bounded arrays and later-engine rotation/particle policies. No new dependency or commercial resource is imported. Gideon itself remains pending.

## Original Gideon combat methods

Selected ComputeState, MC_Fire_Stand, Fire, MC_Extra and MagicPowerup are directly adapted from the original subclass, with typed failures and existing lifetime rules. See [source ledger](M2_GIDEON_COMBAT_SOURCES.json) and [contracts](GIDEON_COMBAT_CONTRACTS.md). No production boss constructor/registration or mutable shared table is introduced. The NPC initialization ID-bound repair is engine integration, not a gameplay rule change.

## Gideon fade and teleport

Selected MC_FadeOut, AI_STATE_Teleport and Special1 are directly adapted from the original subclass. See [source ledger](M2_GIDEON_TELEPORT_SOURCES.json) and [contracts](GIDEON_TELEPORT_CONTRACTS.md). Color storage/rendering, snapshot preservation, checked callbacks and replacement ray/movement policies are explicit engine adaptations. No production boss or new global is introduced.

## Gideon attack and dodge sequences

Selected original close/far states, dodge commands, inline Recoil and shared Bold taunt are adapted directly from original sources. See [source ledger](M2_GIDEON_ATTACK_SOURCES.json) and [contracts](GIDEON_ATTACK_CONTRACTS.md). Typed failures, inspection and shared Beg/Bold implementation are explicit adaptations; no production boss or new global is introduced.

The [production Gideon continuation](../../Documentation/Implementation/M2_GIDEON_RUNTIME.md) adds original initialization, AI_Update, typed boss-health delivery and inspection. The fifth shared table is isolated during destination rehearsals. It supersedes the earlier blanket production-Gideon omission; complete boss encounter/death parity, required remaining services and safe NEW_LEVEL still keep M2 incomplete.

## NPC activation visibility correction

See [contracts](ACTIVATION_VISIBILITY_CONTRACTS.md) and [source ledger](M2_ACTIVATION_VISIBILITY_SOURCES.json). The original ServerDE activation contract and player/AI scheduling are retained. Existing original CanSee glass/volume filtering is reused for replacement PingObjects center rays after diagnosing SubwayA nglass1; no retail PVS implementation is claimed. Added bounded historical inspection and authored-script/lifetime regressions. No reference source, commercial resource or dependency revision was changed.

## Initial corpse placement

The [source ledger](M2_CORPSE_PLACEMENT_SOURCES.json) records original CreateCorpse/CCorpse references. Cached transforms, raw death dimensions, scale and foot offset remain source-driven. [Bounded initial placement](CORPSE_PLACEMENT_CONTRACTS.md) is new replacement-engine SAT clearance, not imported or recovered retail physics. No reference file or commercial asset was changed.

## Original destructible models

CDestructableModel property, initialization, damage/model replacement, destruction,
alpha and sliding rules are adapted from the original ObjectDLL. Shared typed model
bodies reuse the existing atomic box solver; source contact forces remain absent.
AI CheckClearShot now includes the original model-only destructible exclusion.
See [ledger](M2_DESTRUCTABLE_MODEL_SOURCES.json) and [contracts](DESTRUCTABLE_MODEL_CONTRACTS.md).
No new dependency or mutable global is introduced.

## Original SoundFX controllers

The original ObjectDLL SoundFX property/callback/command/state rules now use the
existing typed audio mixer, including real ramp durations and ambient loops.
See [ledger](M2_SOUND_FX_SOURCES.json) and [contracts](SOUND_FX_CONTRACTS.md).
Owned strings fix the original duplicate destructor release; null commands are
checked. FileStream/TIMESYNC, historical VoiceType, reverb and save remain explicit
limits. No reference source, mutable game global or new dependency is introduced.

## PCM streaming and historical audio metadata

The original FILESTREAM request now uses a new bounded REZ-slice PCM window reader
with the existing local sound clock. This is replacement engine code, not imported
retail implementation. Sound/SoundFX preserve observed VoiceType=0 as documented
unused metadata; no voice-category behavior is invented. Nonzero values and
TypeAction still reject. See [ledger](M2_AUDIO_STREAM_SOURCES.json) and
[contracts](AUDIO_STREAM_CONTRACTS.md). No new dependency or mutable global is added.

## Original Ingram Mac-10

The original WeaponData, CSubMachineGun selection, SubMachineGunPU, SUBMA parser
and generic NPC/vector fire paths now use existing typed services. Original
SetupViewModel fallback restores missing alternate/idle animation handling, including
the actual left Mac-10. See [ledger](M2_MAC10_SOURCES.json) and
[contracts](MAC10_CONTRACTS.md). Chrome, multiplayer boost and view-key audio remain
explicitly unavailable. No source asset, dependency or mutable game global is added.

## Original WorldModelDebris

Selected WorldModelDebris properties/Start/Update and CDestructableBrush named-fragment
activation are ported from sources/blood2/ObjectDLL. Rotating and destruction remain
original callbacks. The enclosing-bound motion and primary-world angular envelope
are explicit replacement engine policies, not recovered retail physics. The terminal
empty-list leak is fixed and traversal is bounded. See [ledger](M2_WORLD_DEBRIS_SOURCES.json)
and [contracts](WORLD_DEBRIS_CONTRACTS.md). Reference repositories/assets are unchanged.
