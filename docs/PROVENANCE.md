# Source and Dependency Provenance

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
