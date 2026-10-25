# Original Tesla Cannon

Selected original `GameWeapons.cpp` CTeslaCannon, `WeaponDefs.cpp` entry 13 and
`GameProjectiles.cpp` CTeslaProjectile, CTeslaBoltProjectile and
CTeslaBallProjectile, with the TeslaCannonPU pickup and the AI `TESLA*` weapon
branch. Retail 05_CDM arms Fanatics with `TESLA`/`TESLACANNON` and a destructible
model spawns `TeslaCannonPU`.

## Weapon

The definition is copied unchanged: TYPE_AUTORIFLE, AMMO_BATTERY (8), primary
2 cells for 30–50 damage, alternate 25 cells for 100–150, reload 0.175/1.0 s,
spread 20/20, 1000 u/s, 3500 range, radius 100/150, looping primary animation.
CTeslaCannon::Fire increments `numAltFires` on every alternate Fire call, before
the ammo check (dry shots included). Only the second alternate shot since the
weapon was created is a `CTeslaBallProjectile`; every other one is a
`CTeslaBoltProjectile`. The source never initializes or resets the counter
(SetSpecialData has no caller); it starts at zero here. The alternate shot's
SFX_LIGHTNING_ID beam now uses the shared detached lightning service (VFX lot 6). TeslaCannonPU offers the weapon with 40 cells; destructible containers
may spawn it (bouncing string creation).

## Projectiles

- **Primary** (sprite `Teslaprime.spr`, Explode damage): hidden for its first
  0.01 s, then scaled 0.175; plays an attached retained `projectile.wav` loop
  (600, medium) killed at explosion; explodes with the EXP_TESLA_PRIMARY (7) recipe
  and `impact.wav` (750). Its projectile light is unavailable.
- **Bolt** (model `Lightining.abc`, Electric): its first update casts 2000 units
  along its forward vector (solid objects/world, LiquidFilterFn as volume
  exclusion, one recast past the shooter), teleports to the hit, applies radius
  damage there and is removed. It has no explosion; no hit means no damage.
- **Ball** (same model, Electric): the first update casts the same ray, teleports
  to the hit (or the ray end, normal up), explodes with EXP_TESLA_ALT (8) and
  `altimpact.wav` (2000), applies radius damage and stops. Every 0.1 s for 2.5 s
  it then fires one bolt, at most every 0.5 s, at the first character in the
  destructable list within an attraction radius growing 100 → 400 that is not the
  previous target: 100 Electric radius damage (radius 100) at the target and a
  random `althit1–3.wav` (1500). The shooter is not excluded, as in the source.
  Bolt and ball never fly between creation and their first update (replacement
  physics would only move them one frame).

Electric (13) is a typed damage type with no special handling, as in the source.
Save/load lot 9 covers the projectile family; the original unsaved attraction clocks
remain the documented restore behavior. Projectile-light illumination remains unavailable.

## Original VFX completion — lot 6 (2026-10-25)

[Verification](../implementation/reports/tesla_effects.md) and
[source hashes](../implementation/sources/tesla_effect_sources.json).

- Tesla alternate and Drudge lightning preserve the separate unfiltered solid-object/world
  visual ray, starting at the fire position, with the endpoint at the first hit or full
  weapon range. Both publish boxed / thin-to-thin / high-segment lightning (2/3/2).
  The Tesla projectile's independent 2000-unit damage ray and the Drudge ordinary damage
  filters remain unchanged. A visual failure never suppresses the damage/projectile path.
- Ball attraction publishes boxed / wide-to-thin / high-segment arcs (2/1/2) from the ball
  to the selected character before radius damage. Target order, previous-target exclusion,
  shooter eligibility, strict clocks and audio remain original. Inspectors expose actual
  event IDs, successful arc counts and failures; no unavailable counter stands in for FX.
- Impact 7: default explosion sphere model, eballsmall skin, scale 20→100, duration .5 s,
  alpha .99, waveform/fade/random-rotation 1; TeslaImp sprite at the impact, independent
  X/Y scales .25–.5→.5–.75, 1 s, alpha 1, waveform 0, fade 1, align 2 (random roll).
- The delayed ring has 24 particles using Smoke32_1, offset 10 along the normalized normal,
  color (.5,.5,1), radius 4000, position radius 10, velocity 150–250, zero gravity,
  duration .75 s, rotation −.08–.08, alpha .5, delay .15 s, fade 1 / rotate 2 / align 1.
  The evaluated light state is at offset 25, blue→white, radius 100→0 over 1 s.
- Primary requests model/sprite/ring/light; alternate keeps its existing model/sprite/flame/
  light recipe 8. No nonexistent fragments/flames are added to primary. Generic type-7
  dispatch also treats the absent families as successful no-ops, so EnergyBlast callers
  gain the same impact without phantom missing-component errors. Their missing trails
  and divine type-12 impact remain for lot 8.

The existing lightning service owns its detached particles and light states, bounded
capacity, .25 s particle fade and .5 s light lifetime. Source random-form typo, integer
segment scaling and explicit uninitialized-origin adaptation remain in the
[lightning contract](lightning_contracts.md). No light illuminates geometry yet (lot 10).
Portable per-event cosmetic random streams and the existing replacement rasterizer do
not claim retail random-bit or pixel parity. No save bytes, source assets or gameplay
parameters change; M2's reservation and M3 acceptance remain unchanged.
