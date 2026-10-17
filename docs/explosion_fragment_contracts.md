# Original explosion fragment contracts

This continues [models](explosion_model_contracts.md),
[rings](explosion_ring_contracts.md) and [scene explosions](scene_explosion_contracts.md).
See [source provenance](m2_explosion_fragment_sources.json) and
[evidence](../../Documentation/Implementation/m2_explosion_fragments.md).
**M2 remains incomplete.** Metal fragments are simulated and rendered; their
OBJFX_SMOKETRAIL_1 requests remain explicitly unavailable. They are not CDebris,
WorldModelDebris, dropped weapons, physical server bodies or damaging projectiles.

## Original initialization and launch

Selected CExplosionFragFX Init/CreateObject/Update use owned typed settings,
immutable decoded ABC data and animation state in place of client handles. The
original default model/skin remain exp_sphere.abc / Explosion_1.dtx. Empty, missing
or malformed models fail creation; missing indexed skins fail independently.
DTX decoding is a presentation operation and reports its own failure.

Without random direction, align a basis to normal/+Y, draw a turn in [-pi,pi]
around the normal, then normalize normal*velocity + right*spread and multiply by
velocity. With random direction, draw x/y/z in [-10,10], normalize and multiply
by velocity. Explicit component order avoids platform argument-evaluation ambiguity.
Zero generated directions fail instead of producing NaNs. If any rotate maximum
is nonzero, draw pitch, yaw and roll from their signed ranges, including a draw for
zero-valued axes. Flags retain VISIBLE, MODELGOURAUDSHADE and NOLIGHT. The current
renderer is unlit, not a new Gouraud illumination implementation.

All fields are initialized. In particular, the source's indeterminate false
rotation flag and last collision normal become false/zero. The existing alignment
helper provides a deterministic fallback for vertical directions; retail rotation
matrix parity is not claimed. Original authored values remain separate from these
portability/engine adaptations.

## Update, collision and fade

A fragment stops moving only if speed **< 1** and its last hit normal.y **> .9**.
This check happens before gravity; resting fragments no longer rotate or fall.
Moving fragments add pitch/yaw/roll to their Euler accumulators every effect update.
They add (0,-gravity,0) to velocity and add that velocity to position. **Neither
motion nor angular increments is multiplied by dt.** Preserve this source behavior.
ABC animation alone advances by elapsed time; no visual model key invokes gameplay.
The existing 1 ms effect cadence advances independently of visibility and ImGui.

The collision ray starts at the previous position and ends at previous position +
1.1*new velocity. Existing typed IntersectSegment returns primary-world polygons,
transformed world models or ordinary AABBs, admitting SOLID or RAYHIT objects and
skipping pending removals. No liquid/glass/character/fire-through filter is applied.
Fragments are detached visuals and do not become candidates in the server object
pool. Client/server geometry reuse is an explicit single-player adaptation; retail
client replication, edge/interior and skeletal collision parity are unverified.

On impact, normalize hit normal and velocity, reflect direction by
v + normal*(-2*dot(normal,v)), then multiply by incoming speed*bounce. Place the
fragment at hit point + normalized normal; retain the raw last normal, copied hit
record and checked non-owning handle. Halve every angular increment after the hit.
The existing opaque handle lookup is used before inspection, never a retained game
pointer. The ray dispatches no touch/damage callback. Invalid/unavailable geometry
fails visibly rather than being treated as empty space; no unchecked motion commits.

Fade changes alpha only when elapsed > duration-fade_time **and** elapsed < duration:
alpha = initial_alpha - (elapsed-(duration-fade_time))/fade_time. This is subtraction,
not multiplication by initial_alpha. At exact expiry/overshoot the previous alpha
remains; the terminal update still performs source movement before removal. Zero
fade time never divides. Synthetic low-alpha descriptors can produce negative
source alpha; the existing renderer explicitly rejects it, rather than guessing a
legacy out-of-range byte conversion. The selected recipe always uses initial alpha 1.

Supported bounds are finite existing physics vectors, nonnegative scale/rotation
maxima, spread/velocity [0,10000], gravity [-10000,10000], bounce/alpha [0,1], duration
[.001,600], fade time [0,duration], paths <=1000 bytes without embedded NUL. Spawn
explosion modes/types must be zero. Trail 0 is absent and trail 1 is a retained
unsupported smoke-trail request; other trail IDs reject. These bounds and explicit
failures are not success-shaped implementations of missing original callers.

## Selected recipe, ownership and presentation

The supported high-detail type-3 grenade branch uses original gibmetal2.abc /
gibmetal3.dtx, source position without offset, normalized normal, unit scale and
rotation maxima, bounce .25, duration 10, gravity 1, fade time 2, alpha 1, no child
explosion and trail 1. Spread draws [40,60], then velocity [20,40]. Crucially,
`for(i=0; i<GetRandom(2,5); ++i)` draws a **new condition bound at each iteration**,
interleaved with spread/velocity and CreateObject direction/rotation draws. Preserve
this order, including the final condition draw. It yields two through five fragments,
not a single preselected count. No-fragment branches 0/1/2/6/8 succeed empty; other
recipes reject. The private deterministic minstd_rand stream is separate from
server/model/ring RNG; full retail random sequences are not reproduced.

At most 128 active fragments and 64 retained delivery reports are session-owned.
Capacity/resource failure preserves successful siblings and records separate errors.
Reports include missing trail service even when geometry succeeds. IDs/event IDs
are never reused across unload; arrays, counters, reports and RNG reset on unload.
Source removal leaves fragments alive. Their failure cannot cancel other composite
components or audio; original emitter null-damager suppression remains untouched.
An update failure propagates through the session and removes the failing visual.

The existing read-only explosion model snapshot includes current fragment transforms,
shared-depth textured triangles, alpha and stable center-depth sorting with other
explosion models. No renderer is advanced by inspection. Model/sprite cross-pass
sorting, surface illumination and trails remain outside this subset. The inspector
shows position, velocity per update, motion/rest state, impacts, hit identity/polygon,
alpha/time, decoded triangles, model/skin and missing services, including removed
sources and retained failures. Hidden/reload smoke requires an actual fragment draw.

Tests cover launch RNG/order, source loop conditions, per-update motion, strict rest
thresholds, point reflection/lift, angular damping, fade boundaries, terminal motion,
RAYHIT/pending lifetime, missing collision/resources, capacity, non-reused IDs and
source-independent cleanup. Private tests run nine SubwayA emitters through three
player/world cycles with real metal geometry, contacts and complete expiry. GPU
readback checks resting-fragment half alpha and the real external model/skin.

Source entries remain 203 created / four transferred sounds / 372 editor-only /
one unsupported Gideon / zero failures. Trails, flames, illumination, safe automatic
NEW_LEVEL and other required services keep the actual M2 gate open.

Continuation: [original fragment smoke trails](object_particle_contracts.md) supplies
selected high-detail OBJFX_SMOKETRAIL_1 behavior. Earlier trail omissions and the
4096 sprite-entry limit are historical; the shared renderer now holds 16384 entries.
Other ObjectFX modes, required effects and the actual M2 gate remain open.
