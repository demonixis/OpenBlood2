# Original explosion flame contracts

Selected CExplosionFlameFX Init/CreateObject/Update/AddParticles and the high-detail
EXP_TESLA_ALT (8) recipe now emit actual particles. This extends
[scene explosions](scene_explosion_contracts.md), [rings](explosion_ring_contracts.md)
and [sprite presentation](sprite_view_contracts.md). See the
[source ledger](../implementation/sources/m2_explosion_flame_sources.json) and
[evidence](../implementation/reports/m2_explosion_flames.md).
**M2 remains incomplete.** Surface illumination, other recipes/callers and safe
NEW_LEVEL remain unavailable.

## Original behavior

Init copies three colors, lifetimes/offsets, ramp-up/duration/ramp-down, count,
radius, position radius, gravity, velocity, delay, alpha and flags. The original
normal, ramp times and ramp flags are stored but never used by these methods.
In particular, the Tesla recipe's EXP_FLAME_RAMP_VEL does not ramp velocity.
Do not invent an implementation for those unused authored fields.

Creation uses the default SpriteTextures/drop32_1.dtx only for a null resource;
an empty or absent indexed path fails. It creates one immediate burst and sets
next-add to delay. Fade zero sets authored alpha; every nonzero fade starts at
engine-default alpha 1 until its first effect update. This differs from ring
creation and is intentional. The system retains identity rotation and a fixed
origin; the normal does not orient its particles. Typed zero origins are valid,
avoiding BaseParticleSystemFX's indeterminate local zero-position fallback.

A burst contains three groups, in order: N / 2N / 3N particles in independent XYZ
boxes with half extents position_radius/3, twice that value, and position_radius.
They are boxes, not spherical shells. Each group uses its own constant RGB and
lifetime interval [base-offset,base+offset]. Every component of velocity spans
[-velocity,+velocity]. Original 0..255 colors are normalized once in the recipe.

On each due effect callback, if elapsed >= next-add, emit **one** burst and add
delay to the prior next-add. There is no catch-up loop and no reset to now+delay.
An overdue deadline can thus emit on successive callbacks; zero delay emits on
every callback. The existing 1 ms effect cadence is retained. All five source
fade formulas run after emission. Update returns elapsed < duration, so even the
terminal update can emit a burst before the manager removes the entire system,
including particles whose individual lifetimes have not ended. Ramp-up/down are
not added to duration. Overshoot is not clamped into a different source rule.

The selected type 8 recipe offsets origin 30 along normalized normal; colors are
white, (200,200,255), (150,150,255); lifetimes .75/.875/1 with .25 offsets; ramp-up
.5, duration 2.5, ramp-down 0; N=4 (24 per burst); radius 4000, position radius 0,
gravity 0, speed 125, delay .33, alpha .85, fade 1, ramp flags 1 and
SpriteTextures/teslaprime_3.dtx. Types 0/1/2/3/6 accept an empty flame branch;
other recipe IDs fail. No generic explosion switch or new gameplay damage is added.

## Replacement services and bounds

The replacement bulk particle sampler follows the examined **later LithTech**
ps_AddParticles: six uniform draws per particle, XYZ offset from draws 0..2,
XYZ velocity from 3..5 and lifetime from draw 3. Consequently velocity.x and
lifetime are correlated. Constant colors need no separate random draw. Draws still
occur for zero-width ranges. A separate deterministic minstd_rand stream preserves
this local sampling order, not the unavailable retail engine/global RNG sequence.

Particle integration reuses the existing strict lifetime<0 removal, old-velocity
position update before gravity, and checked finite bounds. It runs every session
step, outside the 1 ms effect cadence. Particle motion, billboard sizing and alpha
rasterization remain explicitly replacement policies, not verified retail parity.

Settings require finite bounded positions/normals, radius [0,640000], position
radius and speed [0,10000], gravity [-10000,10000], alpha/RGB [0,1], duration
[.001,600], ramp times/delay [0,600], N in [1,256], fade 0..5 and known ramp bits
0..7. Lifetime endpoints must be ordered, nonnegative and at most 600. Resource
names have at most 1000 bytes and no embedded NUL. Unused normals may be zero at
the component boundary; the composite recipe requires a nonzero normal to offset
its origin. Nonfinite/backward clocks fail before mutation.

There are at most 64 systems, 2048 live particles per system and 4096 across flames.
All three groups are preflighted as one burst before particle allocation or RNG
consumption. A capacity failure leaves that burst absent, removes the failing
system on update and propagates failure from the session. Already integrated
particles and successful siblings are not rolled back. Sequential updates account
for current live particles; they do not assume future systems will expire.
Creation/indexing failures and retained update errors remain visible in reports.

Events/visual IDs are independent and never reused, including unload. Reports
retain the last 64 deliveries, owned source names and errors. Source deletion
leaves the detached system alive. World unload clears systems/reports/counters/RNG.
No original object pointer is retained. Null-damager suppression, source scheduling,
other effects and sound calls remain unchanged. Success does not set the complete
composite flag while illumination is missing.

## Presentation, inspection and checks

Flames share the existing depth-tested, no-depth-write sprite alpha pass and stable
center-depth order. Radius/640 is their quad half extent. Actual DTX alpha, per-zone
RGB and system alpha multiply; there is no added per-particle fade, additive mode,
lighting or fallback texture. A separate flame_system identity prevents confusing
flames with rings/trails. Draw/inspection calls cannot emit or advance particles.
The existing 16384-entry renderer budget is unchanged.

The inspector exposes active/live/start/expiry/failure totals, IDs, elapsed time,
next-add, bursts/emitted counts, alpha, origin, texture, prepared quads and unused
ramps, plus individual position/velocity/lifetime/color and retained failures.
Synthetic tests cover group geometry/color/sampling, unused fields, immediate and
late emissions, zero delay, all fades, terminal emission, budgets, missing assets,
checked detached lifetime and read-only scene building. Private tests use three
SubwayA/player reload cycles with an explicitly diagnostic Tesla emitter, and the
three actual type-8 property records from 07_cathedral. They do not claim a full
Cathedral session. GPU readback checks actual alpha/color and external Tesla
texture rendering with no inspector and after reload.

SubwayA's nine authored emitters are type 3, not Tesla. Source accounting remains
203 created / four transferred sounds / 372 editor-only / one unsupported Gideon /
zero failures. Remaining required services, illumination and safe automatic
NEW_LEVEL keep the observable M2 exit gate open.
