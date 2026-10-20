# Original Life Leech continuation

This separately requested level-loading continuation ports weapon 16, LifeLeechPU,
CLeechPrimeProjectile/CLeechAltProjectile, DAMAGE_TYPE_LEECH (15), original
FIRETRAIL_1 particles and high-detail explosion recipes 16/17. It extends the
[weapon](weapon_contracts.md), [energy projectile](energy_projectile_contracts.md),
[explosion sprite](explosion_sprite_contracts.md) and
[object particle](shield_weapon_contracts.md) boundaries. Original references are
hashed in [the source ledger](../implementation/sources/all_levels_life_leech_sources.json).
M2 closure is unchanged; map entry is not complete traversal or multiplayer acceptance.

## Weapon and damage

The complete original WeaponData entry supplies both held/view models and icons,
Focus use 2/100, damage 20–30/75–100, cadence .15/1 seconds, primary speed 1000,
range 3500 and primary radius 100. CLifeLeech only selects that definition.
LifeLeechPU gives zero additional Focus, original name/resource 363 and the
Lifeleech_pu model/skin. Pickup, drop, spawn, inventory, client FIRE_KEY and
AI LIFEL name selection reuse their existing source paths. Demo player acquisition
still refuses this non-demo weapon. No replacement asset is supplied.

Leech damage heals a player attacker by one tenth of damage after resistance,
region modifiers and armor, only when the victim is a CBaseCharacter. The source
CPlayerObj::IsDead flag controls refusal, before victim death handling; it is
not replaced with the aggregate's potentially pending death flag. God mode,
non-damageable/dead victims and normal health caps retain existing behavior.
The source slow-death exclusion has no active counterpart: that multiplayer
state remains unavailable in this local runtime. Typed damage and saved damage
state accept the original ordinal 15, retaining checked projectile attribution.

## Primary projectile

The original sprite is Sprites/lleechproj.spr. Color is (1, random .25–1, 0),
trail lifetime scale random .25–2. Initialization samples roll, signed curve
PI*.05–.1 and amplitude 5–20 in source order; subsequent Setup replaces its
initial roll with firing alignment, as in the source. Scale starts at zero,
becoming .2 when elapsed time reaches .01 seconds.

Each original update schedules .001 seconds, starts the attached spatial loop
Sounds/Weapons/lifeleech/projectile.wav (radius 750, priority 1), and stops that
loop on contact/death before explosion. Otherwise it rotates around current
forward and adds the new up vector times amplitude to velocity **per update**,
without multiplying by frame time. It overrides the base ten-second expiry.
Flight uses the existing single continuous point sweep; replacement engine
physics and portable random bits are not verified retail parity.

The source radius-100 colored light's current position and unavailable
illumination are exposed in Objects. No invented lighting is rendered. Missing
sprite/trail/sound or checked service failures remain explicit and remove the
failed projectile. Shooter unlink, source-bound trail removal, sound ownership
and deferred callbacks follow the existing projectile contracts.

FIRETRAIL_1 is the original comet-tail recipe: radius 200, position radius 6,
delay .025, density 10, one particle per sample, yellow/orange Firepart64.dtx,
zero velocity/gravity and .25–.75 lifetime scaled by the projectile request.
Particles stream between owner positions, use the pre-rotation right vector,
and derive lifetime from sampled distance to the trail axis. Stopping changes
the finite lifetime once; movement does not restart it. Unused zero-count
source divisions are skipped. Authored alpha .5 remains unused without ramp
flags. Existing per-system/global capacities and owner removal stay enforced.
Lifetime scaling is bounded to .25–2 and accepted only for recipe 3; all existing
callers keep scale 1. The real decoded texture is rendered by the existing pass.

## Alternate projectile

The OT_NORMAL projectile never flies. Its first update takes the shooter's
current position, publishes explosion 17, then snapshots the original ordered
destructable list. It excludes the shooter and selects live OT_MODEL objects
with SINGULARITY_ATTRACT within 450 units, including the exact boundary.
Each selected object rotates by a random angle within +/-PI*.2, receives
`velocity + gravity*(-.25) + outward*(450-distance)*7.5`, then receives the
original Explode damage attributed to the shooter. It is removed after this
single pass. The sender/owner and each target are rechecked across callbacks.

The donor reads an uninitialized rotation axis for the first target. This port
initializes that axis to world up; subsequent targets use the previous iteration's
gravity*(-.25), preserving the source order. Zero gravity leaves rotation
unchanged. A target exactly at the center receives the gravity term without an
undefined normalized radial vector. These are explicit finite-state adaptations,
not claims about retail undefined behavior. Removal snapshots replace borrowed
list pointers without manufacturing new targets during callbacks.

## Real impact components

Primary high-detail recipe 16 creates explodesmall.spr (.25 to .75, .5 seconds),
smokepuff2.spr (.25 to 1, three seconds), and the original yellow radius 75 to 0
one-second light state. Alternate recipe 17 creates duel_sphere.abc/leechball2.dtx
(scale 100 to 1000, .5 seconds), explode128.spr (.25 to 1.5, .75 seconds), 32
lensflare_1 ring particles and yellow radius 300 to 150 light state. No impact
sound, fragment or flame component is invented. Light illumination is still
unavailable, independently of the real sprite/model/ring presentation.

Sprite alignment 2 now aligns to the normal then rolls around that normal using
a portable per-event random angle in [0,2*PI]. Alignment 0/1 are unchanged;
invalid alignment 3 still fails. Component publications retain independent
resource errors. Renderers stay read-only and shader/dependency sources are
unchanged. Inspection exposes curves, trail scale, loop, light position and
pending illumination; existing effect panels expose all component lifetimes.
