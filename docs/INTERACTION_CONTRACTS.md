# Player USE and rotating doors

> Extended by [BRUSH_CONTRACTS.md](BRUSH_CONTRACTS.md): RotatingBrush and
> selected destructible brushes are connected. Earlier omissions/counts below
> describe their previous increments; the full M2 gate remains open.


This extends [TRIGGER_CONTRACTS.md](TRIGGER_CONTRACTS.md),
[DOOR_CONTRACTS.md](DOOR_CONTRACTS.md) and
[MOVEMENT_CONTRACTS.md](MOVEMENT_CONTRACTS.md). M2 remains open.
[Source selections](M2_INTERACTION_SOURCES.json) separate original game rules
from replacement engine services; [validation](../../Documentation/Implementation/M2_INTERACTION.md)
records the exercised scope.

## Original USE branch

`CPlayerObj::OnCommandUse` selects the ordinary `OnCommandOn(COMMAND_USE)`
branch from PlayerObj.cpp. It casts **82 original units**, from the current
server position plus the current eye level, in the pitch/yaw forward direction.
It sends the owned original `TRIGGER` string through `MID_TRIGGER` to the nearest
hit object **unless that object inherits Door**, including RotatingDoor.
A SelfTrigger is a separate nonsolid target and can therefore relay USE to its
parent, including the existing consuming key query. There is no direct-door
fallback, extended reach, target cycling or bypass of an occluding object.

The typed `PlayerUse` message validates the local client/player association and
finite bounded angles. The source server-shell command callback is replaced by
that checked local dispatch. Client `UpdateUse` detects the rising edge; holding
USE does not repeat. The application binds physical **E**, enabled with **F2**,
alongside the existing movement/look controls. Focus loss clears keys; text
editing/disabled controls suppress USE until a held E is released. Keyboard
repeat cannot revive a held USE key after focus loss. Hiding the inspector with
F1 does not disable input or simulation. USE runs after the shared MoveMgr body
and camera update, so it uses this frame's position, stance and look direction.

`SegmentObjects::All` includes nonsolid objects, unlike weapon queries. Container
and world-model rays use their actual shells; ordinary objects use their AABBs.
Pending objects are skipped. Equal-distance ties keep the primary world first,
then the stable object enumeration order. The player is explicitly excluded.
Enclosing objects do not produce an interior-start USE hit: later LithTech's
`fullintersectline.cpp` makes these hits opt-in with
`CHECK_FROM_POINT_INSIDE_OBJECTS`, which the original USE query does not request.
This source-informed engine adaptation is not a verified retail ray/BSP replica;
inside-world-model exit-face behavior is not implemented. Weapon ray semantics
are unchanged.

The original movement-block guard is retained. Multiplayer respawn, slow-death
rescue, creature use, menus and saved-game restoration remain outside the selected
session. A blocked or rejected USE does not tear down an otherwise valid client.
Synchronous callbacks can remove targets or the player; destruction remains at
the frame boundary and subsequent client work revalidates the association.

## Original RotatingDoor rules

RotatingDoor inherits the existing Door state machine, commands, scheduling,
SelfTrigger ownership, lock relay, sound-request metadata and desired portal
state. Its original defaults override BoxPhysics to false and PushPlayerBack to
true, and add RotationPoint and RotationAngles. Speed/ClosingSpeed are converted
from degrees to radians. Authored Rotation retains its Euler radians; the three
RotationAngles components are degree offsets from those closed angles.

The selected source preserves several unusual details:

- `CalcAngle` contains **two identical increments** per update. Both remain,
  including the possible one-update overshoot and later endpoint clamp.
- Its asymmetric direction test compares both branches with **positive one
  degree**; it is not replaced with a symmetric epsilon test.
- Zero angular span has a defined endpoint instead of source `0/0` arithmetic.
- Door's `StartOpen` only swaps/translates its positional endpoints. RotatingDoor
  does not swap its angular endpoints or initially turn through RotationAngles.
- `DoRotation` computes position as `RotationPoint + rotation *
  (original_position - RotationPoint)`, using the absolute Euler rotation.
  Authored initial rotation can consequently request a first-update positional
  change. The service interpolates that displacement without an unchecked jump.
