# M2 Model and Collision Services

The [M16/grenade continuation](assault_contracts.md) adds ID 9 and mode-specific ammo, original grenade callbacks and explicit finite key-bound metadata warnings. Earlier supported-weapon limits are historical; remaining services and M2 stay open.

> Extended by [weapon impact contracts](weapon_impact_contracts.md): original multi-hit
> material rules, selected impact audio and automatic impact hearing now execute.
> Earlier impact-audio omissions are historical; visuals and the M2 gate remain open.

> Extended by [character animation contracts](character_animation_contracts.md):
> original CAnim_Sound tables now drive locomotion; current model node queries and
> original sphere measurements are inspectable. AI/damage-localization callers
> and complete M2 remain unavailable.

The later [player entry contracts](player_contracts.md) supersede this increment's statements that shell/player entry is entirely absent. Complete gameplay remains blocked; earlier limits below remain unless explicitly superseded.

This continuation implements engine services and two original classes. It does
**not** connect the complete `CBloodClientShell`, `CBloodServerShell` or
`CPlayerObj`. The Caleb preview is an inspection resource, never a substitute
player object. `RunWorld` remains blocked.

## ABC v6

`engine/model.cpp` reads `MonolithExport Model File v6` through bounded,
little-endian section readers. Header, Geometry, Nodes, Animation and AnimDims
are required, unique and exactly consumed. The absolute section chain must move
forward and end with the final sentinel. Unknown sections/versions and nonzero
optional TransformInfo flags fail explicitly. There is no recovery by scanning
for plausible data.

The reader retains all vertices, the primary vertex count, progressive LOD
metadata, UVs, normals, face indices, node indices/parents/flags, animation bounds,
key times/strings, node transforms, packed vertex deformation and animation user
dimensions. Extra LOD selection and normal deformation are not implemented.
Skeleton poses interpolate translation and normalized quaternions in engine
coordinates, composing each child with its parent. The model-view continuation
normalizes ABC -Z forward to engine +Z at decode time, including bounds and
deformation; see [the conversion contract](model_view_contracts.md). Vertex-deformed nodes decode
`byte * scale + translation`, interpolate adjacent frames and apply the node
pose. Caleb's coat requires this path; treating its packed coordinates as rigid
vertices produces visibly broken geometry.

Limits include 128 MiB input, 6 sections, 65,536 vertices, 200,000 faces, 256 nodes,
64 hierarchy levels, 4,096 animations, 65,536 keys per animation, 3 million node
poses and 32 MiB packed deformation. Coordinate magnitudes are limited to 10
million original units. Counts are checked against remaining bytes before their
loops. Referenced nodes/vertices and deformation ownership are checked. Repeated references within a node retain source write order (Caleb has 189 references to 120 distinct coat vertices, sometimes with differing samples);
nonfinite floats, zero rotations, invalid bounds and decreasing/out-of-range
key times fail. Output is empty on parse failure.

Models are read through the existing resource resolver. The weak cache uses the
resolved archive/range, so patch overrides cannot retrieve a base-archive model.
Objects hold shared immutable model data. Loading occurs **after PRECREATE and
before INITIALUPDATE**, allowing original initialization to request animation
indices. Inspection retains an original instance when its requested asset is
missing, with an explicit `model_error`; model APIs cannot report successful
playback on that record. Unload destroys objects and releases preview/cache data.

## Typed original animation interface

| API | Implemented behavior |
| --- | --- |
| `GetAnimIndex` | Exact clip name; `UINT32_MAX` for missing name/model |
| `SetModelAnimation` / `GetModelAnimation` | Validated index; changing clip resets its clock, assigning the same clip does not |
| `ResetModelAnimation` | Restarts the current clip and its first-frame events |
| `SetModelLooping` / `GetModelLooping` | Looping defaults to true; reset separately after completion |
| `GetModelPlaybackState` | `MS_PLAYDONE` for completed nonlooping clips; zero-duration clips emit their keys once and complete |
| `GetModelAnimUserDims` | The actual AnimDims vector, not geometry bounds or guessed player dimensions |
| `MID_MODELSTRINGKEY` | Original BaseClass/aggregate dispatch with a borrowed, synchronous `ArgList` when `FLAG_MODELKEYS` is set |

Playback includes keys at time zero, crossed keys, the terminal key and loop
boundaries in order. It runs without any renderer or inspector. Empty key strings
are ignored. Quoted arguments and semicolon-separated commands are supported;
malformed or excessive commands fail explicitly. Changing/resetting animation or
removing an object inside a callback cancels the remaining events for that old
track. Destruction is deferred until callbacks return; recursive stepping/world
unload is rejected. A step is limited to 60 seconds and 4,096 events/loops; an
invalid or over-budget pure track step leaves its state/output unchanged.

The session advances object callbacks, then physics, then model callbacks. Engine
time is the enclosing session-frame time; event callbacks do not invent subframe
`GetTime` values. This ordering is explicit and tested, but has not been compared
with the retail executable. Pose interpolation is normalized linear quaternion
interpolation; blending/transitions, attachments, node-query APIs, animated
normals, skin/texture decoding and final model rendering remain open.

## Primary-world collision and movement

`engine/collision.cpp` builds a median-split BVH over fan-triangulated solid
polygons of the **primary** DAT world model. It retains source polygon indices.
`SURF_SOLID` bit 0 is used; the 115 additional models in SubwayA are not silently
placed using unverified transforms. The input is convex BSP polygons from the
reader, not arbitrary concave editor meshes. Degenerate triangles are excluded.
The index is capped at 2 million triangles and rejects invalid references and
coordinates. A failed build clears the index.

