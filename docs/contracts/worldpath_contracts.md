# Keyframed translating world models

M2 remains open. This extends [keyframe](keyframer_contracts.md),
[door](door_contracts.md), [interaction](interaction_contracts.md) and
[movement](movement_contracts.md) contracts. See [source/engine provenance](../implementation/sources/m2_worldpath_sources.json)
and [validation evidence](../../../Documentation/Implementation/m2_worldpaths.md).

## Original controller, bounded new service

No original game rule changes in this increment. The existing selected KeyFramer
still collects/owns Keys, computes original position and quaternion-component
offsets, advances its timeline, dispatches messages, ignores early ON and returns
before the final endpoint snap. Its source MoveObject/RotateObject pair reaches
the checked MoveKeyframedObject service. The timeline keeps advancing when a move
is blocked; it does not wait, reverse or invent an arrival event.

The service now admits translating static world models with checked authored
geometry. A requested rotation must equal the current normalized orientation
(within squared quaternion distance 1e-12, allowing either quaternion sign).
A null rotation retains the existing orientation. Angular requests fail with
Unavailable **before** changing position. Dynamic/gravity bodies, containers,
shared client-controlled bodies and incomplete geometry remain unavailable.
Malformed poses/dead handles return Invalid; missing collision returns Unavailable.
No arbitrary SetObjectRotation/TeleportObject bypass is added.

The existing MoveWorldModel atomic pusher/rider solver is reused. Doors retain
GOTHRUWORLD, which permits their wall pockets. Other movers require BoxPhysics;
their actual bounds center (not necessarily their pivot) and half-extents sweep
against primary-world triangles. An interior start or obstruction before the
endpoint blocks the entire request. Polygon-vs-primary-world translation remains
unavailable. No flag is added to a source object to make a path succeed.
The immutable authored/posed shell is retained; only the checked translation
changes, and all rays/support/overlaps apply that same offset. No per-frame
geometry rebuild or new dependency is needed for this translation subset.

Solid bodies crossed by the mover and fresh standing riders retain existing
relative-sweep, DONTFOLLOWSTANDING and jump-detachment rules. The plan validates
all proposed body paths before committing mover/passenger positions. Other world
models are immovable conservative blockers; at most 64 bodies are affected.
Primary-world block diagnostics use a null blocker handle, distinct from zero
block attempts. Touch/sensor callbacks run only after an accepted commit. They
may request removal; destruction stays deferred, reentrant motion rejects and
the keyframe caller reports Invalid if its target becomes pending during contacts.
The original local player still has one MoveMgr integration and observes transported
body/camera positions in the same frame.

This is **replacement collision code**, not recovered retail world-model physics.
It rejects a whole blocked step rather than clipping the mover or applying crushing,
force/blocking-priority arbitration or chain pushes. That difference stays explicit.
Combined angular paths, moving containers, polygon world collision and model
attachments are not implemented by this increment.

## SubwayA observation

The original `htrain1` controls one `happytrain` CDestructableBrush with BoxPhysics,
no GOTHRUWORLD and no dynamic-body state. Its three keys are at times 0/3/5 seconds,
positions (-32,168,4912), (-32,168,2864), (-32,168,3376), all identity rotations.
The inspector/test sends normal ON after original first-update key collection;
this is a direct diagnostic activation, not proof of a complete reachable level script.

At 60 Hz, two complete load/enter/run/unload cycles each produce **297 accepted
moves and two blocked attempts**, followed by source completion with no final snap.
Both blocks meet primary polygon **1209**, normal (0,0,1), at path times approximately
3.0 and 3.01666 seconds; independent collision queries confirm the obstruction.
The train resumes on the return leg and ends at approximately (-32,168,3371.74).
This records the current engine result, not expected retail train parity. The initial
new test assumed every authored move would succeed; investigation replaced that
assumption with assertions for the actual independently checked obstruction,
unchanged rejected poses, resumed motion and original endpoint behavior. Existing
parser bounds, service failures, test timeouts and M2 startup gating are unchanged.

Class counts are unchanged: 160 created source entries, seven SelfTriggers,
372 editor-only, 48 unported, zero failed; 167/170 runtime instances before/after
player, then 158/161 after consuming nine Keys. Unported AI and other classes
could introduce additional blockers later, so this is not campaign completion.

## Inspection and verification

Keyframes includes hidden/inactive/pending controllers, queued original ON/OFF,
target result/counters, actual brush pivot/bounds, pushes/riders/last blocker and
a plan view of copied keys and current collision bounds. The overlay is diagnostic;
world rendering remains absent. `--worldpaths-smoke` verifies actual authored train
positions and collision rays with the UI hidden and after reload. The ordinary
camera `--keyframes-smoke` remains separate and retains its original assertions.

Eight synthetic tests cover translation/immutable shell/rays, primary-world atomic
blocking/descending rider, riding vs DONTFOLLOWSTANDING/jumping, side pushes and
blocked rollback, crossed sensors/reentrant removal, invalid/unsupported poses,
shared player transport and original timeline/endpoint/link lifetime. The private
SubwayA case checks each attempted pose and two full reloads. Existing keyframe
catalog assertions now require the train's first move to succeed instead of requiring
Unavailable. Debug, inspector-free Release and sequential Clang ASan/UBSan/leak
results are recorded in the evidence. Sanitizers remain sequential because the
prior weapons.private case has a narrow unchanged 90-second time budget.

Required remaining objects/services, CinematicTrigger, Gideon/AI, angular paths,
client FX/audio, transitions and attachments still prevent M2 completion. No retail
executable, additional platform or XR verification is claimed.
