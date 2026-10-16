# Original explosion particle-ring contracts

This continues [explosion sprites](EXPLOSION_SPRITE_CONTRACTS.md),
[models](EXPLOSION_MODEL_CONTRACTS.md), [lights](EXPLOSION_LIGHT_CONTRACTS.md) and
[sprite presentation](SPRITE_VIEW_CONTRACTS.md). See the
[source ledger](M2_EXPLOSION_RING_SOURCES.json) and
[validation evidence](../../Documentation/Implementation/M2_EXPLOSION_RINGS.md).
**M2 remains incomplete.** These are actual moving, textured particles; fragments,
flames and surface illumination remain unavailable.

## Original rules and bounded engine service

CExplosionRingFX retains original Init/CreateObject/Update/AddParticles rules.
Owned typed settings, particle arrays and resource names replace client handles.
Particles are relative to the system origin and rotation. AddParticles aligns the
radial plane with the supplied normal when align=1, otherwise world +Y. It caches
right/forward **before** turning the basis by 2*pi/count, then emits normalized
right times position radius and velocity. All particles start with the system's
duration; their white color is multiplied by the system RGB. The existing checked
alignment helper supplies a deterministic vertical-axis fallback where the original
passes parallel forward/up vectors. This fallback is not verified retail parity.

The default texture is SpriteTextures/smoke64_2.dtx. Resource indexing is required
at creation; DTX decoding and errors belong to presentation. A typed zero position
is a valid origin, avoiding BaseParticleSystemFX's uninitialized local position
when both position and server handle are absent. No server pointer is retained.

Creation without delay emits immediately with initial alpha. A positive delay
starts with zero alpha and no particles. At elapsed >= delay, reset start time to
now, clear delay and emit; restore initial alpha **only for fade=0**. Return from
that update without rotation/fade. Overshoot is discarded and new particles do not
inherit the waiting time. A fading delayed ring becomes visible on its next update.

Original rotate modes 1–5 use the five source formulas times pi **per effect update**,
not dt-scaled rotation. The axis is the supplied normal when aligned, otherwise +Y.
Fade modes 1–5 retain linear, endpoint and two-half formulas. Update interpolates
before returning elapsed < duration. The manager removes terminal effects before
presentation; it does not clamp final overshoot into a new gameplay rule. The
CSFXMgr 1 ms cadence applies to effect rules. Particle integration runs each session
step, outside that effect cadence and outside the inspector.

The new engine particle integrator is deliberately small: lifetime -= dt; discard
only when lifetime < 0; position += old velocity*dt; then velocity.y += gravity*dt.
This ordering is supported by the later LithTech source recorded in the ledger,
**not a verified recreation of Blood II's unavailable retail engine**. No collision,
bounce, drag, wind or other particle flags are implemented. The selected ring caller
uses flags=0. Validation preflights the whole array before mutation. dt is finite
[0,60], arrays have at most 2048 particles, and positions/velocities use existing
physics bounds. Bad clocks/state fail visibly; session failure is propagated.

Settings require finite bounded position/nonzero normal, RGB/alpha [0,1], radius
[0,640000], position radius [0,10000], speed/gravity/rotation [-10000,10000], duration
[.001,600], delay [0,600], 1–256 particles, fade/rotate 0–5 and align 0–1. These
explicit supported bounds reject unsupported input without silent clamping.

## Selected high-detail recipes

Types 0/1/2/3 each emit one 32-particle fire ring, offset 25 along normalized normal,
position radius 10, velocity 400, gravity zero, duration .5, alpha 1, fade 1, align 0,
Firepart64.dtx. Radius is 4000/8000/12000/8000 respectively. Type 3 also emits three
32-particle Smoke32_4/5/6 rings: offsets 10/25/40, radius 5000, position radius 10,
duration 1.5, alpha .5 and fade 1. In source order each draws velocity [100,150],
gravity [0,30]/[20,50]/[40,70], then delay [0,.5]. This is nine draws per grenade.
Types 6/8 successfully produce no rings, matching their selected high-detail branch;
other types fail explicitly.

A private deterministic minstd_rand stream preserves those per-component draw
orders and ranges, independent of server/model RNG. It does not reproduce global
retail RNG or fragment draw interleaving. No quality menu or generic particle ABI
is added. The original server emits model/sprite/ring/light requests before its
existing null-damager radius call and sound. Ring failure does not cancel siblings,
audio or other components. Null-damager suppression remains unchanged.

## Rendering, ownership and inspection

BaseParticleSystemFX passes radius/640 to SetupParticleSystem. Presentation uses
that value as camera-facing quad half extent, independent of texture dimensions.
The later renderer references support this policy; retail pixel parity remains
unverified. Actual DTX RGB/alpha is sampled and multiplied by system RGB/alpha.
Particles share the existing sprite depth test, no-depth-write alpha pass and stable
back-to-front sorting with ordinary sprites. Model/sprite passes remain separately
sorted. There is no per-particle lighting, additive blending or invented placeholder
texture. World revision owns material/cache lifetime; failed decoding is inspectable.
Building/drawing a frame never advances particles or original game state.

A session reserves at most 128 systems and 2048 particles, counting delayed systems'
future particles. Reports retain the last 64 deliveries with independent event and
system IDs; identities are never reused across unload. Partial failure preserves
created siblings. Start, normal expiry and failure counters are distinct. Source
removal does not kill detached particles; unload clears arrays, reports and RNG.

Scene explosions displays reserved/live counts, pending delays, elapsed time, alpha,
origin, half size, gravity, texture path, prepared quad/failure counts and expandable
local positions/velocities/lifetimes. Delivery reports and source errors survive
emitter removal. Missing composite components remain explicit; an accepted event is
not a completed composite service.

Tests cover radial layout, normal alignment, gravity ordering, strict particle
expiry, atomic rejection, all fade/rotate formulas, cadence, delayed activation and
overshoot, ordered recipe draws, capacity/partial failure, independent resources,
source removal, read-only presentation and non-reused identities. Private tests
exercise nine SubwayA emitters: 36 rings, 1152 reserved particles, 27 smoke rings
after fire expiry, complete expiry and three player/world unload cycles. GPU tests
check actual pixels, color multiplication and half-alpha blending; application
smoke also requires particle quads while the UI is hidden and after reload.

SubwayA source counts stay 203 created / four transferred sounds / 372 editor-only /
one unsupported Gideon / zero failures. Safe automatic NEW_LEVEL, remaining required
FX/lighting and other services still block the unchanged M2 gate.

Continuation: [original metal fragments](EXPLOSION_FRAGMENT_CONTRACTS.md) now supplies
selected grenade fragment motion/collision and rendering. Earlier blanket fragment
omissions are historical; smoke trails, other required services and M2 remain open.
