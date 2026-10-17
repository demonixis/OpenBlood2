# Original burning deaths and dismemberment

Selected original AI_Mgr, CCorpse, AI_Shared, CGib, CGibFX and ObjectFX paths now
execute with typed objects, owned audio, visible models/particles and inspection.
See [source hashes](m2_death_effects_sources.json) and
[implementation evidence](../../Documentation/Implementation/m2_death_effects.md).
**M2 remains incomplete.** This extends the [ordinary corpse](corpse_contracts.md),
[node visibility](character_node_contracts.md), [hit](ai_hit_contracts.md) and
[object-particle](object_particle_contracts.md) contracts.

## Original decisions

Fire takes precedence unless ALWAYSGIB is set, including with NEVERGIB. Other
lethal hits gib for EXPLODE, ALWAYSGIB or accumulated frame damage fraction >= .90,
unless NEVERGIB. Preserve the unused initial limb random draw, then strict >80,
>70, >50, >70, >50 thresholds for neck/right arm/left arm/right leg/left leg.
The source's optional seven-percent big-gib and six-percent kill voice draws remain
ordered; the VoiceMgr events are inspection requests, not played voice groups.
The commented-out living-NPC limb-loss branch stays disabled.

AI gib launch uses the last direction with Y+2, normalization and speed
50 + 2 * accumulated frame damage. AI_Mgr and SetupClientGibFX negate the same
vector twice; the final vector is also passed to limb creation. Count is integer
mass >> 5; scale is original AI mass / 250 only when positive and below 250.
Current production classes retain the source default AI mass. Corpse gib launch
instead negates the incoming packet, subtracts one from Y, normalizes, scales by
50 + packet damage, then negates through SetupClientGibFX. Do not unify these rules.
Zero-length normalization is a finite zero-velocity safety adaptation.

A failed visual fragment/limb request records its failure but does not prevent the
original NPC removal. Failed core corpse placement still retains the actor and
its diagnostic as before. This fixes the prior blanket fire/gib refusal, without
claiming that every possible frozen actor has the same cause.

## Burning corpse ownership

The AI creates a normal force-update source at its current torso. On corpse
creation, Fire selects skins/enemies/burned.dtx and a random death slot before the
existing fallback check. Setup transfers the source to the corpse, attaches it to
torso, starts original ObjectFX24 with scale 3 / SCALENUMPARTICLES, and plays
sounds/weapons/fireloop.wav at volume .6, radii 200/1000, AI-medium priority and
requested reverb. Output remains dry. A separate smoke source is created at setup.

On an update strictly more than five seconds after burn start, detach/remove fire,
kill its loop, publish ObjectFX21 and attach the smoke source. Strictly more than
five seconds after smoke start, detach/remove smoke. Animation completion keeps
.1-second updates while the attachment exists; otherwise the existing single-player
persistent / multiplayer deadline rules apply. The smoke particle system itself
lasts two seconds, independently from its five-second source lifetime.

Ownership transfers once. Source deletion before transfer cleans its fire object;
corpse deletion, gibbing, failed setup and world unload release both sources,
attachment and loop. This fixes the original destructor's repeated wrong-handle
KillSound calls and avoids its dangling smoke objects. Child identities are checked
and never reused within the session; they are not C++ owner pointers.

## Flame and smoke recipes

The existing particle system renders actual external DTX alpha quads. Fire uses
15 particles per .05-second burst, radius 2500, position radius 12.5, gravity +275,
speed 0..25, lifetime .25..75 and red-to-white color. Smoke uses nine particles per
stream sample, .1-second bursts, radius 2500, position radius 12.5, gravity +125,
lifetime .5..1.5 and gray-to-white color. Only particle count is scaled by three.

Both use filled-sphere emission and MOVINGSOURCE: old particles stay relative to
a fixed origin. Smoke also uses MOVINGSTREAM; sample count is floor(distance/25),
so a stationary source emits nothing. Each stream sample emits nine particles,
stepping backward toward the preceding emission position. Skip the unused divide
by zero when count is zero. Preserve the source's cached right axis before each
random basis rotation, speed/radius/color/lifetime draw order, first-update return,
absolute last-add clock, no catch-up loop and strict duration expiry.
Neither recipe enables alpha ramp flags: authored .75/.25 alpha is not applied.
Particle budgets, independent deterministic client RNG, gravity integration and
billboard raster policy remain the existing replacement engine policies.

