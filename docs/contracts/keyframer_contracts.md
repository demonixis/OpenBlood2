# Original keyframed paths

The later [key audio contracts](keyframe_audio_contracts.md) supersede the request-only
sound statements below: retained attached loops/one-shots now play, with source
replacement and destruction rules. Print presentation remains unavailable.

> The later [train path contracts](worldpath_contracts.md) supersede blanket
> world-path/without-GOTHRUWORLD omissions below for static BoxPhysics translation.
> Angular paths and moving containers remain unavailable; M2 stays open.

M2 remains incomplete. Read [object](object_contracts.md), [camera](camera_contracts.md),
[collision](model_and_collision_contracts.md) and [interaction](interaction_contracts.md)
contracts with the [source selections](../implementation/sources/m2_keyframer_sources.json) and
[implementation evidence](../../../Documentation/Implementation/m2_keyframers.md).

## Original sequencing

KeyFramer retains original ObjectName/BaseKeyName/StartActive/Looping defaults,
world-only property reads, B2BaseClass filtering/callback forwarding, saveable user
bit and delayed first initialization. Runtime/string-created controllers keep
source defaults; restore is rejected. Save/load and sound caching remain absent.
The first update snapshots exact-name targets and collects BaseKeyName0, 1, 2,
... until the first missing number. It selects the first live Key-derived match,
skipping same-named objects of other classes. A duplicate or a key after a gap
is not consumed. Target lookup is one-time: later spawning a previously missing
class does not silently repair the path.

Original KeyData::Copy values are owned before their source Key objects are removed
through deferred lifetime. Every copied Key is a position key; nonempty sound,
paired target/message, and print values retain the source event conditions.
Timestamps accumulate relative to the previous key, not absolute level time.
The first initialization update does not advance path time. ON/TRIGGER before
initialization is ignored; ON while active does not restart. OFF disables updates;
a later ON restarts time/current key but retains original offsets. Source Pause
clears active; Resume sets it without rearming an already-disabled schedule.

Active callbacks add the frame delta and process all crossed keys in order.
One loop at most is processed per update; overshoot is discarded when time resets.
Nonlooping completion does **not** snap to the last endpoint: the source returns
before motion after consuming the last key. Position is linear between current
and next keys. A duration at or below 0.001 uses the next pose. Rotation uses the
typed InterpolateRotation service. Target position offset and **quaternion component
difference** are captured relative to the first key and added back; no replacement
quaternion-composition rule is introduced. A CameraObj with a tracking link keeps
its tracking rotation while receiving path translation.

## Typed engine services and deliberate adaptations

InterpolateRotation validates finite, nonzero quaternions and t in [0,1], normalizes
inputs/output and uses shortest-arc slerp (normalized linear interpolation near
coincident rotations). Invalid input leaves output unchanged. This is replacement
engine math, not proven retail numerical parity.

MoveKeyframedObject validates the complete requested pose before movement. It
supports ordinary OT_NORMAL/OT_MODEL objects through existing MoveObject collision,
followed by normalized SetObjectRotation when requested. Collision may clip the
position; rotation still follows the source call order. The result distinguishes
reached, blocked/clipped, invalid/removed and unavailable. Reached counts are not
rendered-frame counts. Contact removal is checked before rotation; pending objects
cannot receive SetObjectRotation. Containers, world models, other object types and
client-controlled shared player bodies explicitly return Unavailable before any
pose mutation. Combined world-model translation/rotation requires a future atomic
geometry transaction; the existing door solver is not bypassed. Thus SubwayA's
happytrain path is inspectable but its brush motion remains unavailable.

Bounded vectors replace linked KEYNODE allocations and parallel legacy offset
arrays. Each target stores its handle, offsets and result together; MID_LINKBROKEN
removes that entire record, fixing the source's shifted-offset bug after removal.
Keys/strings are value-owned, with no legacy HSTRING lifetime beyond their source
or message dispatch. Discovery is limited to 1,024 contiguous keys and 256 targets;
self-targets reject. Nonfinite or abs(timestamp/cumulative time)>1,000,000 and sound
radius outside [0,1,000,000] reject. Finite negative timestamps keep source ordering;
they are not sorted or clamped. Invalid/over-limit discovery consumes no source keys.
A command revision guard ends the current callback after a reentrant state change;
self-removal ends it too. This deliberately prevents continuing a stale path after
an OFF/ON or deletion delivered by a key/contact callback. No destruction occurs
inside an object callback, and keys outlive their source objects independently.

Message keys use the existing original named-trigger helper, including REMOVE.
Inspection counts resolved live names before dispatch, not successful game effects;
zero matches are exposed. Allocation failure is a diagnostic. Sound keys retain
name/radius/loop data and count requests per live controlled target, but **do not
play sound** or fabricate a sound handle. Original replacement/kill timing awaits
real audio. Print keys retain their text/count for inspection; original broadcast
BPrint presentation is also unavailable. These requests do not satisfy M2 services.

## Inspection and validation

Keyframes lists hidden/inactive controllers and their initialization, activity,
time, schedule, next key, copied keys/poses/events, target offsets/actual positions,
attempt/result counts and unresolved messages. World keeps original source entries
and correctly shows consumed Keys as no longer live. Source entries created is a
historical load count, distinct from current live instances. The original camera
tab still exposes external view/player restrictions separately from world rendering.

Nine synthetic cases cover defaults/filtering, contiguous/duplicate key selection,
ownership and endpoint behavior, looping/overshoot/Pause/Resume, quaternion offsets
and linked cameras, target removal, owned messages/requests, reentrant OFF/removal,
invalid/unsupported poses and finite bounds, plus collision clipping/contact removal.
The private catalog creates all 94 controllers, 383 Keys and 61 cameras from the 57
readable installed worlds. It copies 375 Keys, leaves eight unconsumed and reports
44 controllers without targets in this **camera/key-only catalog**; this is not a
full-world missing-target count. It has no empty paths or malformed-path rejection.
The strict `16_bridge.dat` format rejection is unchanged.

Two actual SubwayA reload cycles verify first-update key consumption, numeric camera
path interpolation reaching the selected client view, missing Gideon script delivery,
the natural two-camera intro/release, and explicit unsupported train motion with
unchanged brush pose. `--keyframes-smoke` checks the authored first camera path with
UI hidden and after reload; the same simulation runs in an inspector-free build.
GPU lifetime snapshots now run one normal update before their initial count, so
reload compares the same post-consumption phase without relaxing equality checks.

CinematicTrigger, GlowingLight, Gideon/AI, keyframed world/container physics, sound,
world transitions, attachment/rendering coverage, persistence and remaining required
classes/services keep M2 open. No retail executable, other-platform or XR parity is
claimed. Existing SDL/dependency pins and commercial assets are unchanged.

Later [light](light_contracts.md), [translating world-path](worldpath_contracts.md)
and [cinematic/audio](cinematic_contracts.md) continuations supersede the blanket
omissions above for their bounded subsets. CinematicTrigger now uses real WAV
voices and dry SDL output; KeyData sound requests, reverb, angular/container paths,
general audio/rendering and complete M2 acceptance still remain unavailable.
