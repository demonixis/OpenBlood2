# M2 continuation: original explosion particle rings

The selected original CExplosionRingFX and high-detail CExplosionFX fire/smoke ring
recipes now run in Blood2. A bounded particle integrator and the existing SDL_GPU
alpha pass render actual moving DTX-textured particles. This extends the previous
[point-light state](m2_explosion_lights.md), [models](m2_explosion_models.md) and
[sprites](m2_explosion_sprites.md). **M2 remains incomplete.**

## Behavior and boundary

Each supported fire ring has 32 radial particles. Grenade type 3 adds three delayed
smoke rings. Original source offsets, velocities, gravity ranges, delay/fade rules,
cached-right orientation and per-update rotation remain separate from new engine
integration. Source removal leaves detached particles alive; all state is cleared
on unload. The original null damager still delivers zero damage. Partial particle
failure does not cancel another component or sound.

The engine implements checked lifetime, explicit position-before-gravity stepping
and camera-facing quads. BaseParticleSystemFX's radius/640 determines quad half size.
Later-generation LithTech references justify this bounded policy; **retail Blood II
motion and pixel parity have not been verified**. Parallel orientation vectors use
the existing deterministic fallback. Zero origin is initialized explicitly. The
private RNG stream preserves the nine ring draws' order/ranges, not full composite
random sequencing. No new dependency, ECS or commercial file is imported.

Real DTX alpha and system color/alpha feed the shared, stably sorted sprite pass.
Inspection includes delayed/hidden state, individual particle position/velocity/life,
resource failures and retained delivery reports. It is read-only and survives
folding/hiding the inspector. Full composite availability remains false: fragments,
flames and surface illumination are still missing. Safe automatic NEW_LEVEL remains
refused before teardown. SubwayA counts are unchanged: **203 created, four transferred
sounds, 372 editor-only, one unsupported Gideon, zero failed entries**.

See [contracts](../../contracts/explosion_ring_contracts.md),
[source provenance](../sources/m2_explosion_ring_sources.json) and
[machine-readable evidence](evidence/m2-explosion-rings.json).

## Validation

Debug passes **534/534 headless checks** (81.32 s)
and **35/35 GPU checks** (134.48 s). Inspector-free Release passes
**534/534 headless checks** (6.95 s) and **35/35 GPU checks**
(42.34 s). Each totals **569/569**, including 111 private-data
checks (82 headless). Targeted Clang ASan/UBSan/leak checks pass **42/42**
(93.99 s). Both GPU matrices contain no Vulkan validation
error/VUID diagnostic.

The first full Debug run found one obsolete aggregate-count assertion in the
existing private sprite test: it expected only 30 total quads. It now still requires
all 30 original sprites, plus the exact live ring-particle count and valid resources
for every entry. The complete matrix then passed; the initial failure is retained.

Commands: `cmake --build --preset debug -j 4`, `cmake --build build/release -j 2`,
then `ctest --test-dir build/<configuration> -L headless -j 4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use the existing Clang `build/sanitize` configuration, the seven affected test targets,
and `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` with
CTest regex `^(explosion_rings|explosion_lights|explosion_models|explosion_sprites|sceneexplosion|sprite_view|objects)\.`.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
23 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve. Execution
evidence retains source hashes, the committed project snapshot and patch.

New behavior checks include all fade/rotation formulas, radial planes, particle
gravity/expiry ordering, atomic invalid input, delay activation/overshoot, explicit
recipe RNG draws, capacity exhaustion, partial resource failures and identities across
unload. Private tests exercise all nine SubwayA emitters through three player/world
cycles: 36 systems, 1152 reserved particles, nine fires expiring before 27 smoke rings,
then complete cleanup. Rendering calls do not advance clocks.

New GPU checks compare full/half-alpha pixels and RGB multiplication using synthetic
DTX fixtures, then draw external Firepart64/Smoke32 textures. The existing complete
application smoke now additionally requires at least 32 valid fading particle quads
while the inspector is hidden, then repeats after reload. Existing sprite depth/alpha
checks remain in the matrix. Captures are generated locally under ignored build
folders; commercial textures are not copied into tracked evidence.

## Remaining work

This does not complete composite FX, surface lighting, Gideon or safe automatic
level transfer. Other original callers and low-detail recipes remain unconnected.
Windows/macOS/mobile, other GPU backends and comparison with the retail executable
were not tested. M3's playable slice and the unchanged M2 acceptance gate remain open.