- An unsuccessful transform leaves all angles, positions and geometry unchanged;
  it cannot report an open/closed endpoint. The door retries on later updates.

The new virtual `Door::Move` dispatch replaces the source's Opening/Closing
virtual methods without changing the shared Door callback/aggregate chain.
Nonfinite parameters, mismatched current rotation, excessive transforms and
unsupported restore creation fail explicitly. Nonidentity quaternion-only
Rotation properties without equivalent stored Euler angles reject at creation;
retail DAT Euler properties are supported. No property-name alias is guessed.

The original player `ObjectTouch` opening-door branch records a **-500 unit/s
body-forward velocity** when PushPlayerBack is enabled, once while pending.
The local equivalent of the source's added-velocity update is applied before
MoveMgr, then consumed once; it is not another position integration. Live respawn
clears it. PushPlayerBack does not disable ordinary kinematic body/rider transport.
The old force-ignore limit and crush notification protocol are not reconstructed;
typed atomic blocking replaces the unsafe rotate/move/rollback sequence.
Rejected transactions emit no touch or speculative push impulse.

## Replacement rotation collision

This is new engine code, **not recovered or verified retail physics**.

The checked authored shell and its initial rotation remain immutable. Each pose
is rebuilt from that basis with `desired * inverse(authored)` rotation and its
current pivot, avoiding accumulated vertex drift and double rotation. The posed
shell, triangle BVH, actual bounds center and dimensions commit together. Rays,
body movement, clearance, support and the inspector read that same pose. Static
containers continue rejecting runtime transforms.

The service follows the unwrapped Euler path, including multi-axis/large-angle
requests, rather than choosing a shortest quaternion arc. A vertex-travel bound
subdivides it into at most 0.5-unit steps. Swept envelopes conservatively detect
potential contacts; bodies keep axis-aligned dimensions. Riders are identified
by fresh support, honoring DONTFOLLOWSTANDING and jump detachment. Pure yaw has
no vertical envelope inflation that would spuriously drag a detached top rider.
Potentially contacted bodies follow the rigid displacement, with at most eight
bounded forward separation attempts for AABB corner growth. An unresolved
contact blocks the entire request.

Each proposed body path is checked against primary geometry and other old scene
positions; staged body positions are also checked against each other. Other
world models remain immovable, using conservative bounds. GOTHRUWORLD lets the
door enter its wall pocket. No chain pushing, crushing damage, mass/impulse
solver or retail blocking priorities are claimed. Conservative envelopes can
push slightly early or reject otherwise feasible moves.

Limits are **512 subdivisions, 262,144 triangle-steps and 64 affected bodies**
per transform. Exceeding geometry/time work limits returns Unavailable; the
requested angles do not advance. The implementation copies/rebuilds a bounded
pose/BVH per subdivision; broad-scene performance still needs profiling.

All positions commit before original mover/body touch callbacks. Transported
bodies sweep nonsolid sensors along their accepted subdivided paths; each sensor
receives at most one contact per body/transaction. Handles are rechecked after
callbacks, removal stays deferred, reentrant motion fails. Direct sensor
crossings by the door mesh itself remain pending. The local player still has
one MoveMgr integration and one same-frame camera/weapon update.

## Inspection and remaining gate

**Session** shows USE command/message counts, the last ray endpoints, target,
distance, result, direct-door refusal and pending/consumed door push requests.
**Doors** includes hidden/inactive RotatingDoor instances, posed geometry, hinge,
Euler angles, authored angular offsets, angular speeds, subdivision count,
blocker, push/rider counts and SelfTrigger state. **World** retains every rejected
source entry and reason. Inspector TRIGGER buttons remain direct diagnostic
commands; they do not pretend to enforce the player ray's reach/occlusion.

RotatingBrush, trains/path movers, complete AI/player/combat callbacks, generic
pickups, audio playback, portal visibility, save/restore, world transitions and
other platform/XR validation remain pending. Successful inspection and this
interaction subset do not close M2 or make the game playable.
