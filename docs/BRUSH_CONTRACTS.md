# Rotating and destructible world-model brushes

> Later extension: [pickup contracts](PICKUP_CONTRACTS.md) supersede blanket
> pickup/SpawnObject omissions below for eight ammo and four health/armor classes.

> Extended by [PUSHABLE_CONTRACTS.md](PUSHABLE_CONTRACTS.md): selected Pushable/Gravity brushes and
> brush damage impulses now work with bounded box physics. Omissions and counts
> below describe the earlier stage; polygon dynamics and full M2 remain open.

> Extended by [EXPLOSION_CONTRACTS.md](EXPLOSION_CONTRACTS.md): original CDebris requests and explosive
> brush area damage are now connected. Earlier omissions/counts below are historical;
> WorldModelDebris, pushable brushes, client FX/audio and the full M2 gate remain open.

This extends [INTERACTION_CONTRACTS.md](INTERACTION_CONTRACTS.md) and
[WEAPON_CONTRACTS.md](WEAPON_CONTRACTS.md). M2 remains open. See
[source selections](M2_BRUSH_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_BRUSHES.md).

## Original rotation rules

`RotatingBrush` inherits the selected `CDestructableBrush` lifecycle and owns the
original `Rotating` aggregate. Its callback goes through the parent and the
BaseClass aggregate chain once. Initial update binds the aggregate to the checked
object handle before the parent disables its timer; the aggregate then arms its
own 0.001-second update if StartOn is true. Dispatch still occurs at most once
per session step. Hiding the inspector does not affect simulation.

The property schema retains the original defaults and editor groups. Rotation
properties are read on world creation only, as in `Rotating.cpp`; string/runtime
creation retains the aggregate's off/zero-speed defaults. The brush parent reads
its own properties for world and string creation. Positive AxisRevTime becomes
2*pi/time radians per second; zero/negative values disable that axis. Direction
comes from each RotateForward property. Spin-up/down durations and sound requests
remain separate per source behavior; no audio playback is claimed.

- ON starts spin-up only while off. OFF starts or restarts spin-down while not
  off. TOGGLE starts spin-up while off and spin-down in every other state.
  TRIGGER, including a direct player USE, is not an ON alias.
- Each axis subtracts the current frame time before computing its ramp fraction.
  Reaching exactly zero remaining time retains the ramp state until the next
  update. Nonpositive durations take the source immediate-completion path;
  a zero-duration/zero-delta division gets an explicit finite guard.
- Normal rotation integrates `GetTime() - last_time`, not just the frame delta.
  Spin-up does not reset last_time. A long off/inactive interval can therefore
  produce a large first request after restart. This source behavior is retained.
- Source pitch/yaw/roll start at zero. Authored Rotation remains the actual initial
  pose, but the first moving update targets angles accumulated from zero. The
  engine sweeps that first pose change instead of silently offsetting the angles.
- Spin state/timers advance even if collision blocks a request. Angles/geometry
  advance only after Moved. last_time updates on a rejected move, as it did after
  the source's unchecked RotateObject, preventing an ever-growing retry.
- Spin-down reaching off retains the source extra scheduled update; the next
  off update stops rearming. There is no background fixed-rate rotator loop.

The rotation request uses the current object position as its pivot. It does not
orbit an external hinge. Finite values, representable rates, stored Euler/current
quaternion agreement and existing geometry budgets are checked. Excessive travel
returns Unavailable with unchanged geometry; no wrap-to-shortest-arc shortcut is
introduced. Quaternion-only nonidentity creation without equivalent Euler data
rejects, as for RotatingDoor.

## Collision and lifecycle

The existing atomic rotating solver now accepts rotation about the current
position without GOTHRUWORLD. Later LithTech `RotateWorldModel` also rotates at
the object position without requiring that flag; this is supporting engine
reference, not proof of Blood II executable parity. Orbital/translated requests
still require GOTHRUWORLD. The mover mesh itself may overlap primary-world
geometry during pure rotation; transported bodies must still clear the world.
Other world models remain conservative immovable blockers.

The immutable authored shell, current posed shell/BVH, rays, dimensions, support,
body/rider transport, 512 subdivisions / 262,144 triangle-steps / 64 affected-body
limits and deferred callbacks retain the interaction contracts. BoxPhysics uses
the existing conservative AABB solver; it does not replicate the later engine's
box-rotation collision shortcut. No crushing or push-priority parity is claimed.
Static containers still reject transforms. Generic physics never integrates a
world-model brush or integrates the local player a second time.

Rotation and damage remain owned aggregates, with stable handles and deferred
reclamation. A destruction update marks the brush pending before forwarding to
aggregates; Rotating checks liveness and cannot move the pending brush. Global
CDestructable list membership is removed exactly once, including unload and
creation rollback. Rejected precreate is now a **failed** source entry; only the
explicit original B2 difficulty/game-type marker produces a filtered entry.

## Destructible brush subset

`CDestructable` was extracted from PlayerObj into shared source files, retaining
the existing player health/armor/resistance branch and list ownership. Added
original branches read CanDamage, TriggerDestroyOnly and damage/death target
messages; KILL uses typed Death damage with a live sender. Per-update cumulative
damage resets follow the source update callback. Named death messages run before
named damage messages. A recursion guard and liveness checks prevent a death
callback or REMOVE relay from recursively damaging a pending object.

The brush starts at authored HitPoints and ObjectMass, binds its named world
model, preserves BoxPhysics/FireThrough/AllowMarks and surface user flags, and
schedules its removal on the next update after death. **Destructable** controls
brush removal; **CanDamage** controls health damage. These are distinct source
properties: Destructable=false can leave a dead brush present, whereas
CanDamage=false prevents KILL from reducing health. TriggerDestroyOnly allows
Death but rejects ordinary damage. Damage and death remain selected branches:
character reactions, special damage modifiers, impulses and complete retail
CDestructable semantics are not newly claimed. The pre-existing dead guard also
suppresses repeat Death notifications; CanDamage=false does not accumulate
otherwise unused hit metadata in this subset.

Weapon fire-through queries can skip a brush with FireThrough, using the same
bounded adaptation already used for Door. Ordinary rays, USE and movement still
hit it. This does not implement full glass penetration, material effects or
source projectile continuation.

The separate original CDebris aggregate and WorldModelDebris are **not imported**.
Their authored properties remain inspectable, including the world-only byte
conversion of Amount. Removal records an owned diagnostic (name, position, last
direction, cumulative damage, amount), retaining the latest 32 until world unload.
This is a destruction request, not rendered debris, spawned fragments or sound.
The source surface fallback through debris materials and decals remain pending.
Pushable/gravity brushes, Exploding=true and nonempty SpawnObject reject creation
because their missing branches affect gameplay. Save/restore creation rejects.
No missing/ambiguous model is replaced by arbitrary geometry.

## Inspection

**Brushes** lists visible, hidden and inactive live brushes, draws their posed
shell, and exposes state, radians, angular velocity, ramp time, motion result,
subdivisions, blocker, pushes/riders, sound requests, health and damage gates.
ON/OFF/TOGGLE/KILL diagnostic buttons queue original messages for the next frame;
KILL uses the selected brush as an explicit diagnostic sender. Selection and
requests use generation-checked IDs. Destruction requests remain inspectable
after the brush disappears; World retains rejected entries and authored properties.

Other movers, destructible/rotating models and sprites, pushable brushes, FX,
explosions, SpawnObject, audio, portals, saves/world transitions, full player/AI
and campaign gameplay remain pending. No retail executable comparison or new
platform/XR verification was performed in this increment.
