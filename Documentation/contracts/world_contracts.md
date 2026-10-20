# Resource, World and Session Contracts

The [launcher data-mod continuation](launcher_contracts.md#data-mods) extends
`mount_game` with explicitly ordered `--rez` overlays after the unchanged stock
archive order. Whole-mount rollback and resource ownership are preserved;
Nightmares and code mods remain excluded.


Historical unconditional RunWorld refusal below is superseded by [checked session startup](session_start_contracts.md).
The M2 acceptance gate remains separate.

The [CityHub entry continuation](cityhub_entry_contracts.md) supersedes earlier
missing-named-world-model and CityHub refusal statements: absent named BSPs are
explicitly omitted before initial update, with no runtime instance. All other
preparation failures retain their checks. Later simulation and the M2 gate remain
separate from successful NEW_LEVEL initialization.

> Continued by [named keep-alive entry](player_keepalive_contracts.md): supported
> captured players can enter an already loaded different world at a named start.
> Automatic NEW_LEVEL still refuses; staged cleanup is not world rollback.

> Extended by [level-exit contracts](exit_contracts.md): original ExitTrigger and
> shell exit/load messages now execute. NEW_LEVEL still explicitly refuses missing
> keep-alive transfer before teardown; this is not a completed level transition.

> Extended by [character audio contracts](character_audio_contracts.md): original
> voice helpers, player sound keys and material/liquid footsteps now execute.
> Earlier blanket character-audio omissions are historical; AI hearing and M2 remain open.

> Extended by [scene explosion contracts](scene_explosion_contracts.md): original
> CClientExplosionSFX activation/removal and detached audio, preserving its null
> damager (zero delivered damage). Client FX and complete M2 remain unavailable.

The later [sprite contracts](sprite_contracts.md) add original DetailSprite,
strict SPR manifest/frame-reference binding and durable creation failures. DTX
pixels remain undecoded; the complete M2 gate stays open.

The later [world sound contracts](world_sound_contracts.md) add the selected built-in
Sound whole-clip loops and consumed-source accounting. Earlier blanket loop/world
ownership omissions below are historical; streaming and the M2 gate remain open.


> Extended by [brush_contracts.md](brush_contracts.md): RotatingBrush and
> selected destructible brushes are connected. Earlier omissions/counts below
> describe their previous increments; the full M2 gate remains open.


The later [player entry contracts](player_contracts.md) supersede this increment's statements that shell/player entry is entirely absent. Complete gameplay remains blocked; earlier limits below remain unless explicitly superseded.

M2 is **in progress**, with bounded resource/world inspection and a partial original-code connection. This document describes implemented behavior, not the complete roadmap exit gate.

## REZ v1

`engine/resources.cpp` indexes the original little-endian, 168-byte REZ v1 header and directory records. It reads bounded directory blocks, not entire retail archives. Resource payloads are read on demand. Directory names and filenames are normalized using ASCII case folding and slash conversion. Absolute paths, drive prefixes, empty components, `.` and `..` are rejected. There is no basename/suffix fallback.

Mount order is `BLOOD2.REZ`, optional `SOUND.REZ`, optional `BLOOD2P.REZ`, optional `BLOOD2P2.REZ`. A later exact path replaces an earlier entry and retains its override count. `BLOOD2NM.REZ` is excluded. Each mount and a complete `mount_game` are published only after validation. Duplicate normalized paths within an archive, invalid record kinds/ranges, overlapping/repeated nonempty directory blocks and excessive budgets reject the archive. Empty directories may share offset zero. File keys are bounded and skipped according to the original record layout.

Limits: 32 mounts, directory depth 32, 16,384 directories, 100,000 files per archive, 32 MiB directory metadata per archive, 1,024-byte normalized paths, 255-byte components, 4,096-byte comments and 4,096 keys per entry. A resource read is limited to 128 MiB. The backing file is reopened and its range checked for each read; changing a mounted archive is unsupported. No claim of authenticity or same-size change detection is made.

## DAT v56 Structural Reader

`engine/world.cpp` accepts version **56** only. It validates declared sections, counts, object record lengths, property lengths/types, duplicate property names, finite interpreted numeric fields, BSP chunk offsets and the final 24-byte bounds record. Parsing fails atomically: the caller receives an empty `World` and an error, never a partial success. A failed reload preserves the current inspection world.

Objects use the actual property count, not a byte scan. Types 0–7 are retained with their original type and flags: string, vector, color, real, flags, bool, integer and Euler rotation. Integer/flags payloads are floats on disk and must represent an exact signed 32-bit integer. Booleans must be 0 or 1. Rotation stores three Euler angles plus a reserved float; it is not a quaternion. `EulerAngles` preserves the source angles for `GenericProp.m_Vec`, and supplies yaw × pitch × roll rotation for `GenericProp.m_Rotation`. No engine-facing quaternion type was repurposed.

BSP output includes model names, flags, raw bounds/translations, texture names, planes, surface bases/indices, polygon point indices and positions. Coordinates remain in original units and handedness. Surface/plane/texture/point indices are checked. The two vertex-count bytes are added without narrowing to an 8-bit value. Geometry budgets are 2,000,000 counted elements per world, 65,536 points/polygons/leaves/portals per model, 4,096 additional models, 65,536 world objects and 1,024 properties per object. The input is limited to 128 MiB; count checks occur before count-dependent allocations.

This is **not a complete DAT decoder**. Header extension bytes, node records, visibility records, unused surface payloads and per-model lightmap/auxiliary tails are bounded but not interpreted. No BSP graph traversal or collision claim follows from bounding their storage. The 12 bytes following the extra surface basis vectors contain non-float bit patterns in the private level; treating them as an RGB float vector incorrectly rejects valid data. They remain opaque. Unknown versions/types and unsupported structural variants fail explicitly.

The inspector draws a Z/X wireframe from raw model points, with a selected source-object marker. It does not establish world-model placement/transform parity, polygon solidity, correct shading, texture loading or a gameplay camera. Its 40,000-edge preview budget affects drawing only.

## Original Objects and Session Boundary

The resource request follows the selected load-command portion of original `CBloodClientShell::StartNewWorld` and the field-reading order of `CBloodServerShell::LoadWorld`: `CMSG_LOADWORLD` (108), filename, game type, load type, difficulty. The local transport uses an owned bounded buffer and an explicit length-prefixed string; it does **not** implement the retail network ABI. Only fresh single-player requests are accepted. The complete client/server shells have not been imported. UI/audio/save/restore/player methods are not stubbed as successful operations.

The inspection harness parses the selected world, applies game settings, then creates registered classes using original `MID_PRECREATE`/`MID_INITIALUPDATE`. Each source object retains all its properties and an explicit disposition. Registered objects with invalid properties fail creation; unregistered runtime classes remain visible as missing. Original `CF_NORUNTIME` light/preprocessor classes are classified separately. The `GameSettings` editor group is accepted from the original B2 registration.

New original imports are `GameStartPoint`, `PathPoint` and the two named-trigger helpers. `GameStartPoint` preserves the source name internally, renames its engine object to `Blood2StartPoint`, reads team/rotation/trigger data and releases its strings. `PathPoint` preserves its original path-model/skin requests, dimensions and SMELL timeout/removal behavior. Named-trigger dispatch and REMOVE use the original helper flow and M1 message/list/lifetime services. Model/skin requests are indexed or reported missing, not silently described as loaded.

`SetObjectDims` stores validated finite nonnegative half-dimensions; it does not implement collision. `GetNextObject` enumerates active live objects in stable slot order without allocating a snapshot for each step; inactive entries are skipped and a dead predecessor returns null. See [path-manager contracts](path_manager_contracts.md) for the active-list correction. `SetObjectUserFlags` stores the original user flags. Object simulation and teardown run independently of the inspector. All original instances are destroyed while the adapter/string storage still exists. Generation-checked selection cannot refer to an object from a previous load.

`SessionState` is `Empty`, `Inspection` or `Blocked`; there is deliberately no reachable `Running` state. `run_world` fails explicitly because the full shells, required classes, player association, collision and animation are absent. Loading an inspection world is not starting the game. `--inspect-world` returns 0 for successful inspection, `--start-game` returns 6 for the missing runtime services; parser/resource failures return 5. Existing M0/M1 exit codes remain unchanged.

## Service Evidence

The inspector's Diagnostics tab counts typed adapter services reached during world creation, object updates and teardown. UI property display is outside this tracing interval. Named targeting/trigger transport and enumeration additionally have synthetic contract tests; they are not exercised by a playable retail session yet. Counters describe boundary calls, including helper calls such as `GetPropString` → `GetPropGeneric`, rather than a complete server/client-shell execution trace.

| Service family | Current state |
| --- | --- |
| REZ lookup/read, DAT structural decode | Implemented; synthetic corruption tests and private `01_SubwayA` tested |
| Original object property/lifecycle/string calls | Implemented and exercised by 17 retail-level instances |
| Named targeting, trigger transport, links/lists, enumeration | Implemented; synthetic original-helper/M1 tests |
| Dimensions and user flags | Stored/inspectable; dimensions are not a collision solver |
| Full shell entry/update/exit and player association | Unsupported; session startup fails |
| Required world classes, world-model attachments/motion | Mostly unported; every source entry remains visible |
| Collision/movement, ABC/skeleton/animation/string events | Unsupported; no success-shaped facade |
| Textures, sound, model resource payloads | Indexed/referenced only; no decoder or playback claim |
| Save/restore, multiplayer, expansion, VR | Outside this implemented subset |

## M2 service continuation

The [model/collision contracts](model_and_collision_contracts.md) supersede the
initial increment's absence of ABC decoding and collision services. Current
SubwayA accounting is 18 original instances, 372 preprocessor-only entries and
190 unported runtime objects. WorldProperties is now original executable code.
Primary-world collision and model playback/deformation are implemented within
the stated limits; complete shells/CPlayerObj and moving world models remain open.

The [diagnostic 3D view](world_view_contracts.md) supersedes the absence of any world
rasterization: existing player/scripted cameras now drive depth-tested geometry and
letterbox presentation. Materials, lighting, models/FX and complete M2 remain open.

The [DTX continuation](texture_contracts.md) supersedes the absence of world textures: opaque unlit base colors now render, with named rejection diagnostics and bounded cache ownership. World vertices are now 32 bytes; the three-million-vertex payload limit is 96 MB. Earlier geometry limits, current transforms and the M2 gate remain unchanged.

The [destination preparation continuation](destination_contracts.md) shares const property validation and structural CPU preparation with fresh loading.
Blocked exits now perform read-only destination lookup after refusing transfer; earlier blanket resource-lookup omissions are historical.
No destination callbacks run and no active session is discarded; safe automatic NEW_LEVEL and M2 remain incomplete.

The [baked lighting continuation](baked_lighting_contracts.md) keeps each polygon's per-vertex baked RGB in `Polygon::colors` (parallel to points, empty for generated geometry). The polygon prefix and model tail remain opaque.

The [lightmap continuation](lightmap_contracts.md) reads surface lightmap axes, polygon lightmap extents and the trailing per-polygon RGB565 lightmap block, which is accepted only when exactly one offset validates. The bytes before the block stay opaque (`opaque_bytes`).
