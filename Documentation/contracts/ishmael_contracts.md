# Original Ishmael AI and revenant

The complete-level loading continuation integrates original IshmaelAI and
IshmaelREV with the [Orb](orb_contracts.md), original heal/shockwave weapons and
the existing checked AI lifecycle. [Source hashes](../implementation/sources/all_levels_ishmael_sources.json)
identify game code and the later-engine sphere-query reference. This does not
reopen M2 or certify complete campaign/network equivalence.

## Profiles, decisions and animation

Each variant has its own original static animation table, Ishmael.abc and either
Ishmael.dtx or Ishrev.dtx. Defaults retain PASSIVE, Cabal, ALWAYSRECOIL, hearing 0,
sensing 1000, sight 800, smell 0, walk 75/run 180/roll 105/jump 400, mass 250,
strength 5, health/armor 500 and original ammunition/three weapons. Shared
original difficulty and zero-random-health behavior remain in effect.

Health below .1 enters EscapeHide except when already there. Sense below .65 or
a higher-than-body target selects AttackFar. No-stimulus idle/search/escape states
return Idle; other states search visually. Close attacks teleport at <=20% sense,
reconsider at >=35% or excessive height, and otherwise use the shockwave. Far
attacks run beyond 75% at similar height and use Orb within 75%. Facing retains the
source metacommand increment/decrement. Ordinary Ishmael finishes command 7 after
target loss; the revenant immediately searches. Recoil dispatches teleport.

Standing fire uses FIRE_STAND[6], a nonlooping actual model clip, and original
FIRE_KEY callbacks. Selecting far fire adds 20 Focus before selecting Orb; close
fire selects the original shockwave. Heal selects its weapon and the same clip,
then multiplies current health by 1.1 only at completion, without maximum clamping.
Actual recipe 18 particles are published by the weapon's original fire callback;
resource/capacity failure is an unavailable shot. Base original invalid-ordinal
retention and checked physical-dimension behavior remain unchanged.

## Teleport and sound

Fade removes SOLID, and the revenant additionally removes GRAVITY. The first
update only changes flags; each later update subtracts exactly 1/16 alpha. At
zero, the source chooses laugh1..3 at radius 1000 and moves once along an
unnormalized random X/Z direction times sensing distance. The ray tests objects
without a gameplay filter. A hit backs off 100 times that same unnormalized
direction; a miss uses the full endpoint, preserving the source's unused backoff.
The existing replacement body solver checks the destination with solidity before
committing it, as for Gideon/Zealot. Reappearance restores alpha/SOLID and only the
revenant explicitly restores GRAVITY. There is no invented retry/search loop.

The mounted retail archives contain none of `sounds/chosen/ishmael/laugh1.wav`,
`laugh2.wav`, or `laugh3.wav`. Original sound requests therefore retain explicit
missing-resource diagnostics and failure counters; teleport continues as in the
source. No substitute sound/path or commercial-data change is supplied. The real
Zealot magic sound used by Ishmael's death is available and decoded.

## Death damage and sphere query

DEATH[0] restores alpha, removes gravity and sets original upward velocity 30.
At animation completion it publishes the actual Divine shockball and detached
magic sound, then visits a checked snapshot of touching objects. Only MODEL and
WORLDMODEL objects other than self receive damage. Each receives its own integer
25..50 draw, attributed to Ishmael, with no distance falloff. The direction is
old velocity plus global force times -.15 plus radial direction times
`max(0,450-distance)*5`. The source velocity/rotation mutation statements are
commented out and remain absent. A coincident center has a finite zero radial
term. Ordinary target damage physics still decides its own response. Removal and
inter-object callbacks are deferred; dead/pending identities are skipped.

`CollectObjectsTouchingSphere` uses a bounded owned snapshot, including inactive
alive objects. The later LithTech reference checks position/dimensions box overlap
then strict distance against query radius plus dimensions magnitude plus .1.
The implementation uses these same geometric tests with stable slot order,
rather than claiming the unavailable retail spatial-tree order. This is distinct
from a center-only radius or a closest-point sphere/AABB test. Invalid input clears
output and fails; pending/stale objects are excluded. No generic successful stub
or destructible-only substitute is used.

## Effects and inspection

Heal18 retains the source offset (-25,0,0), ring emission, rotating system,
18 particles per .1 seconds, radius 500, position radius 20, gravity 150,
blue/white color, .75..9 lifetime and final .25-second alpha ramp over duration
2.5. It uses the original Lensflare_1.dtx, without changing other object recipes.

Divine shockball22 uses exp_sphere.abc/divineball.dtx, scale 100 to 1000 over .5
seconds, alpha .5 and original waveform/fade. Its red aligned ring has 32
particles, radius 7500, position radius 25, velocity 1000 and duration .5.
Offsets preserve the raw supplied normal (ring *30, light *15), including the
shockwave's ground-ray-miss case. The red point light requests radius 300 to 150
over one second. Point-light state exists; world illumination remains unavailable,
as in the existing graphics scope. No nonexistent sprite/flame/fragment component
is reported as required. All actual component/resource failures remain visible.

The inspector exposes variant, heal/teleport counts, death deliveries and effect
errors. Deterministic RNG, bounded physics/particles and the checked sphere order
are portability/replacement policies, not retail numerical parity claims.
