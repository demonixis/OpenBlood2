# Downtown grenade contact — 2026-10-25

Reported: `13_downtown`, time 5.600331791, M16 alternate fire at two enemies,
`CGrenade: Grenade box flight unavailable/invalid (including embedded starts)`.
That message groups several failure causes; the exact reported encounter has
not been reproduced.

## Reproduced defect and correction

A controlled two-character test reproduces the flight refusal: a character moves
into a nonsolid grenade before its next projectile step. The solver rejected the
overlapping start before `CGrenade::HandleContact` could run. The original
`GameProjectiles.cpp:139` explodes an unbounced grenade on a living character.

The replacement solver now delivers precisely that contact without moving the
grenade. Original deferred explosion/damage, both sequential passes, owner
exclusion, stable first-hit selection and callback removal remain intact. Other
embedded starts stay refused. Failure diagnostics include the query, substep,
position, velocity, penetration, polygon, obstacle and remaining bounces.

## Verification

- `assault.overlapping_contact` fails before the correction and passes after it:
  two overlapping actors, unchanged grenade position, one touch, damage to both,
  two explosion passes, plus removal from an overlapping target's touch callback.
- `assault.failure` retains embedded-world refusal even with a living character
  at the same location, and checks the detailed failure diagnostic.
- Release: 44 focused tests passed (M16, proximity/remote bombs, inventory use,
  service coverage and private data), plus all 14 object-contract tests. The game
  executable builds; the final M16/object run passes all 30 tests.
- Clang ASan/UBSan: 27 synthetic M16/object-lifetime tests passed. Leak detection
  is disabled on this macOS sanitizer runtime; invalid memory and UB halt the run.
- `assault.downtown` fires two alternate M16 shots toward authored enemies from
  level entry and advances ten seconds. This smoke passed before the correction
  too; it is not a reproduction of the player's precise encounter.

An exploratory direct-fire stress probe (13 shots in 3.22 seconds) instead hit
`Smoke-trail particle capacity exceeded`. That separate VFX limit remains open;
no pool limit was increased or failure suppressed here. M2/M3 acceptance is unchanged.
