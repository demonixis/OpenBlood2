# Original weapon impact sparks

Selected `CWeapon::UpdateWeaponFX`, `CWeaponFX::CreateSparks` and
`CSparksFX::CreateObject/AddSparks/Update` now run for flesh and metal vector impacts.
Read [weapon impact contracts](weapon_impact_contracts.md),
[blood-spurt contracts](blood_spurt_contracts.md), and
[source hashes](m2_spark_sources.json). M2 remains incomplete.

## Source selection and recipes

Flesh always requests sparks, independently of the chance-gated blood jets. Metal
requests them for bullet/shell/BMG ammunition and melee. Other melee surfaces set
the original flag but have no recipe in CreateSparks: the zero-duration/count case
is reported explicitly and creates no fabricated effect. Other surfaces consume
no spark RNG draw. All visual failures remain separate from damage and hearing.

The existing AddImpact normal*1.25 position is retained; the client normal is then
normalized. Raw sampled damage determines the count, before inventory multipliers.
Flesh uses normal*50, colors (192,0,0)..(255,0,0), blooddrop1..3 (B2_LITE),
truncated damage*4 below 50 and otherwise 200 particles, duration2, emission radius5,
particle radius200 and gravity-150. Metal draws 0..19: zero chooses the large
variant. Direction is normal*75; colors are yellow..white; texture is particle2.dtx.
Small metal uses truncated damage*2 / cap100 / duration.5; large uses damage*5 /
cap250 / duration2. Both use emission radius.3, radius175 and gravity-150.
The strict damage<50 branch and integer truncation are preserved, including count0.

CreateObject emits a single burst and starts its clock immediately. AddSparks keeps
the original direction magnitude, normalizes the direction, then forms velocity
bounds normalized_component +/- magnitude. Components outside the strict (-.001,.001)
interval are clipped at zero on the incoming side. This is only an initial velocity
restriction; particles do not collide with walls or floors. No extra liquid gravity
or alternate recipe is added to this branch.

Update fades alpha linearly from creation time. Exact duration is still present at
alpha0; WantRemove only returns false strictly after duration. Original detached
systems otherwise remain empty in the shared SFX list. The manager reclaims these
**already invisible, exhausted** systems after duration instead of retaining empty
objects until unrelated list eviction. The original class behavior remains directly
tested. Shared eviction, parent-list bookkeeping and fade-color mode are not ported.

## Engine adaptations and ownership

Sparks are per-session owned values. Publication checks the source once; later
source/target/weapon removal leaves them alive. Unload clears systems and reports;
IDs never repeat. The .001 scheduling interval and simulation clock remain independent
of UI/render visibility. A failed update retains its error in report history.

Bounds: 128 systems, 16384 particles, 255 particles/burst, 64 reports, 100-byte labels,
1000-byte texture paths, finite vectors, duration(0,10], emission radius[0,100],
particle radius(0,10000], gravity magnitude<=10000, delta[0,1]. Color inputs are
[0,255]. Invalid recipes, stale owners, missing assets and budgets fail explicitly;
no silent eviction. Textures are indexed at creation and strictly decoded by the
existing presentation cache; decode failures remain visible.

Portable particle RNG is separate from the existing per-weapon cosmetic selection
stream and from gameplay RNG. Six fractions interpolate position, velocity and
color using the later LithTech AddParticles mapping; this and the shared
position-before-gravity integrator are replacement engine evidence, not a 1998
executable trace or retail bitwise RNG parity. Zero world position is deterministic,
repairing the original uninitialized base-particle local for detached origin effects.

SDL_GPU renders actual DTX billboards at radius/640 with per-particle RGB and nonnegative system
alpha through the existing depth-tested sprite pass. Raw alpha stays inspectable;
the presentation lower clamp handles float particle-lifetime tails after the double
clock crosses duration (reproduced at 30 Hz), without deleting remaining particles. Inspector snapshots never
advance simulation. Sprites shows counts, origin, outgoing bounds, clocks, assets,
failures and terminal reports; weapon impacts link their request IDs. Existing
SPIR-V/MSL shaders and dependency pins are unchanged. Native Metal is not tested
by NVIDIA runs.

Underwater smoke bubbles, generic/fade-color sparks, wood fragments, shared SFX
list eviction, outside-world classification and remaining required M2 services are
still unavailable. These visuals do not establish complete campaign traversal.
