# Original pushable brushes and bounded box physics

> Later extension: [pickup contracts](PICKUP_CONTRACTS.md) supersede blanket
> pickup/SpawnObject omissions below for eight ammo and four health/armor classes.

This extends [brush](BRUSH_CONTRACTS.md), [explosion](EXPLOSION_CONTRACTS.md)
and [movement](MOVEMENT_CONTRACTS.md) contracts. M2 remains incomplete. Read the
[source ledger](M2_PUSHABLE_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_PUSHABLES.md).

## Original game rules

Selected CDestructableBrush Pushable and Gravity branches are connected.
Pushable disables AllowMarks, forces FLAG_GRAVITY, sets blocking priority 40,
enables CDestructable damage physics and adds USRFLG_MOVEABLE (bit 4). Other
brushes retain priority 100. Gravity-only brushes simulate but cannot be pushed
by the player and do not receive damage impulses. SAVEABLE and surface bits retain
the previous source behavior; a flag does not implement save/restore.

Mass and friction follow original CDestructable::SetMass: friction is
`5 + mass * 10 / 500`. The original clamps its local parameter but then uses the
unclamped member in the formula; mass 750 therefore gives friction 20. Finite
mass in [0, 1000000] is accepted. The engine does not infer collision forces from
mass or send a fabricated force with a touch notification.

For enabled damage physics, directions shorter than 0.1 produce no impulse.
Otherwise normalize the direction, replace a nonpositive aggregate mass with 1,
and add `damage * 2500 / mass` along it to existing velocity. This occurs after
resistance and before armor, under the existing dead/CanDamage/TriggerDestroyOnly
gates. The special Death branch does not apply an impulse. Checked SetVelocity
rejects out-of-range results; inspection exposes acceptance. Player-specific
impulse accumulation/transport and trapped-character branches are still omitted.
Explosion damage can now impart impulses to eligible brushes through the same
ordinary damage path; it does not add a separate physics integration.

CPlayerObj uses the source priorities: 50 while supported and 40 while airborne.
Fresh local support and the submitted velocity replace the source network ground
bit. Jump motion loses ground priority before collision. Original MoveMgr remains
the only player integrator; generic physics skips client-associated players.

B2BaseClass still applies the original FLAG_DONTFOLLOWSTANDING. In particular,
ordinary brushes do not automatically become riders of other moving brushes.
The engine honors that flag instead of silently simulating rigid stacks.

## Replacement engine services

WorldBodyProperties, ConfigureWorldBody and SetBlockingPriority are typed local
services, not a recreation of the SDK function table. Configuration is one-time
and requires checked live, named closed world geometry, FLAG_BOXPHYSICS and no
FLAG_GOTHRUWORLD. Dynamic polygon shells and simultaneous rotating/gravity
brushes reject creation. SpawnObject still rejects. Missing/ambiguous models and
invalid shells remain failures. Changes that invalidate a configured body shape
are rejected; a static world model cannot acquire gravity without configuration.

The body translates its existing immutable shell; collision uses its axis-aligned
bounds. Central object_bounds_center handles authored pivots offset from geometry
for sweeps, support and volume membership. Ray and container queries see the
accepted pose. Rotation/kinematic mover entry points reject dynamic bodies.

Generic physics advances active bodies in steps no larger than 1/120 second,
with a 0.5-second catch-up ceiling. Unsupported time remains a failure. Free motion
uses `v*dt + a*dt²/2`, then `v += a*dt`. Supported walkable faces remove acceleration
into their plane. Ground drag uses the analytic exponential solution for constant
acceleration/friction; very small unaccelerated velocity sleeps. Liquid/conveyor
rules run once per substep; environmental damage runs once per session step.
Hidden bodies still simulate; inactive bodies do not. These are replacement
engine choices, informed by later-generation LithTech motion code, **not verified
retail Blood II physics**. No code from that later engine is linked.

A body sweep checks the primary world and live solid objects. Up to four sliding
legs remove inward velocity. Initial penetration blocks the entire move and zeros
velocity; the object remains inspectable. No depenetration, crushing, restitution,
rotational inertia or recursive chain pushing is implemented. A clipped movement
is a valid collision response, reported as blocked in inspection.

Supported ordinary riders follow the accepted piecewise path. Up to 64 are
validated before anything moves. A blocked rider, an attached world-model rider
or excess riders rejects the whole move. DONTFOLLOWSTANDING and jumping riders
are detached. Other riders' old positions can conservatively block a proposed
path; this is not a stack solver.

A higher-priority ordinary body may push one pushable box through a horizontal
side contact. Both complete paths must be clear before either position commits.
A later floor contact can be flattened only when fresh support and that contact
are both horizontal; the entire horizontal path is checked again. More complex
slope/corner pushes conservatively stop. Failed box pushes leave its pose unchanged;
ordinary actor collision can still advance to the obstruction. Character pushing
is positional and does not inject an invented impulse into the box.

Positions commit before touch callbacks. Snapshot handles, pending-object checks
and the movement reentrancy guard prevent speculative/duplicate initiator contacts
and destruction during dispatch. Removal remains deferred. Contact sensors see
accepted path segments only; riders and the pushing actor share one committed
movement. Complete replication, retail networking and save/restore are not added.

## Inspection and limits

Brushes lists visible, hidden and inactive bodies and shows mass, friction,
gravity, pushability, priority, pivot, velocity, acceleration, fresh support,
last motion result, integration/push/rider/block counts and last damage impulse.
World retains authored properties and rejected source entries. Existing diagnostic
commands and destruction/explosion records retain checked ownership and lifetimes.

Dynamic polygon collision, rigid stacks, recursive pushes/crushing, SpawnObject,
WorldModelDebris, player impulses, client FX/audio, full player/AI/combat, portals,
world transitions and save/restore remain pending. M2 is not a playable session.
No retail executable comparison, additional platform or XR validation is claimed.
