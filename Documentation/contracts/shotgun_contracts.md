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
selects shell ejection before `CWeapon::Fire`; the client effects are implemented below.

## Pickup and NPCs

`ShotgunPU` gives WEAP_SHOTGUN with 8 shells (resource 351, `Shotgun_pu`), may be spawned by
containers/string creation and is the drop class for a held shotgun. `WeaponStrToInt` maps
the `SHOTG` prefix to the shotgun; NPC fire uses the default `r_gun` branch like the Beretta,
and FindBestWeapon/raw-ammo selection is unchanged. The Fanatic's default `COMBAT` weapon
still resolves to the Beretta (the add-on combat shotgun is not compiled in the patched
retail object.lto).


## Original VFX completion, lot 2 (2026-10-25)

The [source ledger](../implementation/sources/shotgun_effect_sources.json) covers the original
client/server CShotgun, ViewWeapon, WeaponFX and effect implementations. The
[verification report](../implementation/reports/shotgun_effects.md) records the checks.

- The first-person muzzle uses `shotgun1.spr` / `shotgun2.spr`, scale 0.19, duration 0.05 s
  with the original extra `frame time + 0.05` minimum. Only an accepted FIRE_KEY starts it;
  dry/blocked fire does not. The animated `muzzle` node and original offset-times-ten placement
  follow the view weapon. Desktop applies the -20-unit sprite depth bias while preserving
  projected size (later LithTech renderer corroboration); XR attaches to the actual tracked
  muzzle, with the corresponding model scale and no desktop projection/bias. Hidden/dead
  weapons do not display flashes. Both authored sprite clocks run while hidden.
- Client CShotgun alternates primary `fire` and `alt_fire` animations on returning to REST.
  The second barrel remains **primary** for damage, cost, flash and fire sound. An alternate
  request with that barrel pending becomes primary. Animation SOUND_KEY reloads now play
  locally; doubled separators in the retail key are collapsed before strict audio validation.
- Server CShotgun selects ejection only for alternate fire. UpdateWeaponFX consumes the bit
  at the **first contact**, even if a fire-through brush/non-liquid volume suppresses its
  AddImpact. The first published contact may therefore create two casings and one muzzle
  smoke; later pellets/penetrations never repeat them. A complete miss creates neither, as
  in the source. The local muzzle flash still appears. All sixteen rays, seeded damage and
  spread, ammo costs, collision preflight and damage callbacks remain unchanged.
- Casings use `models/ammo/sshell.abc`, `skins/ammo/sshell.dtx`, scale 2.25, up 60–90,
  right 50–70 (mirrored left hand), forward 10–25, angular velocities ±4π. InitMovingObject
  precedes HALFGRAVITY: acceleration is -250 dry / -125 liquid. Position integrates before
  velocity; collision reflects with 0.35 / 0.20 damping. Two contacts at most play material
  `ShellDrops/.../Shell1..2.wav` sounds at radius 150, then rest. Expiry is strictly after
  20 seconds; the 100-slot pool replaces the oldest. Effects retain no owner pointer.
- Muzzle smoke keeps the shell-specific 0.04→0.25 scale, alpha 0.25, duration 1 s,
  directional velocity/deceleration, random drift and B2_LITE smoke variants. Underwater
  source/destination substitutions emit one shell-ammo bubble per request through the
  existing CSparksFX path, using the authored bubble texture, colors and positive gravity.
- Wood/stone impacts add 1–3 original rotating splinter/chip sprites, including the
  cumulative velocity perturbations, 3-unit normal offset, upward-normal duration/speed
  branch, liquid gravity halving and **per-update** motion/rotation. Existing impact flash,
  smoke, metal/flesh sparks, blood and impact sounds still execute through their shared paths.
- Marks use the original material mapping, 0.035 scale, normal alignment and 0.1 RGB.
  Glass/flesh/sky/unknown do not receive a mark. At eleven existing marks within squared
  distance 100, move the nearest encountered mark, preserving its texture/rotation; global
  capacity is 300. Marks are fixed world positions, including those on moving brushes.
- Liquid contacts create the original four-drop bursts (every effect update for 0.3 s),
  normal velocity 175, ±10 spread, ±1 origin radius, 0.75 s particle lifetime, -500 gravity,
  radius 100 and authored VolumeBrush colors. The aligned ripple is scale 0.1, alpha 0.25,
  duration 1 s with no fade or growth, as specified by WeaponFX.

### Ownership and explicit engine boundaries

`ShotgunEffects` owns bounded client-only models, sprites, particles and 64 copied reports;
its RNG never consumes gameplay randomness. Publication is refused during MID_SAVEOBJECT.
Missing/corrupt cosmetic resources and update failures remain inspectable and never undo
an accepted shot. New sprite/model textures are validated before admitting the effect.
World unload/load/reset clears every transient pool; no saved class or payload is added.
View-weapon recreation resets barrel/flash state, consistent with existing client restoration.

Effect source positions follow the existing weapon contract: the player's camera-origin
shot or the NPC's authored firing origin. The original muzzle-origin firing/near-wall
correction remains outside this lot; the visible first-person flash does follow the muzzle.
As in destruction lot 1, casing collision is the replacement primary-world finite segment
service: no moving-brush collision or original outside-world classification. Marks use
normal-aligned quads; the original ClipSprite polygon clipping service is unavailable.
The original mark's one-unit clip query also starts after the 1.25-unit impact offset.
Fragment/ripple/splash pools each cap at 80, alongside the port's existing separate smoke
and spark pools, rather than recreating the legacy shared heterogeneous list. An empty
parentless splash is retired after its final particle; the original can retain it forever.
These engine adaptations, CRT-independent random bits and no manual headset validation
prevent claiming pixel-identical retail parity. M2 closure/reservation and M3 acceptance
are unchanged; the remaining VFX inventory is lot 3.
