# Original world-model debris

This extends [brush destruction](BRUSH_CONTRACTS.md), [box motion](PUSHABLE_CONTRACTS.md)
and [rotation](INTERACTION_CONTRACTS.md). Read the [source ledger](M2_WORLD_DEBRIS_SOURCES.json)
and [implementation evidence](../../Documentation/Implementation/M2_WORLD_DEBRIS.md).
M2 remains incomplete.

## Original callbacks

`WorldModelDebris` inherits `CDestructableBrush` and adds the original `Rotating`
aggregate. Precreate calls the parent first, then reads InitialVelocity and
GravityMultiplier for world/string construction. Rotation properties still read
only on world creation. INITIALUPDATE binds Rotating and forces it off before
forwarding to the parent/aggregates, overriding authored StartOn. Hidden fragments
are real checked objects bound to their own named closed DAT models.

Brush death performs CDebris creation, then CreateWorldModelDebris, then deferred
source removal. The original lookup visits `Name + "debris" + index`, beginning at
zero. Every matching WorldModelDebris is started; other classes with the same name
do not terminate numbering. The first empty list terminates traversal. Matching
remains case-sensitive. The adaptation releases the original leaked terminal empty
list, owns the parent name and bounds traversal at 65,536 indices / 100-byte names.
Missing or ambiguous geometry still fails creation; no mesh aliases are inferred.

Start calls original Heal(10000), starts spin-up, sets on, replaces **all flags**
with VISIBLE | GRAVITY | RAYHIT | SOLID, sets the authored initial velocity,
blocking priority 100 and a .01-second update. In particular it drops BOXPHYSICS,
DONTFOLLOWSTANDING, FULLPOSITIONRES and MODELGOURAUDSHADE. Repeated starts heal
and reset velocity/ramp again; they do not reset the rotation angles/last clock.
ON/OFF/TOGGLE remain Rotating commands, not aliases for Start.

While on, Update sets acceleration to global gravity times (multiplier - 1),
rearms .01 and queries current support. Support includes primary-world polygons
without fabricating a world HOBJECT. Any standing result clears on and starts
spin-down. The subclass's next off callback disables its own deadline; the
aggregate can still rearm until its ramp finishes. Landing does not clear gravity,
acceleration or horizontal velocity. It does not remove the fragment. Original
subclass-before-parent update order and once-per-step aggregate dispatch remain.

Failed typed gravity/support/start services suspend scheduling and expose an
error. Rotation retains its existing moved/blocked/invalid/unavailable report;
blocked rotation does not cancel the source state/ramp clock. Save/restore remains
explicitly rejected. No new mutable original global or shared table is introduced.

## Replacement physics boundary

A dedicated `rotating_debris` property opts this original class into the existing
dynamic-body integrator, including when initially hidden/gravity-off. Ordinary
pushable brushes retain their BOXPHYSICS requirement and reject dynamic rotation.
Mass/friction preserve the parent formula. Start's missing BOXPHYSICS is accepted
only for this configured mode; GOTHRUWORLD is still rejected.

Translation sweeps the **current enclosing axis-aligned bounds**, including the
correct pivot offset, through the primary world and solid objects. Incoming rays,
actor collision and rendering still use the current authored polygon shell when
BOXPHYSICS is clear. This conservative moving bound may stop concave pieces early;
it is not general dynamic polygon collision or verified retail physics.

Rotation reuses the bounded atomic angular mover. Each angular subdivision also
checks a conservative envelope against the primary world before committing any
piece/rider pose. A blocked sweep preserves all poses. Existing angular work,
affected-body, rider, lifetime and deferred-contact limits remain. Free translation
and scheduled rotation are separate original phases; there is no second player
integrator, rigid-body torque, restitution, crushing or stack solver. Source
SetForceIgnoreLimit(0) asks to retain contacts; the replacement dispatch already
delivers all eligible contacts and does not synthesize force magnitudes. General
force-threshold/contact-force services remain unavailable.

## Inspection and ownership

Brushes lists hidden/inactive fragments and shows activation/failure, on/off,
start/landing counts, authored velocity, gravity multiplier, rotation/ramp state,
actual current flags, collision mode, bounds, velocities, support and motion results.
The parent's original KILL command can activate its named pieces. No diagnostic
command runs inside drawing. Terminal brush history retains named-fragment
start/failure counts after the source is destroyed. Independent fragments are not
owned by their destroyed parent and can later be damaged/removed themselves.
Unload clears records, checked IDs, lists and aggregate membership.

CityHub has ten real WorldModelDebris entries. Two load/unload cycles exercise
billboard destruction, its six fragments, actual motion/support and cleanup.
GPU checks render a real fragment before/after motion, assert hidden/pending removal
produces no draw, and reload the world. These checks do not certify all campaign
fragment behavior, retail collision equivalence, client gibs, rotator audio, safe
NEW_LEVEL or the unchanged M2 exit gate.
