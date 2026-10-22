# Low-ceiling steps: duct exits

22 October 2026. The user reported that a crouched player can sometimes enter a
duct but not leave it, blocked by its own collision, and related it to the
recent [airborne-step fix](cityhub_jump.md).

## Diagnosis

Crouching itself is sound. Caleb's crouched and standing boxes differ only in
height ((12,28,12) and (12,42,12)), `ResizeObject` keeps the feet, and a
standing attempt that is blocked keeps the short body, as the original
`SetObjectDims2` refusal does. Seeded CityHub roams with frequent crouching
(60 s each, four seeds) found no standing attempt refused only by the 0.01-unit
[contact tolerance](../../contracts/contact_overlap_contracts.md): every refusal
was real geometry above the head.

The trap is in the replacement stair solver. Its up path lifted the body by the
whole step limit (half the current half-height, 14 units crouched, 21 standing)
and refused the step unless that entire lift was clear. Any step therefore
needed 2.5 half-heights of headroom: 70 units crouched, 105 standing. CityHub's
ducts are 64 units high and 09_Abandoned_Buildings' 68; the crouched body (56)
has 8 or 12 units to spare, so no lip could be climbed inside one. A duct whose floor is a few units below the room
floor is entered by dropping in and cannot be left. Standing players met the
same refusal on steps under ceilings lower than 105 units.

Later LithTech's `StairStep_Segment` (`runtime/shared/src/collision.cpp`) does
not lift by the limit. It raises the body by the actual intrusion of the
polygons under its lower band, then collides the rest of the body normally, so
only the real step needs headroom.

## Change

The up path now stops 0.001 below a ceiling (or other solid) and the across and
down paths start from the reached height. A step succeeds when its landing rise
is positive and within both the limit and the reached lift. The limit, the
no-upward-velocity rule, the walkable landing and every other gate are
unchanged; a step taller than the headroom is still refused. Like the existing
solver this is replacement physics, not a trace of the 1998 executable.

## Evidence

`locomotion.duct` fails without the change and passes with it:

- crouched, a 60-unit duct (8 units over the fixture's 52-unit crouched body)
  whose floor is 6 units below the room: the player crawls out onto the room
  floor, stands, crouches back in (dropping 6 units), crawls out of the open end
  and stands again;
- standing, a 6-unit step under a 90-unit ceiling (8 units of headroom);
- crouched, a 10-unit lip in the same duct stays refused.

A static scan of the 26 installed single-player worlds (8-unit grid, primary
world geometry, neighbours 16 units apart) looked for a step within the limit
whose lower side has less headroom than the old full lift needed while both
sides fit the body. It is a sampling, not an exhaustive search. It found:

| World | Stance | Rise | Headroom (lower / upper side) |
| --- | --- | --- | --- |
| 09_Abandoned_Buildings | crouched | 4 | 68 / 64, ducts on several floors |
| 05_CDM | standing | 8 | 87–96 / 87–88 |
| 06_Airship1 | standing | 8 | 104 / 96 |
| 07_Cathedral | standing | 16 | about 100 / 98 |
| 15_Crypt | standing | 16 | 104 / 88 |

A dynamic probe then crawled a crouched Caleb for 0.25 s in eight directions
from each of the 5,067 crouch-only spots of 09_Abandoned_Buildings. It recorded
362 steps (rises 3–4 units, 11–12 units of headroom) whose full 14-unit lift hits
the ceiling, i.e. steps the former solver refused. The same probe stops once on
both solvers after a teleport into the `GRATE2` destructible brush; that start
penetration is a probe artifact, unrelated to stepping.

The private `locomotion.low_ceilings` replays two of these spots and fails
without the change: a crouched Caleb climbs the 4-unit duct lip at
(-752, 956, 2700) in 09_Abandoned_Buildings (previously stuck at 956), and a
standing Caleb climbs the 16-unit step at (160, 16, -512) in 15_Crypt under a
104-unit ceiling (20 units of headroom for a 21-unit limit).

The other locomotion, ledge, stair and contact cases are unchanged.
