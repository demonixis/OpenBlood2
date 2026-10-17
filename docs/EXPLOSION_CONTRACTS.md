# Original brush explosions and debris requests

> [World-model debris](WORLD_DEBRIS_CONTRACTS.md) now connects the selected original
> fragment activation/lifecycle with bounded replacement motion. Earlier blanket
> WorldModelDebris omissions below are historical; full retail physics remains open.

> Extended by [scene explosion contracts](SCENE_EXPLOSION_CONTRACTS.md): original
> CClientExplosionSFX activation/removal and detached audio, preserving its null
> damager (zero delivered damage). Client FX and complete M2 remain unavailable.

> Later extension: [pickup contracts](PICKUP_CONTRACTS.md) supersede blanket
> pickup/SpawnObject omissions below for eight ammo and four health/armor classes.

> Extended by [PUSHABLE_CONTRACTS.md](PUSHABLE_CONTRACTS.md): selected Pushable/Gravity brushes and
> brush damage impulses now work with bounded box physics. Omissions and counts
> below describe the earlier stage; polygon dynamics and full M2 remain open.

This extends [BRUSH_CONTRACTS.md](BRUSH_CONTRACTS.md) and the selected
[damage transport](WEAPON_CONTRACTS.md). M2 remains incomplete. Read the
[source ledger](M2_EXPLOSION_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_EXPLOSIONS.md).

## Original rules

CDestructableBrush now owns the original CDebris aggregate alongside CDestructable.
Precreate and initial update still forward through BaseClass once. The aggregate
reads world **and string** properties; this corrects the preceding increment's
world-only Amount diagnostic. Runtime creation retains defaults. Restore remains
unsupported. Amount retains the original byte conversion, including wraparound.
Scale remains signed; nonfinite or absolute values above one million reject.
Custom resource strings use owned C++ strings instead of HSTRING storage.

Material selection and surface fallback follow the source order: Stone, Metal,
Wood, Terrain, Plastic, Glass, Flesh, Liquid, Energy. The first enabled material
provides GetSurfaceType when the brush's authored surface is zero. Each enabled
material emits a separate request with its original surface/10, SIZE_SMALL and
optional TYPEFLAG_CUSTOM bits. Custom=true alone emits nothing; the presence of
any custom model, texture or sound enables the custom bit independently.
This fallback query does not rewrite the original authored surface user flags.

Create negates the damage direction, subtracts one from Y, normalizes and scales
by 50 + cumulative damage. SetupClientGibFX then negates its shared vector **on
each material call**. Multi-material velocities therefore alternate signs. These
source quirks are preserved. The singular direction (0,-1,0), whose adjusted
vector is zero, produces finite zero velocity instead of undefined normalization.

If Exploding is enabled, CDebris requests EXP_GRENADE (3), upward normal, and
`Sounds\Weapons\c4\explosion_1.wav` at radius 1000, then applies area damage.
The original ReadProp declares but **never reads ExplodeDamage or DamageRadius**.
The effective values remain the constructor defaults: **100 damage, 200 radius**.
World retains authored overrides; the inspector explicitly labels effective values.
No silent correction of this source behavior is introduced.

## Damage and lifetime

The selected ObjectUtilities::DamageObjectsInRadius branch walks CDestructable
members in original list order. For target position P, origin O and average
half-dimension A, its distance measure is |P-O|² - A². Damage is full below R²/2,
then falls linearly in that squared measure to zero at R². A target exactly on
the boundary still receives a zero-damage message. No ray, visibility, activity
or SOLID filter is added; source damage can cross walls and reach hidden/inactive
objects. This is not a conventional linear-distance explosion falloff.

Local owned Damage messages retain type Explode=5, with area_effect as a separate
boolean corresponding to source flag 16. The checked sender is the exploding
brush, not its last attacker. Source brush calls use the same responsible and
sender object; this helper deliberately requires that supported relationship.
Other callers, explosion classes, multiplayer attribution and the retail wire
format are not newly implemented.

The existing selected aggregate applies resistance, armor, health and named
triggers. God mode, CanDamage, TriggerDestroyOnly and dead guards still apply;
Destructable separately controls brush removal. Player stats synchronize through
the existing shell path. Explosion impulses, AI-specific damage and complete
retail damage metadata remain pending. The report counts **delivered messages**,
not reduced-health targets: it can include the already-dead exploding brush or
protected receivers.

Destruction runs on the next scheduled update. A blast can kill another brush,
whose explosion is scheduled for a later update, preserving deferred chain
reactions without recursive destruction. The helper snapshots checked handles
before callbacks, retaining original order. Targets removed by an earlier
callback are skipped; objects spawned during dispatch wait for the next blast.
If a callback removes the source, delivery stops and the report marks cancellation.
The source remains allocated until the callback/step completes. This strengthens
lifetime safety compared with walking mutable source links directly.

Finite/range checks bound origin, radius and damage; an underflowed squared radius
rejects. The existing 65,536-object arena bounds snapshot size. A failed message
stops dispatch and marks cancellation. No geometry query or second physics/player
integration is introduced.

## Inspection and pending effects

Brushes shows effective surface, amount/scale, materials, effective explosion
values, custom resource paths and indexed/missing status. Its queued KILL command
uses the original message path. Hidden and inactive objects remain listed; World
retains failed creation and authored properties after runtime removal.

The adapter retains the latest **64 debris requests** and **32 explosion reports**,
plus the existing 32 destruction records. They own names, vectors and resource
paths and survive source reclamation. All clear on unload. Finite copied requests
can still be recorded after a damage callback marks the source pending. They are
inspection records, not an implemented FX renderer or sound queue. Explosion
reports are stored after damage to include delivery/cancellation totals; this
recording order does not execute client effects.

The inspector exposes origin, velocity, dimensions, flags, amount, scale, resource
status and damage counters. Indexed does not mean decoded/loaded/played. Missing
optional FX resources do not prevent server damage. WorldModelDebris fragments,
client gib simulation/rendering, audio, decals, Pushable/gravity brushes,
SpawnObject, other explosion classes and save/restore remain pending. Pushable,
gravity and SpawnObject brushes still fail creation. Missing/ambiguous geometry
and older incompatible schemas remain explicit failures.

No retail executable comparison, additional platform or XR validation is claimed.
