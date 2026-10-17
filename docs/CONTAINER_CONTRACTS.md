# Container, liquid and ladder boundary

> [Compound-shell validation](COMPOUND_SHELL_CONTRACTS.md) now accepts independently closed components sharing edges; ambiguous/non-manifold components still fail. Geometry and authored BoxPhysics are unchanged.

> Extended by [character audio contracts](CHARACTER_AUDIO_CONTRACTS.md): original
> voice helpers, player sound keys and material/liquid footsteps now execute.
> Earlier blanket character-audio omissions are historical; AI hearing and M2 remain open.

> The [scene explosion continuation](SCENE_EXPLOSION_CONTRACTS.md#hot-query-allocation-adjustment)
> reduces hot-query copying to fresh snapshots of bound containers. Order,
> lifetime, membership and source physics rules are unchanged.

This extends [MOVEMENT_CONTRACTS.md](MOVEMENT_CONTRACTS.md) and
[LOCOMOTION_CONTRACTS.md](LOCOMOTION_CONTRACTS.md). Source selections are recorded
in [M2_CONTAINER_SOURCES.json](M2_CONTAINER_SOURCES.json); validation is in
[M2_CONTAINERS.md](../../Documentation/Implementation/M2_CONTAINERS.md).
M2's complete playable-session gate remains open.
The later [specialized volume extension](SPECIALIZED_VOLUME_CONTRACTS.md) adds
Conveyor, Damage and FreeFall, additional numeric damage types, and the original
player volume-damage grace. It supersedes those pending items and the narrower
damage-type list below; other limits remain.

## Original rules selected

`VolumeBrush`, `Water`, `Blood`, `Acid` and `Ladder` are original game classes
ported selectively from VolumeBrush.cpp/VolumeBrushTypes.cpp. They retain the
base/aggregate callback chain, registered source defaults, container codes,
named world-model binding, hidden state, ON/OFF/TOGGLE, current, viscosity and
volume damage. Strings are owned C++ values; there are no packet/vtable stubs.
The adapted precreate reader also accepts the typed runtime/property-string
path. Malformed or unsupported codes/types are rejected at creation.

Water/Blood/Acid use source viscosity defaults 0.5/0.8/0.5. Non-player bodies
receive the original inverted viscosity factor, liquid gravity and current
through synchronous `MID_AFFECTPHYSICS` with a typed stack-owned payload.
The original viscosity=1 endpoint skips damping (inverted factor zero); it is
preserved. Generic viscosity is discrete per physics substep, not a measured
retail frame-rate equivalence. Lights and volumes are not movable bodies.
Client-associated players skip generic integration entirely.

Volume damage runs once per session frame, separately from motion substeps,
using the source 0.1-second dose/cadence and same-time allowance for multiple
bodies. It reaches the ordinary original damage aggregate through owned typed
messages. Supported types are Normal (0), Melee (4), Suffocate (6), Death (7),
FreeFall (8) and Acid (14). Suffocation bypasses armor; resistance still applies.
The selected instant-death branch clears health and armor unless god mode blocks
it. Secondary combat effects/death flow remain outside this increment. Retail
CityHub Water stores DamageType=8 with Damage=0; this metadata is preserved,
not rewritten to the source registration's suffocation default. Acid does not
invent damage when the actual property is zero. Ladder's original override
bypasses generic volume damping and damage; its player's MoveMgr owns motion.

MoveMgr retains the first-liquid/first-ladder branches, additive active currents,
source linear water/ladder stopping, global-gravity suppression, pitch-relative
movement, surface swimming and the 0.75 jump impulse when leaving water.
Walking/running swim speed is 87.5/175 and ladder speed is 200/400. Space adds
upward acceleration; Ctrl adds downward acceleration when airborne in liquid
or free movement. Supported wading with the head above water retains ordinary
land movement. The original body animation selection remains active, including
crouch dimensions; no invented swim animation is selected (`DoSwimAnim` is
disabled in the source). Ordinary collision, clearance and stairs still use the
same shared player body.

`CPlayerObj::UpdateAirLevel` drains 100 air over 15 seconds with the head in
Water/Blood/Acid; FreeFall is exempt. Empty air causes 5 damage/second before
resistance. Air recovers at 100/2.5 per second outside liquid. Recovery is clamped
to 100 rather than preserving the source's possible final overshoot. This local
port uses MoveMgr's accepted frame duration (at most 0.1 seconds) and final head
membership for air, rather than a second independent integration clock.

## Geometry and typed queries

`ObjectRecord` owns an immutable shared `ContainerShape`; no pointer into a
replaceable World survives creation. Creation requires one unambiguous named
model. A missing/open/invalid shape produces a diagnostic and deferred cleanup,
not a fake box volume. Removal makes it invisible to queries immediately;
world unload destroys original objects before their services and resources.
Teleport, rotation, resize, dimension, velocity/acceleration changes and
solid/gravity flags cannot mutate a static volume into an unsupported moving
world model. Normal object movement remains available.

The DAT reader retains both base and extra serialized vertex indices and now
records the base ring length explicitly. Container topology uses that base ring;
retail extra indices often repeat it with additional edge subdivisions. The
other reader consumers retain their existing index data. Volume vertices are
already in world coordinates at `WorldTranslation`; rotation is applied around
that pivot, and creation uses that pivot for the object's position. Bounds are
computed from vertices, not the v56 bounds that include another translation in
the tested files. The source Pos property remains available for comparison.
Later LithTech world-model initialization/transform code informed this mapping;
its BSP node layout and physics implementation were not imported.

Build limits: 4,096 points, 4,096 polygons, 512 indices per face and 16 million
edge/vertex closure comparisons. Edge closure handles T junctions and welds
near-identical split vertices within 0.01 original units; query geometry is not
snapped. Invalid planes, face indices, transforms and non-convex individual
faces fail without partial output. Convex shapes use plane tests; closed
non-convex shells use oriented solid angles. Body overlap combines point contents
with the existing triangle/AABB BVH boundary query. Bounds only reject obvious
misses. Boundary tolerance is 0.001 units. Opaque DAT BSP nodes are not traversed.

`ContainerState` owns up to four body records plus total membership and a
separate head record, matching the original `MAX_TRACKED_CONTAINERS=4` limit.
Queries validate handles and exclude pending objects. Object-slot order is
stable and replaces the unavailable retail spatial traversal order. As in the
source, the first head volume is checked for Hidden; a hidden first volume
masks later head candidates. The first liquid/ladder body candidate also consumes
that category even when hidden. Current, gravity and Hidden are refreshed every
movement substep, fixing stale values after ON/OFF without membership changes.
Overlap order/parity with the retail executable is unverified.

## Inspection and remaining work

The Containers tab lists every live supported volume, including hidden ones;
World still lists missing, rejected and filtered source entries. It shows head
and bounded body membership, truncation, air, water/ladder/surface states,
current, gravity, viscosity, damage and pending fog/surface/key properties.
The front view draws actual volume triangles, the player box and eye. Trigger
commands and collision-checked player position edits are queued and handle
checked. F2 enables movement; F1 hides the UI without stopping simulation.

PolyGrid/sprite water rendering, underwater fog/color/audio, key query/removal,
AI liquid/ladder reactions, save/restore, conveyor/crane/minefield behavior,
separate Damage/FreeFall class registration and moving/rotating volumes remain
unported. A generic VolumeBrush can carry supported Damage/FreeFall codes; this
does not implement those specialized classes. The inspector labels effect
properties as requests. No full textured world or retail movement parity is
claimed, and no new dependency or ECS was introduced.
