# M2 — Original flesh and metal impact sparks

Original `CSparksFX` now supplies the flesh impact burst alongside the existing
blood jets, plus the yellow/white sparks from metal vector-weapon contacts.
Both render real external DTX particles and expose their lifetime, recipe,
resources and failures in the in-game inspector. **M2 remains incomplete.** Implementation commit: `0d98741`.

Read the [contracts](../../contracts/spark_contracts.md),
[source ledger](../sources/m2_spark_sources.json) and
[validation ledger](evidence/m2-sparks.json).

## Preservation and adaptations

The source chooses flesh bursts independently of blood-spurt probability. Counts
use raw sampled damage, integer truncation and the original threshold50/caps.
Metal retains its 1-in-20 larger variant. The source direction-normalization and
component clipping remain intact, as do gravity-150, emission radii, particle
radius/640, colors, B2_LITE blooddrop variants and creation-time fade clocks.
There is one burst; particles have no collision or emitter integration.

Cosmetic selection stays on the existing per-weapon stream; particle interpolation
uses a separate owned stream. These new effects never consume gameplay RNG or
cancel damage/hearing. Later LithTech supplies evidence for the six-fraction
AddParticles mapping and the existing particle integrator; retail executable or
bitwise RNG parity is not claimed.

The source class only returns false after duration when WantRemove is set.
Detached systems would otherwise remain empty until shared-list eviction. The
session manager reclaims these exhausted invisible systems strictly after duration,
without truncating live particles. This memory adaptation is documented and tested
separately from the original class behavior. World unload clears owned effects;
IDs survive reload. No target, source or weapon pointer is retained.

A melee hit on another surface sets the original spark flag but encounters an
empty zero-duration recipe. This stays an explicit diagnostic, not fabricated
particles. Missing textures, invalid data, stale sources and exhausted capacity
also fail visibly. The inspector distinguishes requested events, live systems,
retired systems and rendering failures, without advancing clocks.

## Validation

Debug with inspector, Release without inspector, and Clang ASan/UBSan with leak
detection each pass **69/69 headless tests** (18 private-data cases). Debug and
Release each pass **9/9 NVIDIA GPU tests**, with no Vulkan validation errors.
The device logs identify NVIDIA GeForce RTX 3060 Laptop GPU. Exact commands and
logs are retained in the validation ledger. Tests cover burst bounds, component thresholds, raw-damage recipes,
large/small metal, strict fade/retirement, stale sources, particle/system limits,
missing/decode failures and repeated real-world reloads. The private fixture fires
a real original Cultist Beretta at another Cultist on a labelled diagnostic floor;
it verifies damage, detached effects and all three blood textures plus particle2.
Each private cycle decodes 582 particles from the real weapon and explicit
texture probes. This is not a complete authored-level traversal.

The existing combat/corpse-clearance assertions and level-transition tests remain
unchanged. GPU readback uses NVIDIA and checks both red and yellow real textures,
subsequent movement, source-independent rendering, expiry to a clear image and
three unload/reload cycles. Readback records 1107 red and 27 yellow pixels in the
initial 300-particle fixture, then an entirely clear image after expiry. The
captures were visually inspected and stay in ignored build directories.

A 30 Hz precision tail was also reproduced: double-precision session time can
cross duration while float particle lifetimes remain just positive. Presentation
clamps negative alpha to zero, while the raw original value remains inspectable.
The new boundary test passes in all three configurations. All 10,923 reference
files match the inventory hashes and all six reference repositories remain clean.

## Remaining gate

Underwater smoke bubbles, generic/fade-color spark callers, shared SFX eviction,
wood fragments, outside-world classification and other required original services
remain unavailable. This increment changes no source repository, commercial asset,
shader, dependency pin or M2 acceptance criterion. Native Metal execution is not
established by Linux NVIDIA tests.
