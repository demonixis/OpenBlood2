# Original sawed-off shotgun

This extends the [weapon](weapon_contracts.md), [impact](weapon_impact_contracts.md),
[AI fire](ai_fire_contracts.md) and [pickup](pickup_contracts.md) contracts. Sources:
`Shared/WeaponDefs.cpp` (WEAP_SHOTGUN, ID 4), `ObjectDLL/GameWeapons.cpp/.h` CShotgun,
`Weapon.cpp` Fire/FireSpread, `WeaponPickups.cpp` ShotgunPU and `AI_Mgr.cpp`
WeaponStrToInt/Fire (03_Museum Fanatic, ShotgunPU). Retail parity is not claimed.

## Definition and firing

The definition is copied verbatim: AMMO_SHELL, one shell primary / two alternate,
4..6 / 8..12 damage, 0.5 / 1.0 s reload, spread {200, 100} both modes, range 3500,
**sixteen vectors per fire** in both modes, strength 3 (two-handed 4), no projectile,
`Shotgun_pv.abc` for both hands and the `shotgun1/2.spr` flash sprites.

`CWeapon::Fire` now follows the original FireSpread loop for every weapon: one damage draw
shared by all vectors of a fire, then a horizontal and vertical spread draw per vector
(same order, from the same seeded generator), each vector traced and fired through the
existing FireVector/FireProjectile paths with its own impacts and damage message. Shells
cost the ammo use once per fire; other ammunition costs one use per vector, reduced to what
is available. Every previously selected weapon has one vector per fire and keeps its exact
former behavior (same draws, cost and result). The no-consumption collision preflight and
the post-hearing re-trace now cover every vector; a callback that removes the shooter stops
the remaining vectors. `last_pellets` reports the vectors of the last accepted fire and
`last_shot` its final vector.

The source lets two-shell alternate fire with one shell left and stores a negative count;
the port's existing ammo setter refuses negative counts, so that case stays the documented
dry refusal (the client view weapon also requires the full alternate cost). `CShotgun::Fire`
only selects shell ejection before `CWeapon::Fire`; casings are a client effect that is not
ported.

## Pickup and NPCs

`ShotgunPU` gives WEAP_SHOTGUN with 8 shells (resource 351, `Shotgun_pu`), may be spawned by
containers/string creation and is the drop class for a held shotgun. `WeaponStrToInt` maps
the `SHOTG` prefix to the shotgun; NPC fire uses the default `r_gun` branch like the Beretta,
and FindBestWeapon/raw-ammo selection is unchanged. The Fanatic's default `COMBAT` weapon
still resolves to the Beretta (the add-on combat shotgun is not compiled in the patched
retail object.lto).