## Detached limbs and postmortem damage

CreateLimb copies the original model/skin, samples the limb node position but uses
the parent's object rotation. It selects limb_head/limb_arm/limb_leg, applies the
original named-node mask and raw clip dimensions, then sets velocity after creation.
Y is replaced with one before normalization and a 200..500 speed draw. Limbs do
not inherit parent visual scale. Only heads remain kickable. Masks address named
nodes plus consecutive _extraN names, not skeletal descendants. Preserve the
source arm +3 hide, which can already hide the neighboring root, and the original
arm/leg loop-index skips when isolating a limb. Corpse transfer copies hidden limbs.

CCorpse now delegates damage to CDestructable, then performs its separate current
pose/sphere segment lookup. There is no random fallback for a missed corpse limb.
Preserve the source's bitwise EXPLODE guard (not enum equality), .5 limb-loss chance,
health mutation before that guard, and delayed .1-second removal after gibbing.
Visible corpse neck always creates a head; visible arms/legs use 80/60 percent.
Already hidden limbs never duplicate. Corpse head hits do not reuse living-NPC
instant-headshot rules. Reentrant aggregate damage retains existing deferred
lifetime checks; removed owners do not continue spawning.

The runtime-only CGib limb selection uses 10 HP, mass 3, nonlooping animation,
VISIBLE/RAYHIT/GRAVITY/TOUCH_NOTIFY and the existing nonsolid body integrator.
Head contacts with moving characters use original doubled velocity plus upward
kick, two bounces, spin reset and impact sound. Preserve first-update axis selection,
zero initial spin deadline, source bounce thresholds and multiplayer removal timing.
These bodies are nonblocking. Limb node positions can put their raw clip boxes
inside the train floor/walls. Creation therefore reuses the bounded nonsolid-body
placement service with the parent as its anchor before physics begins; failure
removes the child and reports it. Requested/resolved placement is inspectable.
Collision/friction and this placement remain replacement physics.
Authored general CGib templates, generic-material child gibs on limb destruction,
its blood-spurt effect and save/restore are not implemented by this runtime subset.
A destroyed limb is removed; no successful recursive-gib effect is claimed.

## Client flesh fragments

Typed publication creates client models separate from server collision/damage
objects, with retained reports after source removal. The selected high-detail
SFXMgr rule doubles count with 20 percent probability; otherwise the source's
(DBYTE)1.5 multiplier equals one. B2_LITE model/sound variants are 1..3. Preserve
.8 dimensions for random starting positions, asymmetric Y placement, independent
.8..1.35 scale axes, near-cardinal launch basis special cases and per-axis velocity
ranges. SIZE_SMALL is unused in this selected CGibFX source body.

InitMovingObject zeroes the earlier HALFGRAVITY/STICKY flags: use .5 of original
client gravity (-1000 normal / -500 liquid), position-before-gravity integration,
.35/.20 reflection damping, floor bounce count and spin changes. Container sampling
is at creation, as in the source. Collision uses the existing primary-world ray.
A resting model remains until unload; general SFX-list eviction/fade and retail
outside-world classification are not reproduced. Capacity is 256 models and 64
retained reports, with conservative space reserved for doubled requests. Exhaustion
and resource/query failures remain visible. IDs survive manager clear; no shader
or dependency change is required. Detached positional impact WAVs are world-owned
one-shots and release on completion/unload.

Blood-spurt particles, blood impact splats/pools and their child-effect random
sequences remain explicitly unavailable. Their absence is not a successful FX
service and does not close M2. The renderer and Skeleton inspector expose fragment
counts, resources, poses, velocity, liquid/rest state, impacts and errors. Corpse
and limb panels expose actual health, masks, clocks, attachment/effect/voice IDs and
cleanup. Inspection never advances simulation.


Continuation: [original corpse blood pools](blood_splat_contracts.md) supersedes the pool omission for corpse ground hits. Blood-spurt particles, gib impact callers, shared SFX eviction and other required services remain incomplete. The M2 exit gate is unchanged.

The [blood-spurt continuation](blood_spurt_contracts.md) extends the selected original weapon/fragment/limb callers and impact splats. Earlier blanket omissions are historical; separate flesh sparks, generic debris and M2 remain incomplete.
