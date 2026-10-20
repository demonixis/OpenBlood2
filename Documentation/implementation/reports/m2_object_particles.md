# M2 continuation: original fragment smoke trails

Grenade metal fragments now create real smoke trails using selected original
CObjectParticleFX rules and the high-detail OBJFX_SMOKETRAIL_1 recipe. The existing
SDL_GPU sprite pass draws the actual smoke64_2.dtx texture with per-particle color.
This continues [metal fragments](m2_explosion_fragments.md) and
[particle rings](m2_explosion_rings.md). **M2 remains incomplete.**

The source places one particle per two units of traveled distance, in reverse
order along the segment. It skips its first update, compares emission delay against
absolute time and stops permanently when the parent stops, retaining the existing
particles through a .75-second tail. Parent removal immediately ends the trail.
The authored count 12 and alpha .25 are unused in this source branch; actual system
alpha remains 1. Tests preserve these quirks rather than introducing guessed effects.

Typed parent IDs and copied poses replace client pointers. Missing resources and
capacity failures remain visible. All particle work runs outside the UI; inspection
exposes parent/motion/deadline/resource/counter and individual particle state.
Per-particle RGB extends the existing bounded particle integrator. The stream skips
a source zero-count division that could produce unused NaNs, without changing the
number of emitted particles or RNG draws. Existing physics/raster policies remain
replacement engine behavior, not a verified retail comparison.

The sprite entry limit is now 16384, because the private 60 Hz scenario alone reaches
6304 simultaneous trail particles. Trail budgets are 128 systems, 2048 particles
per system and 8192 total. Exhaustion returns an explicit failure. There is no new
external dependency or commercial asset in Git.

See [contracts](../../contracts/object_particle_contracts.md),
[provenance](../sources/m2_object_particle_sources.json) and
[validation records](evidence/m2-object-particles.json). SubwayA remains **203 created,
four transferred sounds, 372 editor-only, one unsupported Gideon, zero failed
source entries**. General ObjectFX modes, ramps/multiplayer, remaining explosion
flames, surface illumination and safe automatic NEW_LEVEL are still unavailable.

## Validation

Debug passes **545/545 headless checks** (78.06 s)
and **39/39 GPU checks** (163.15 s). Inspector-free Release passes
**545/545 headless checks** (7.68 s) and **39/39 GPU checks**
(106.32 s). Each totals **584/584**, including 115 private-data
checks (84 headless). Targeted Clang ASan/UBSan/leak checks pass **53/53**
(188.05 s). Both GPU matrices contain no Vulkan validation
error/VUID diagnostic.

Commands: `cmake --build build/debug -j 3`, `cmake --build build/release -j 2`,
then `ctest --test-dir build/<configuration> -L headless -j 4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use existing Clang `build/sanitize`, nine affected test targets and
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` with CTest
regex `^(object_particles|explosion_fragments|explosion_rings|explosion_lights|explosion_models|explosion_sprites|sceneexplosion|sprite_view|objects)\.`.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
20 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve. Evidence
retains source hashes, the committed project snapshot and patch.

Private tests run three player/world cycles at 50, 60 and 20 Hz. They observe
27 trails each, peaks of 5872 / 6304 / 2936 live particles and complete expiry.
Observed emitted-particle maxima are 6819 / 6819 / 6716; frame-dependent movement
and sampling are inherited source behavior, not a new frame-rate-independent model.

GPU readback checks 2,914 synthetic covered pixels, including 992 opaque color
assertions. Five actual smoke64_2 quads cover 7,090 pixels; their texture contains
2,769 partial-alpha texels. Application checks also exercise drawing with hidden
inspection and reload. Captures remain in ignored build directories. Other
platforms/backends, global retail RNG and retail executable comparisons remain
unverified. M3's playable slice is not claimed.
