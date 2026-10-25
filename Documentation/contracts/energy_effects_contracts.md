# Energy, boss and rift effects — VFX lot 8

The source is Blood2's high-detail `ClientShellDLL/ExplosionFX.cpp` and `ObjectFX.cpp`,
with the existing `ObjectDLL/GameProjectiles.cpp`, `GameWeapons.cpp` and `Spawner.cpp`
callers. [Verification](../implementation/reports/energy_effects.md).

- Energy blast uses FIRETRAIL_2 (4), Ground Strike GROUNDFLAME_2 (27), both with half
  particle lifetime; Shikari uses BUGSPRAY_1 (31) at quarter lifetime. Preserve
  initial-update-before-Setup's default EnergyBlast branch, stopped emission clocks,
  the ground flame's ramp duration without an alpha ramp, and no GroundStrike impact.
  Restore republishes one trail; save payloads and damage remain unchanged.
- Flame-small (12) uses its 1/8/2-second flame phases and eleven-second light; the
  35-second gray light retains the duplicate Radius1 write. Blue shockball (19)
  keeps its source model, ring and light, independently of Divine's red effect.
- Singularity (18) and Spawner rift (23) each create two models. The second retains
  the first model's Z rotation. Singularity absorption attaches particle sphere 9
  to the hole, with inward speeds drawn from the reversed -125/-175 endpoints.
  Rift 24 is also represented, without claiming an exercised campaign caller.
- Death Ray impact (25) and alternate power rings (37/38), plus target electricity
  (39), retain their distinct geometry, colors, absolute emission clock and lifetime.
  The weapon owns one ordinary invisible BaseClass anchor, created lazily for the
  first alternate shot and removed with the weapon. Subsequent shots reposition it,
  just as the source does; it stays still between shots. Rings use the original
  eye-height/player rotation and swapped muzzle offset. This cosmetic anchor has
  no save payload; a load clears client effects and recreates it on the next shot.
- Naga eye impact (28) retains the endpoint-as-normal message quirk. Spike (29),
  stone chunk (30) and ground pound (31) use their respective smoke, rings and
  fragments. Fragment loop bounds are drawn each iteration; fragments have no
  smoke trail. No new camera shake is introduced.
- An absent component in a supported composite succeeds with an empty component
  list. Missing required art and invalid inputs still report failure. All 32 base
  composite recipes now have source-selected components; that is not visual parity.

Reuse the bounded client pools, private cosmetic RNG and existing fragment collision
adaptations. Power-ring emission uses a stable finite basis when the source's local
up/forward alignment degenerates. High-detail port verification is separate from an
original-executable A/B comparison. Evaluated lights still require lot 10 to illuminate
geometry. No commercial assets, shader artifacts or dependencies are changed here.
M2-GPU-001 and M3 campaign acceptance remain unchanged.
