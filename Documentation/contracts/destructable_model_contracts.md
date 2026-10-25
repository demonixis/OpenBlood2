# Original destructible models

> [Destruction VFX lot 1](destruction_debris_contracts.md) now renders the CDebris wood,
> metal and glass requests with original launch/scale/alpha, motion and impact sounds.
> Earlier generic-debris omissions remain historical for this subset only.

> [Compound-shell validation and ground queries](compound_shell_contracts.md) preserve polygon gaps and offset authored-box bounds during initial ground placement. Original model rules remain unchanged.

`CDestructableModel` is a selective direct port of original
`ObjectDLL/DestructableModel.cpp`, registered under B2BaseClass. It reuses
CDestructable, CDebris, ABC/DTX resources and the existing model renderer.
[Source ledger](../implementation/sources/m2_destructable_model_sources.json) records the import and
[implementation evidence](../implementation/reports/m2_destructable_models.md)
records verification. CityHub's ceilingfan2.abc has one unnamed clip, 19 keys and duration599. The ABC
reader accepts the empty authored name, preserves its ordinal and validates every
other count/key/pose normally. No synthetic name, animation or geometry is supplied.
No dependency or new mutable game global is introduced;
the existing destructible intrusive list remains covered by destination isolation.

## Source rules and lifetime

Precreate calls the base/aggregates once before reading model properties. Nonempty
InitFilename/InitSkin override inherited resource fields. Pushable forces gravity
and touch notification; string creation additionally forces the original solid,
visible, shadow and Gouraud flags. Difficulty filtering precedes initialization.
Chrome, world-bypassing bodies, invalid numeric properties and partial
destroy-resource pairs reject creation. A savegame creation skips initial update and
restores through the original Save/Load block ([save/load lot 8](save_load_contracts.md#implemented-movers-and-brushes-lot-8)). A lone damage filename or skin is retained
but disables the damage threshold, as in the source. Missing required ABC data fails
initialization and is reported as a failed world entry, never successful geometry.

Initialization sets original HP/max HP, mass, friction `5 + mass * 10 / 500`,
priority 40/100, damage impulses only for Pushable, and SAVEABLE/MOVEABLE/
SINGULARITY_ATTRACT plus authored surface bits. SAVEABLE is metadata, not a save
implementation. Auto dimensions use clip zero, the eight rotated corners, then
ModelScale. Authored nonzero dimensions are neither rotated nor scaled. Initial
small-shadow removal uses squared dimensions <1000; damage/destroy replacements
use <8000. Replacement auto dimensions are scaled but **not rotated**. Visibility,
shadow and additive tint flags do not implement missing lighting/shadows.

The original SFX_DESTRUCTABLEMODEL client handler only sets physics dimensions and
CF_DONTSETDIMS. This direct integration shares one checked body between client and
server; each successful dimension assignment is the actual publication, counted
in inspection. There is no fabricated network/SFX acknowledgement.

World gravity objects call the existing downward ground-placement helper. A
no-hit/start-inside ray leaves position unchanged as the original helper does;
its DE_NOTFOUND remains visible. Collision-clipped movement is not a placement
success guarantee. There is no automatic depenetration of authored overlaps.

Damage dispatch reaches CDestructable once, including CanDamage, trigger-only,
impulses, death trigger and supported SpawnObject. Destructable gates the model's
reaction, separately from aggregate HP loss. Crossing the positive damage threshold
changes resources/dimensions once. Lethal damage schedules .001 updates; exploding
models retain the original .25 cascade delay, decremented by the aggregate after
the model's update. Existing CDebris keeps fixed effective damage100/radius200,
squared-distance area falloff and material order. Gib records remain diagnostic
requests, not rendered gibs or a complete explosion composite.

No destroy resource removes the model; a complete pair replaces it and assigns
exact DestroyVisible/Solid/Gravity flags. A lifetime adaptation emits CDebris while
the source is still checked alive, then requests deferred removal; the original
requested removal first and continued using its engine handle. Reentrant destruction
is guarded, and callbacks cannot access reclaimed objects. Source repeated destruction
on a later scheduled update of a surviving destroyed model is retained. Runtime
resource/support failures stop scheduling and freeze the inspectable object; initial
failures remove it and leave their error in world accounting. Owned C++ strings
replace five legacy string handles; message-read handles are released once.

## Replacement physics and sound

ConfigureModelBody marks an ordinary model body separately from characters and world
meshes. Its half dimensions are the game's dimensions, not inferred visual bounds.
The existing box planner now accepts these bodies: bounded sliding, analytic gravity/
ground friction, higher-priority single-box pushes, atomic body/rider paths, deferred
touches and checked lifetime. A configured body cannot become an attached visual
child or character body. Models require no world-mesh BoxPhysics flag. There is one
integration per body; client-associated players still use MoveMgr only.

This is replacement physics, not retail parity. Initial overlaps, recursive stacks,
crushing, torque and collision-force magnitudes remain unavailable. TouchNotify keeps
the original two-frame sliding/standing grace and movement-distance test. Production
touches carry force zero; consequently they do not establish the force-dependent
standing flag or start a sliding loop. Explicit nonzero-force test callbacks exercise
the original branch without claiming production force support. PlayObjectLoopSound
uses real decoded, spatial, owner-following whole-clip loops, retained until stop or
owner destruction. The original material fallback path, radius1000 and priority0
are preserved. Missing WAVs fail visibly; priority arbitration and reverb remain
unimplemented.

## Alpha, AI and inspection

Alpha commands preserve case-insensitive matching, exactly-three-argument fade time,
positive-rate instant snap and elapsed-time interpolation. The source repeatedly
parsed its initial pointer for multi-command text. Bounded parsing advances through
all commands and validates finite supported alpha/rate values before any mutation;
malformed text retains the previous fade and an explicit diagnostic.

AI CheckClearShot uses IGNORE_LIQUID | IGNORE_GLASS | IGNORE_DESTRUCTABLE: the last
flag excludes only destructible CDestructableModel objects. CanSee retains its
separate visibility filter; it does not ignore these models even when their debris
is glass. Ordinary weapon collision/damage still hits them.

Objects displays current damage/death/replacement state, shared dimensions, body
mass/friction/priority/velocity/counts, fade, ground result, loop and failures for
hidden/inactive as well as visible instances. Existing model/skeleton/resource views
show ABC/skin/pose state without advancing simulation. Readiness and source-entry
counts do not imply a playable CityHub or safe automatic NEW_LEVEL. Required
remaining classes/services, world transfer and M2 remain incomplete.

## RotatingModel

Original ObjectDLL/RotatingModel.cpp adds the existing Rotating aggregate to
CDestructableModel and binds its owner before forwarding initial update. The
original axis/reverse/ramp/command and zero-based-angle rules are shared with
RotatingBrush. Ordinary model rotation uses the checked SetObjectRotation
service; physics dimensions remain axis-aligned and unchanged. Later LithTech
serverde_impl.cpp si_DoObjectRotation corroborates this ordinary-object branch.
World-model brushes retain their existing atomic rotating collision solver.

Destruction and removal still pass through the same aggregate chain once; a
pending owner cannot rotate. The Objects inspector exposes state, requested
angles, speed and result for hidden/inactive models too. Rotation sounds retain
the existing diagnostic-only behavior; this addition does not claim audio or
retail rotational physics parity.
