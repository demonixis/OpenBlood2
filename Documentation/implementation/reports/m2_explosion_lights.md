# M2 — Original explosion point lights

Original CExplosionLightFX initialization, creation and update now evaluate the
flash and residual point lights emitted by the supported CExplosionFX recipes.
Scene explosions exposes their position, radius, RGB, delay and lifetime. Models,
sprites and sound can fail independently without canceling the light state.

See [contracts](../../contracts/explosion_light_contracts.md),
[source ledger](../sources/m2_explosion_light_sources.json) and
[execution evidence](evidence/m2-explosion-lights.json).

The selected types are small/medium/large/grenade, napalm fireball and Tesla
alternate. The source's second “scorch” light is a shrinking 25-second light, not
a decal: its duplicated Radius1 assignment leaves Radius2 at the preceding zero.
Delayed lights preserve the original restart-at-current-time rule and return
before interpolation. Uninitialized false change flags are explicitly repaired.

All nine SubwayA emitters create two lights. After .11 seconds, each flash has
radius 228 / RGB (1,.544,.088), while each residual has radius 99.56 / RGB
(.20132,.20132,.20132). Their source objects have already been removed. Three
world/player cycles check flash expiry, residual survival and complete unload.
Synthetic cases cover other recipes and delay, including late updates.

These are evaluated client point-light records, **not surface illumination**.
GPU tests exercise the existing application, hidden UI and reload; they do not
prove rendered light contribution. No new lighting shader, shadow/fog renderer,
scorch decal or successful complete-composite flag is introduced. World-source
counts stay 203 created / four transferred sounds / 372 editor-only / one
unsupported Gideon / zero failures. Remaining composite services and safe automatic
NEW_LEVEL still keep **M2 incomplete**.

## Verification

Debug passes **528/528 headless checks** (72.3 s)
and **33/33 GPU checks** (157.68 s). Inspector-free Release passes
**528/528 headless checks** (10.68 s) and **33/33 GPU checks**
(105.02 s). Each totals **561/561**, including 109 private-data
checks (81 headless). Targeted Clang ASan/UBSan/leak checks pass **42/42**
(84.79 s). Both GPU matrices contain no Vulkan validation
error/VUID diagnostic. There is no new light-pixel test because illumination is
explicitly unavailable; hidden/reload smoke checks evaluated light state.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
16 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve. Execution
evidence retains source hashes, the committed project snapshot and patch.

## Reproduction

From Blood2, with the configured external content root:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug -L headless --output-on-failure -j4
ctest --test-dir build/debug -L gpu --output-on-failure
cmake --build build/release -j2
ctest --test-dir build/release -L headless --output-on-failure -j2
ctest --test-dir build/release -L gpu --output-on-failure
cmake --build build/sanitize -j2 --target explosion_light_tests explosion_model_tests explosion_sprite_tests scene_explosion_tests light_tests object_contract_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(explosion_lights|explosion_models|explosion_sprites|sceneexplosion|lights|objects)\.' --output-on-failure -j1
```

Trigger a source in Scene explosions to inspect its independent light, model and
sprite deliveries. Fold/hide the inspector without pausing the session.

[Inspector capture](evidence/m2-explosion-lights-inspector.png) shows the two active
lights and the explicit illumination limitation. Per-light values and retained
deliveries are further down the scrollable panel.