Continuous swept AABBs use separating-axis intervals against triangles; a point
query is a zero-size box. Broadphase queries allocate no temporary geometry.
Tests cover high-speed crossing, the shared diagonal between triangles, touching
surfaces, movement parallel to/away from a face, initial penetration, missing
solid flags and invalid inputs. Dynamic objects currently use axis-aligned boxes.

`MoveObject` is connected to this index, clips displacement, slides over up to
four contacts, removes inward velocity and records the last normal, source
polygon/object and grounded flag. Touch-notify objects receive typed original
callbacks along the travelled path, including nonsolid triggers. Callback-driven
removal is deferred. An unloaded world, unsupported moving world-model type,
recursive move, invalid destination or initial penetration fails explicitly
(overlaps of at most 0.01 units are now tolerated; see
[contact overlap contracts](contact_overlap_contracts.md)).
`TeleportObject` intentionally bypasses collision and resets contact state.

Velocity/acceleration accessors and global force are real storage/services.
Active objects with velocity, acceleration or gravity integrate with at most
1/120-second substeps and a 0.5-second catch-up limit. No stationary object is
forced into that loop. This is an initial engine service, **not verified original
player physics**. Missing behavior includes moving/rotating world-model collision,
standing-object following, depenetration, crush/impulse response,
containers, friction/mass, original BSP contents and legacy world-polygon handles.
Object touch callbacks currently carry zero stopping force; world contacts are
inspectable but are not dispatched with a fabricated world HOBJECT. Legacy
`IntersectSegment`, `GetStandingOn` and `GetLastCollision` remain absent.

## Original classes and world settings

- `WorldProperties` retains its original property-to-console-variable and global
  force logic, inactive state and destructor cleanup. Registration is typed.
  Its text properties allow 499 bytes, matching its original 500-byte read
  capacity; other property limits remain unchanged. `GetPropString` copies the
  typed string directly, while an overlong `GenericProp` fails instead of silently
  truncating. Stable console-variable handles store strings and parse numeric
  values without locale-dependent conversion.
- `SmellHint` retains original linked-list membership, 20-second scheduling and
  removal. Its link starts initialized, including rejected creation/teardown
  paths. Selected DLink helpers come from the original SDK. Tests verify expiry,
  unload/reload, list lookup and difficulty-filtered destruction.

## Inspection and validation

Objects shows decoded model status, animation time and motion/contact data.
Models exposes Caleb's animations, dimensions, string markers, node hierarchy,
skeletal/vertex-animated wireframe and preview controls. Services shows collision
index counts, gravity and original console-variable values. Preview controls
change only the labelled preview, never game rules. Hidden UI still advances
original services and the preview. GPU unload/reload verifies that model and
collision resources are released/rebuilt without stale object selection.

See [continuation results](../../../Documentation/Implementation/m2_services.md) for
actual private-data counts, build/test evidence and remaining integration work.

## MoveMgr continuation

[Movement contracts](movement_contracts.md) now specify the selected original
controller. Client-associated player records skip generic integration: MoveMgr
substeps drive a single shared body through typed CPlayerObj movement messages.
Its fresh support query distinguishes uphill motion from motion away from the
ground plane, with real world-polygon or live object identities. Walking, jump,
gravity, wall/ceiling sliding and stable gentle-ramp support are tested. Containers, moving-platform following and full retail physics parity remain open.

## Locomotion extension

[locomotion_contracts.md](locomotion_contracts.md) supersedes the previous
fixed-body/stairs limitations: original player clips now drive checked shape
changes, and grounded FLAG_STAIRSTEP movement uses an up/forward/down sweep.
The local GetMovementState support query and weapon IntersectSegment are also
implemented; this does not provide every original PhysicsLT query or moving BSP
collision. Keep source rules and replacement-solver limitations distinct.

## Sprite resource continuation

[Sprite contracts](sprite_contracts.md) add bounded SPR manifests and per-instance
frame clocks to the existing active-object animation traversal. No model events
are synthesized for sprites. CastWorldRay uses the solid primary-world index for
original DetailSprite flush placement; dynamic objects are excluded by its source
query flags. DTX decoding/rendering and verified retail ray/timing parity remain open.

The [AI visibility continuation](ai_sight_contracts.md) adds selected original CanSee
and a typed ray object filter. Default weapon/USE ray behavior is unchanged.
Visibility queries do not implement target selection, AI states or complete M2.

The [animated model continuation](model_view_contracts.md) supersedes earlier model/skin rendering omissions for existing ABC/held/view models. It also corrects the absent-TransformInfo quaternion default at the reader boundary, shared by all pose/node/attachment queries. Explicit zero remains direct and nonzero flags remain unsupported. Close-view projection is replacement presentation; required gameplay/FX and M2 remain open.

## Destructible model continuation

[Destructible-model contracts](destructable_model_contracts.md) add original authored
model dimensions and a separate typed model-body flag reusing bounded box motion.
Models do not become character bodies or world meshes. Real ceilingfan2.abc has an
empty animation name; the reader preserves that name and its ordinal while retaining
all structural, key-count, pose and payload validation. This is not an absent clip
or a synthesized bind pose. Required services and M2 remain incomplete.

The [stopping-contact and falling-damage continuation](fall_damage_contracts.md)
now supplies accepted ordinary-movement impulses, original player falling damage,
engine mass/friction and saved server-falling state. Earlier blanket omissions are
superseded only for this boundary; dedicated mover/body/projectile forces, retail
physics parity and the M2 gate remain open.
