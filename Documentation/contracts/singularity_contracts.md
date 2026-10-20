# Original Singularity generator

This extends the [weapon](weapon_contracts.md), [shotgun FireSpread](shotgun_contracts.md),
[Naga beam](naga_weapon_contracts.md), [Tesla](tesla_contracts.md) and
[AI fire](ai_fire_contracts.md) contracts. Sources: `Shared/WeaponDefs.cpp` (WEAP_SINGULARITY,
ID 8), `ObjectDLL/GameWeapons.cpp` CSingularity, `GameProjectiles.cpp/.h`
CSingularityProjectile, `WeaponPickups.cpp` SingularityPU, `ClientShellDLL/LaserBeamFX.cpp` and
`AI_Mgr.cpp` WeaponStrToInt (03_Museum Prophet). Retail parity is not claimed.

## Weapon

The definition is copied verbatim: AMMO_BATTERY, 50 per fire in both modes, 5..10 damage,
5 s reload, no spread, **zero projectile velocity**, range 3500, damage radius 500 and
`CSingularityProjectile` for both modes. `CSingularity::FireProjectile` first queries
solid objects without a filter from the fire position along the range (skipping the
interior it starts in) and publishes the `LASER_BLUE_LARGE` burst beam to the hit or the
range end; a beam failure never suppresses the projectile. `SingularityPU` gives the
weapon with 100 cells (resource 365) and is its drop/spawn class. `WeaponStrToInt` maps
`SINGULAR`; NPCs aim it through the default `r_gun` branch.

`CLaserBeamFX` now follows the original `SetupBeam` table for every LASER type: small beams
scale 0.05..1 over 0.25 s and last 0.5 s; large beams 0.1..2.5 over 0.35 s and last 1 s,
both fading their 0.5 alpha over the last fade time, with the per-colour `beam*.dtx`
texture. The Naga red-small beam is unchanged.

## Projectile

The projectile has no file, so the original precreate falls back to the grenade model and
skin; its zeroed initial scale keeps it invisible. The port's ScaleObject refuses zero model
scales, so it keeps scale 1 and omits `FLAG_VISIBLE` instead (dimensions stay 1 unit and
`FLAG_RAYHIT` is kept). Its damage type is the original `DAMAGE_TYPE_SINGULARITY` (3), now
accepted by typed damage messages and saved damage state. It never flies.

Its first update (0.1 s cadence) casts forward 3000 units against solid objects with the
liquid filter, recasting once from the shooter's hit point when the shooter is first. It
arms 25 units off the hit point along the normal (world up on a miss), adds the
`EXP_SINGULARITY_PRIMARY` (18) explosion and the `Singularity\impact.wav` sound (radius 2000),
and stops. For five seconds it walks the original destructable list: other live OT_MODEL
objects with `USRFLG_SINGULARITY_ATTRACT` within 600 units are pulled by
`(600 - d²/600) * 0.75` toward it each update; characters within half the squared radius take
the projectile damage; inside 50 units an object is stopped and moved to the centre, then a
character takes double damage and anything else is removed (its `OBJFX_SINGULARITY_1` client
effect is counted as unavailable). Damage messages come from the projectile, attributed to
the shooter. Removal is deferred, so the list stays valid across damage callbacks.

Characters now carry `USRFLG_SAVEABLE | USRFLG_SINGULARITY_ATTRACT` from CBaseCharacter
initialization, like projectiles, corpses and pushable destructible models already did. The
EXP_SINGULARITY_PRIMARY composite recipe has no client components yet; its publication
results remain in the scene-explosion report.

Without any monster, a Cabal NPC's Enemies list falls back to the Cabal list, which contains
the NPC itself; tests without a player place a monster.
