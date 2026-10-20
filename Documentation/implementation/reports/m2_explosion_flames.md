# M2 continuation: original explosion flame particles

CExplosionFlameFX now runs its original creation/update/emission rules, connected
to the selected high-detail Tesla alternate explosion recipe. Its three groups
produce actual colored particles through the existing SDL_GPU alpha pass, using
external teslaprime_3.dtx. Inspection exposes their clocks, emission deadlines,
colors, resources, prepared geometry and retained failures. **M2 remains incomplete.**

The source emits immediately and then one burst per due update. It advances the
previous deadline, applies the five fade formulas and removes the whole system
at duration, even when individual particles still have time remaining. Authored
normal/ramp times/ramp flags are unused by the source methods and remain unused.
With fade enabled, creation alpha is 1 until the first update. These details have
behavioral checks; no guessed ramp or smooth catch-up behavior is substituted.

The missing bulk particle service uses an explicitly documented later-engine
sampling policy, with six draws and shared velocity.x/lifetime interpolation.
It is not claimed as Blood II retail RNG/physics/raster parity. Budgets reject
oversized bursts before emission or RNG consumption; errors remain observable.
Successful siblings and detached lifetime are preserved. No dependency changes,
commercial files, extra gameplay integrator or new ECS are introduced.

See [contracts](../../contracts/explosion_flame_contracts.md),
[source provenance](../sources/m2_explosion_flame_sources.json) and
[validation records](evidence/m2-explosion-flames.json).

## Validation

Debug passes **550/550 headless checks** (84.07 s)
and **41/41 GPU checks** (170.16 s). Inspector-free Release passes
**550/550 headless checks** (7.18 s) and **41/41 GPU checks**
(108.54 s). Each totals **591/591**, including 117 private-data
checks (85 headless). Targeted Clang ASan/UBSan/leak checks pass **58/58**
(184.54 s). Both GPU matrices contain no Vulkan validation
error/VUID diagnostic.

Commands: `cmake --build build/debug -j 3`, `cmake --build build/release -j 2`,
then `ctest --test-dir build/<configuration> -L headless -j 4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use existing Clang `build/sanitize`, ten affected test targets and
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` with CTest
regex `^(explosion_flames|object_particles|explosion_fragments|explosion_rings|explosion_lights|explosion_models|explosion_sprites|sceneexplosion|sprite_view|objects)\.`.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
17 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve. Evidence
retains source hashes, the committed project snapshot and patch.

The three diagnostic cycles reach 79 / 81 / 80 live particles and observe eight
bursts before expiry. The Cathedral records each emit 24 particles immediately
and 48 after .35 seconds; their resources decode and terminal updates remove them.

The private scenario explicitly creates a diagnostic Tesla emitter in SubwayA for
three player/unload cycles at 50/60/20 Hz. SubwayA's nine authored explosions remain
grenades. The three actual Tesla property records from 07_cathedral are also loaded,
triggered and checked, without claiming a complete Cathedral session. GPU checks
render the external Tesla particle texture without an inspector and after reload.
GPU readback verifies 3,875 opaque-texel color/alpha blends. Each of two private
reload cycles renders 48 particles covering 20,592 pixels; the real texture has
2,000 partial-alpha texels. The capture shows blue Tesla sparks with varying size
from perspective, matching the decoded resource. Captures remain in ignored build directories.

SubwayA source accounting is unchanged: **203 created, four transferred sounds,
372 editor-only, one unsupported Gideon, zero failures**. Illumination, safe automatic
NEW_LEVEL, other required FX/callers and the actual M2 gate remain open. This is not
M3 completion or a verified retail/platform comparison.
