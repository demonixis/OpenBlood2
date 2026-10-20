# M2 continuation: original explosion metal fragments

The high-detail grenade branch now executes selected original CExplosionFragFX
launch, rotation, collision, rest, fade and expiry behavior. Actual gibmetal2.abc /
gibmetal3.dtx resources render through the existing SDL_GPU model pass. This extends
[particle rings](m2_explosion_rings.md), [models](m2_explosion_models.md) and
[scene emitters](m2_scene_explosions.md). **M2 remains incomplete.**

## Observable behavior and adaptations

Original fragments advance velocity, position and Euler angles per effect update,
not per elapsed second. Their 1.1-velocity ray reflects on world/object geometry,
lifts them one unit off the surface and halves angular speed. The strict source
rest test stops both rotation and gravity on suitable ground contact. Fade uses
subtraction and strict endpoint conditions; the final update still moves before
removal. A private random stream preserves the source loop's newly drawn bound on
every condition, interleaved with launch draws. It does not reproduce global retail
random sequences.

The existing typed segment service supplies primary-world, transformed brush and
object AABB hits. This is replacement engine behavior, not verified retail client
collision parity. Original indeterminate flags/last-normal fields are initialized,
invalid queries fail, and no collision is silently replaced with a miss. Fragment
models use checked detached identities and never apply damage or player movement.
Source removal leaves them alive; unload clears all owned state.

Inspection exposes position, velocity per update, rest/impact state, last polygon or
checked object, resource errors, elapsed time and alpha. Model construction/rendering
is read-only. The original OBJFX_SMOKETRAIL_1 request is visible as unavailable;
no placeholder trail claims success. Child explosion modes and other fragment
recipes remain rejected. Missing trails/flames/illumination and safe automatic
NEW_LEVEL still block the full session gate.

See [contracts](../../contracts/explosion_fragment_contracts.md),
[source ledger](../sources/m2_explosion_fragment_sources.json) and
[validation records](evidence/m2-explosion-fragments.json). SubwayA accounting remains
**203 created, four transferred sounds, 372 editor-only, one unsupported Gideon,
zero failed source entries**. No reference repository or commercial asset is changed.

## Validation

Debug passes **540/540 headless checks** (77.36 s)
and **37/37 GPU checks** (165.61 s). Inspector-free Release passes
**540/540 headless checks** (6.69 s) and **37/37 GPU checks**
(105.4 s). Each totals **577/577**, including 113 private-data
checks (83 headless). Targeted Clang ASan/UBSan/leak checks pass **48/48**
(117.24 s). Both GPU matrices contain no Vulkan validation
error/VUID diagnostic.

Private runs produce **27 fragments and 80 observed impacts per SubwayA cycle**,
with complete expiry and IDs never reused across three cycles. GPU readback covers
1,510 synthetic half-alpha pixels and 6,830 pixels from the actual 12-triangle metal
model. These are bounded test observations, not claims of retail parity.

Commands: `cmake --build build/debug -j 2`, `cmake --build build/release -j 2`,
then `ctest --test-dir build/<configuration> -L headless -j 4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use the existing Clang `build/sanitize` configuration and eight affected test targets,
with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` and
CTest regex `^(explosion_fragments|explosion_rings|explosion_lights|explosion_models|explosion_sprites|sceneexplosion|sprite_view|objects)\.`.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
20 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve. Evidence
retains source hashes, the committed project snapshot and patch.

Synthetic checks exercise every launch mode, RNG ordering, reflection/lift/damping,
strict rest and fade boundaries, terminal motion, cadence, RAYHIT objects, pending
removal, missing collision and resources, capacity failure, detached ownership and
non-reused IDs. The existing model regression retains all nine explosion sphere
assertions and additionally accounts for fragment draws. Private checks activate
all nine SubwayA emitters through three load/player/unload cycles, inspect actual
metal resources, observe contacts and wait for complete expiry.

GPU readback verifies original fade after a real synthetic floor contact/rest and
separately draws the external metal model/skin. Application smoke requires a checked
fragment draw while the inspector is hidden and validates state again after reload.
Captures stay in ignored build folders; commercial resources are not copied into Git.

This is Linux GCC/Clang and Vulkan evidence. Other platforms/backends and comparison
with the retail executable remain unverified. Full composite FX, safe world transfer,
Gideon and other required services remain open; M3's playable slice is not claimed.
