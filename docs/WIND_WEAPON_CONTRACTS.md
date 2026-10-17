# Original Gideon wind and one-shot shockwave boundary

Read [projectile lifetime](ENERGY_PROJECTILE_CONTRACTS.md),
[damage](WEAPON_CONTRACTS.md), [AI firing](AI_FIRE_CONTRACTS.md),
[model presentation](EXPLOSION_MODEL_CONTRACTS.md),
[evaluated lights](EXPLOSION_LIGHT_CONTRACTS.md),
[source ledger](M2_WIND_WEAPON_SOURCES.json) and
[validation](../../Documentation/Implementation/M2_WIND_WEAPON.md).
M2 is incomplete. This dependency does not register Gideon or Behemoth.

## Original weapon route

Both modes of the complete original Gideon Wind definition use weapon ID 42,
TYPE_RIFLE **0**, AMMO_NONE, damage 10–20, zero projectile velocity/radius,
range 75, one shot, reload 1/0 seconds, windfire.wav in all four sound slots,
null held-model paths and CBehemothShockwaveProjectile. The source display name
is oddly `Gideon_Shield`; it is retained. Player acquisition remains refused.

The original projectile branch takes precedence over CGideonWind::FireVector's
1200-unit velocity push. That dormant vector branch is **not** implemented and
explicitly refuses direct calls; there is no invented push in ordinary wind fire.
Existing CWeapon preflight, damage multiplier, retained audio/hearing, no-ammo
semantics and checked Setup are reused. Energy now shares the original base
FireProjectile creation/setup path; its sound override and behavior remain intact.
GIDEON_WIND name selection and the generic AI r_gun/body-forward branch execute.
Gideon's distinct rr_gun/extra/state overrides remain unported.

The runtime-only shockwave is OT_NORMAL, has the original damage aggregate (HP5),
source flags, dimensions, scale, and 0.001-second update schedule. Original fallback
Grenade filename/skin strings are retained but OT_NORMAL binds no model or sprite.
ScaleObject now retains finite nonnegative scale on OT_NORMAL; this metadata does
not produce geometry. No projectile flight phase runs for this stationary one-shot.
Its first Update overrides ordinary projectile contact/death/expiry processing.
It samples the shooter's **current** position, publishes FX/audio, traverses the
original destructible list and returns false for deferred removal.

## Defined ground query and damage

The reference reads uninitialized `iq.m_To` while adding `(0,-1000,0)`.
The port explicitly uses `from + (0,-1000,0)`. This is a necessary defined-behavior
repair, not a claim about the undefined retail result. The checked solid-only
ray uses the existing entry-face/SkipInterior policy, includes other solid objects
and primary geometry, and has no owner/class filter. Skipping enclosing bodies
is a replacement-engine policy, not verified retail physics parity. On hit,
position/normal come from the hit. On miss, the original shooter position and raw
normal `(0,-1000,0)` remain. Unavailable/invalid collision fails before effects or
damage; losing the shooter before Update fails instead of reading an invalid pose.

Damage snapshots original CDestructable list order before callbacks. A null source
object terminates that snapshot; stale/removed targets are skipped. The shooter
is excluded. Every other destructible whose **center** is at distance <=450 receives
the full selected damage, including the projectile itself. There is no visibility
filter, dimension expansion, distance falloff or solid-object requirement.
The raw direction is target position minus blast center. Damage position is the
**target position**, type EXPLODE, with no AREAEFFECT flag. The projectile sends the
message while the shooter remains the responsible attacker. This is deliberately
separate from DamageObjectsInRadius's different falloff/flags/origin rules.

Checked identities survive callbacks; removal of projectile/shooter cancels remaining
work, target removal skips later delivery, and callback-created objects are not added
to the snapshot. A latch prevents nested MID_UPDATE from duplicating or prematurely
finishing the one-shot. Destruction remains deferred. Delivery totals do not imply
health changes, and interrupted partial traversals remain visible.

## Presentation, lifetime and inspection

The original high-detail EXP_SHOCKBALL_LARGE (20) recipe uses exp_sphere.abc and
bmothball.dtx, scales 200→1500 with sqrt(time/duration), duration 0.5 s, alpha 0.5
with linear fade, per-update local-up rotation 0.05, and no random orientation or
recipe RNG draw. It has no position offset. The white evaluated light lasts 1 s,
radius 400→100, and uses `position + raw_normal * 15`; the unusual -15000 Y offset
on a ground-ray miss is retained. This light is inspection state, not illumination.
The actual time/explode.wav is detached, inner/outer radii 100/1000, misc-medium
priority. Model, light and sound are independent of source removal and each other;
missing resources remain reported and do not suppress original damage.

Existing Scene explosions inspection shows model/skin/geometry, clocks, alpha,
light state and audio failures. Objects show the pending original projectile;
weapon inspection resolves its checked child handle. The bounded terminal history
retains ground-ray endpoints/result, blast center/normal, candidates, in-radius
count, deliveries, interruption and failures after the one-shot disappears.
Rendering is read-only; no extra player integration or mutable process global is
introduced. Destination rehearsal retains source projectile/owner identity and
cleans its temporary original lists. Unload clears reports, voices, models and lights.

High-detail visual recipes, unlit alpha rendering and the replacement ray policy
are not retail renderer/physics parity. Gideon initialization/states/shield/powerup,
remaining required FX/services, safe NEW_LEVEL and the actual M2 gate remain open.

The [shield/powerup continuation](SHIELD_WEAPON_CONTRACTS.md) now implements original CGideonShield and owner-bound ObjectFX19/36. Earlier blanket shield/ramp/object-particle omissions are superseded only for these selected recipes. Boss actions, required remaining services and M2 stay open.
