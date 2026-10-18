# Original blood spurts and debris impact contracts

Scope: the selected flesh recipe of `CParticleExplosionFX`, original vector-weapon
blood selection and two jets, `CGibFX` launch/impact callers, and `CGib`'s deferred
impact request. Read [pool contracts](blood_splat_contracts.md),
[death contracts](death_effect_contracts.md), and
[weapon impact contracts](weapon_impact_contracts.md) before extending these paths.
M2 remains incomplete.

## Source rules

- Flesh impact always draws the original integer range0..3, including damage >25. Request
  blood when the draw is zero OR raw sampled damage >25. The inventory multiplier
  does not scale the two visual recipes. Cosmetic failure never cancels damage.
- Client impact audio precedes the two particle systems. Both share the selected
  blooddrop texture (B2_LITE variants 1..3). The entry jet uses normal*30/50,
  overwritten Y40/50 and X/Z spread20; the exit uses normal*(-2.5/-3)*damage,
  Y+30/40 and the same spread. Reversed finite component endpoints are intentional.
  Lifetimes/fade are 1/.5 and 1.2/.8 seconds. Each gets an immediate Update.
- Each flesh fragment publishes a two-second/.5-fade jet at its initial position,
  velocity*.6, with immediate Update. Its impact creates a .14+random(-.1,.1)
  splat; growth .01 applies at normal.y >=.5. Position is the post-bounce point,
  with the original .3-normal correction when opposing travel, without pool Y+.1.
- A qualifying original CGib contact sets pending blood after reflection/damping.
  The next Update consumes it using current position and velocity*.1, then the
  client multiplies by .6. Decode does not immediately Update. Kicking alone does
  not set pending blood. Only the existing flesh-limb subset is supported.
- One non-bouncing half-gravity emitter creates six interpolated samples, two
  particles each, BEFORE moving. Jitter is +/-.5; drift is (0,-10..-5,0); color
  is 128/255. Lifetimes are 10 + sample*(time since emission)/6. Particle gravity
  is -100; emitter gravity is -200 normally, -100 when initialized in liquid.
  Contents are sampled once, using the first container including hidden ones.
- Emitters integrate old velocity before gravity, reflect/damp .35 or .20 and
  stop emitting on the first primary-world impact. Existing drops continue fading.
  Their smaller splat is .05+random(-.05,.05), growing only at normal.y >.5.
  Sound radius300 chooses gib_impact0..3.wav; a missing zero is not remapped.
  Sound and splat failures are separate. Shared normal/normal alignment is reused.
- First Update starts the effect clock. Fade begins strictly after fade_time;
  exact end time remains alive at alpha zero; strictly later removes it. Initial
  zero elapsed updates emit but avoid invalid zero-length collision rays.

## Ownership, inspection and bounds

`BloodSpurts` owns values per session, without source/target pointers. Server
publication checks the live source; explicit client publication has no server
owner. Source death/removal cannot destroy detached drops or marks. World unload
clears all effects and histories; IDs are never reused. Preparation and inspection
never advance clocks. Reports retain ended/failed state and child-event identities.
Sprites inspection shows current emitter pose, velocity, liquid state, particles,
alpha, texture failures and history; weapons/limbs/fragments expose originating IDs.

Limits: 128 systems, 2048 particles/system, 16384 total, 64 retained reports,
source labels100 bytes, textures1..3 (zero requests client selection), finite
physics vectors, lifetime (0,10], fade [0,lifetime), update delta [0,1]. Capacity
failure is explicit, never silent eviction. A whole 12-drop burst is checked before
insertion. Existing particles use the shared checked integrator. The .001-second
schedule avoids a second update in the same publication frame.

The original selection happens on the server; this integration deliberately uses
its existing per-weapon cosmetic stream for that draw. New visual availability
must not perturb established AI/damage randomness. Probability, range and draw-before-
damage-test order are preserved, not global/retail random ordering. Regression tests
retain their existing authored combat and corpse-clearance assertions.

The engine keeps portable per-component random streams, explicit values instead
of packets/client handles, finite primary-world segment collision and existing
SDL_GPU billboard rasterization. These are documented adaptations, not retail
bitwise parity. Textures are indexed at publication and decoded by presentation;
missing/corrupt decode remains visible. No shader/dependency changes are required.

## Still unavailable

Separate flesh SparksFX, the generic particle-explosion/debris recipes,
GetPointStatus outside-world classification, shared original SFX-list eviction,
full save/restore and broader M2 acceptance remain open. Arterial spurt calls
commented out in original corpse/player/living-AI code stay disabled. This
continuation does not enable living-NPC limb loss or add collision to cosmetic FX.

The [impact audio correction](../../../Documentation/Implementation/gib_impact_audio_fix.md)
uses `IntersectWorldSegment(position, next)` for emitter movement. The current
frame's endpoints bound the query: a distant surface must not stop emission or
produce an impact sound/splat before the emitter reaches it. `CastWorldRay` remains
a direction-based query for original sprite placement.

The [original impact sparks](spark_contracts.md) continuation supersedes the flesh/metal spark omission only. Underwater bubbles, generic recipes and remaining M2 services stay unavailable.

The [M2 acceptance review](m2_acceptance_contracts.md) adds the exact requested impact-sound path to live/expired inspection, checks the original absent-zero asset explicitly, and keeps unrelated failures rejecting the route.
