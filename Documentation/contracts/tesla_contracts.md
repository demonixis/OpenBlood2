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
SFX_LIGHTNING_ID beam to the first solid hit is client-only and counted as
unavailable. TeslaCannonPU offers the weapon with 40 cells; destructible containers
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
Save/load, the projectile lights and the lightning SFX are not ported.
