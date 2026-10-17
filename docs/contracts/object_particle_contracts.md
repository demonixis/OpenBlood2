# Current budget continuation

The [authored exit-fade validation](screen_fade_contracts.md) supersedes the old
smoke-only 2,048/8,192 budgets below: a measured 10,240-particle trail now fits
explicit 16,384 per-system/aggregate limits, with a 32,768-quad sprite snapshot.
Other recipes retain their limits; emission, RNG, lifetime and failures are unchanged.

# Original fragment smoke-trail contracts

Selected CObjectParticleFX Init/CreateObject/Update/AddParticles/GetRandomColor and
CObjectGeneralFX's high-detail OBJFX_SMOKETRAIL_1 recipe now run for grenade metal
fragments. See [fragment contracts](explosion_fragment_contracts.md),
[source ledger](../implementation/sources/m2_object_particle_sources.json) and
[evidence](../../../Documentation/Implementation/m2_object_particles.md).
**M2 remains incomplete.** This implements one original trail recipe, not a generic
ObjectFX service, multiplayer trails, ramps, child explosions or other emission modes.

## Exact selected source rules

The recipe uses radius 2000 (render half extent 2000/640), positive gravity 25,
add delay .01, density 1, EMITLOCATION, flags 0xff, speed [20,50], lifetime [.5,.75],
color [96,192] in all channels and SpriteTextures/smoke64_2.dtx. Offset, duration,
ramp times/types, rotations and position radius start at zero. Authored count 12
is **unused** by EMITLOCATION. Authored alpha .25 is also **unused**: only an enabled
alpha ramp calls SetObjectColor. The selected white/opaque system retains alpha 1;
DTX alpha still controls every rendered pixel. Do not silently apply the unused value.

Creation retains a fixed origin and identity system rotation. MOVINGSOURCE emits
positions relative to that origin; ROTATINGSOURCE prevents rotating old particles.
ALIGNROTATION uses the parent's current forward direction only for new velocities.
Parent poses are copied and checked, never retained as pointers.

The first effect update resets last-add time to absolute zero and returns without
emitting or updating last-object position. Subsequent updates compare absolute
now-last_add >= .01. One burst runs when due; no catch-up loop is introduced. The
1 ms effect cadence remains separate from per-session-step particle integration.

Without a duration, exact zero movement since the previous parent observation
sets moving=false and duration=elapsed+.75. This selected single-player path never
resumes even if the parent moves again. Position freezes, and existing particles
continue aging; their system survives through exact elapsed==duration, disappearing
only when elapsed>duration. A removed parent ends the whole trail immediately,
including any remaining particles, matching the cleared-server-handle update guard.

At each burst, count=floor(distance(last_emission_position,current_position)/2).
Start at current_position-origin and step backwards by
(last_emission_position-current_position)/count. The prior endpoint is excluded.
Short movement does not accumulate after a burst: last emission position still
updates when count is zero. Skip the source's unused zero-count division; emit
nothing and consume no random values. This avoids undefined/non-finite work without
adding a particle the source loop would not have emitted.

For each particle, draw speed, one shared RGB interpolation ratio, then lifetime.
Velocity is normalized current parent forward*speed; grayscale is (96+96*ratio)/255.
The independent deterministic RNG preserves these local draws, not the original
global random sequence. No guessed emission randomness or particle fading is added.

## Engine policy, ownership and budgets

The existing [particle integrator](explosion_ring_contracts.md) retains strict
lifetime<0 removal, position-before-gravity order and positive Y gravity. Its new
per-particle RGB defaults to white for rings; finite [0,1] validation preflights the
entire array. Integration and billboard sizing remain later-engine-informed
replacement policies, not verified Blood II retail parity.

Original dynamic list order is retained: explosion fragments in list 4 advance
before object particles in list 7 (FX IDs 36 and 52). The fragment owner resolves
trail parent IDs after movement/removal. No trail is a server object, attachment,
damaging body or new gameplay integrator. Deleting the original emitter leaves its
fragments/trails alive; deleting a fragment removes its associated trail on update.
Unload releases all arrays/reports/RNG, with IDs never reused.

Limits: 128 trail systems, 2048 particles per trail, 8192 particles across trails.
A burst preflights its count/budget and endpoint bounds before allocating particles
or consuming random values. Exhaustion fails visibly; no silent truncation or
success-shaped empty emission is used. Update failure removes the failing trail,
retains its error in bounded delivery reports and fails the session step. Failed
trail creation preserves the successful metal fragment and records the missing
component separately. Model/ring/sprite/audio siblings remain independent.

A checked pose has finite bounded coordinates and a unit quaternion within .001
squared-length tolerance. Invalid clocks, absent resources and unsupported recipe
IDs fail. The current API exposes only recipe 1 and a nonzero checked parent ID;
there are no accepted-but-ignored unsupported flag/ramp fields.

The existing sprite pass capacity grows from 4096 to 16384 entries (98304 vertices)
to hold these bounded trails plus rings and ordinary sprites. Private tests observe
6304 trail particles alone. Material/decoded-image limits stay unchanged. Particle
RGB and DTX alpha use the same depth-tested, no-depth-write alpha pass and stable
center-depth sorting. Model/sprite cross-pass sorting and retail raster parity
remain unverified. Rendering and inspection never advance clocks or emit particles.

## Inspection and evidence

The scene-explosion inspector shows trail/parent IDs, emitting/stopped state, fixed
origin/current source position, elapsed time, stop deadline, emitted/live/prepared
counts, actual and unused authored alpha, resource path and per-particle local
position, velocity, lifetime and grayscale. Retained reports show creation/update
failures after source/child removal. Hidden/reload smoke requires real trail quads
independently from rings, sprites and metal models.

Synthetic tests cover exact stream layout and RNG draws, first-update/absolute-time
behavior, unchanged count/alpha quirks, current-forward versus fixed-origin state,
strict stop/removal boundaries, no resumption, zero-count emissions, resource and
budget failures, atomic color validation, parent deletion and non-reused identities.
Three private SubwayA/player cycles at 50/60/20 Hz exercise nine emitters, 27 trails,
real DTX decoding, complete expiry and cleanup. GPU readback validates opaque tint
against known synthetic texels and actual external smoke64_2 alpha quads.

SubwayA accounting stays 203 created / four transferred sounds / 372 editor-only /
one unsupported Gideon / zero failures. Remaining flames, illumination, safe
NEW_LEVEL and other required services keep the observable M2 exit gate open.

Continuation: [original explosion flame particles](explosion_flame_contracts.md)
now implements the selected Tesla component. Earlier blanket flame omissions are
historical for that recipe; surface illumination and the actual M2 gate remain open.

The [shield/powerup continuation](shield_weapon_contracts.md) now implements original CGideonShield and owner-bound ObjectFX19/36. Earlier blanket shield/ramp/object-particle omissions are superseded only for these selected recipes. Boss actions, required remaining services and M2 stay open.

The [burning death/dismemberment continuation](death_effect_contracts.md) supersedes
blanket fire/gib/corpse-damage omissions for its selected original paths. Real
flame/smoke particles, burned corpses, runtime limbs and flesh models now run with
inspection. Blood-spurt/splat FX, generic gib/save services and full M2 remain open.
