# M2 — Original explosion models

The original CExplosionModelFX Init/CreateObject/Update methods now drive real
ABC/DTX models alongside the existing explosion sprites. Scale, rotation, opacity
and lifetime remain visible in the Scene explosions inspector. The SDL_GPU pass
uses actual model triangles and skin alpha with existing world/model depth.

See [contracts](../../contracts/explosion_model_contracts.md),
[source ledger](../sources/m2_explosion_model_sources.json) and
[execution evidence](evidence/m2-explosion-models.json).

## Scope

The high-detail small/medium/large/grenade and Tesla-alternate model branches are
connected. Napalm fireball explicitly has no model component. All source scale
waves and fades, random orientation modes and cached-axis rotations are covered.
Per-update rotations deliberately do not multiply by elapsed time. Portable RNG
keeps original draw order/range formulas without perturbing server gameplay RNG;
retail random sequences are not claimed.

Models outlive their emitters, expire independently of smoke and clear at unload.
Missing models/skins fail visibly while sibling sprite/audio requests continue.
Frame snapshots do not advance clocks, and hidden/no-UI operation follows the same
session path. Opaque characters and view weapons keep their existing rendering.

All nine SubwayA emitters create the original **168-triangle exp_sphere.abc** and
Explosion_1.dtx, with alpha **.585 after .11 seconds** and expiry at **.5 seconds**.
Three private world/player cycles test resource preparation, source-independent
lifetime and cleanup. Other selected recipes use synthetic fixtures. The isolated
GPU capture exercises the private sphere at .25 seconds (alpha .375).

Rings, particle flames, fragments/trails, lights, additional types/callers and
quality settings remain unavailable. Model and sprite alpha passes are separately
ordered; cross-type and triangle transparency parity is not claimed. Global light
scale is absent. Complete-composite availability therefore remains false. These
client visuals do not alter source-world counts: **203 created / four transferred
sounds / 372 editor-only / one unsupported Gideon / zero failures**. Safe automatic
NEW_LEVEL and other required services keep **M2 incomplete**.

## Verification

Debug passes **523/523 headless checks** (66.02s)
and **33/33 GPU checks** (167.61s). Inspector-free Release passes
**523/523 headless checks** (17.5s) and **33/33 GPU checks**
(108.08s). Each totals **556/556**, including 108 private-data
checks (80 headless). Targeted Clang ASan/UBSan/leak checks pass **61/61**
(103.44s). Both final GPU matrices contain no Vulkan validation
error/VUID diagnostic. The full existing opaque model/world, movement and audio
checks remain enabled.

GPU pixel readback verifies half-opacity on 1,510 covered synthetic model pixels.
The private sphere capture uses 168 triangles and alpha .375; its pixel count is
retained in the focused GPU log. Hidden/reloaded application smoke requires an
actual evolving model submission in addition to the existing two sprite draws.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
24 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged, and local Markdown links resolve. The
execution record includes source hashes, the committed project snapshot and patch.

Initial GPU readback exposed a leak in the new test harness: it initialized the
same Capture repeatedly. Reusing its target fixed the leak. The new GPU tests now
fail explicitly on Vulkan validation errors; final logs are checked for diagnostics.
The final RNG helper also preserves original one-draw modulo/range formulas instead
of using standard-library distributions. No acceptance criterion was weakened.

## Reproduction

From Blood2, using the configured external content root:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug -L headless --output-on-failure -j4
ctest --test-dir build/debug -L gpu --output-on-failure
cmake --build build/release -j2
ctest --test-dir build/release -L headless --output-on-failure -j2
ctest --test-dir build/release -L gpu --output-on-failure
cmake --build build/sanitize -j2 --target explosion_model_tests explosion_sprite_tests scene_explosion_tests sprite_view_tests sprite_tests texture_tests world_view_tests model_view_tests object_contract_tests attachment_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(explosion_models|explosion_sprites|sceneexplosion|sprite_view|sprites|texture|world_view|model_view|objects|attachments)\.' --output-on-failure -j1
```

Trigger a source in Scene explosions, then inspect model and sprite entries and
their separate delivery reports. The inspector can be folded or hidden throughout.

[Private original model capture](evidence/m2-explosion-models-sphere.png),
[synthetic half-opacity readback](evidence/m2-explosion-models-alpha.png) and
[inspector capture](evidence/m2-explosion-models-inspector.png) are generated by the
GPU checks. The isolated sphere is a diagnostic camera view, not an authored
cinematic screenshot or a retail visual comparison.
