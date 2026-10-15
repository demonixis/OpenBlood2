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
