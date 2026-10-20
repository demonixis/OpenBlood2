# M2 — Original explosion sprites and smoke

Original CClientExplosionSFX emitters now create visible fire/smoke sprites using
selected original CExplosionFX recipes and CExplosionSpriteFX lifecycle code.
The existing SDL_GPU sprite pass renders their real SPR/DTX frames, growth and
opacity. The Scene explosions inspector exposes active effects, prepared resources,
expiry, failures and the missing pieces of each composite.

See [contracts](../../contracts/explosion_sprite_contracts.md),
[source selections](../sources/m2_explosion_sprite_sources.json) and
[execution evidence](evidence/m2-explosion-sprites.json).

## Behavior and limits

The source scale waveforms, fade modes, normal offsets and strict end duration are
preserved. Client sprites own their state independently of the server emitter;
removing that emitter leaves fire/smoke running, while world unload clears them.
The original null-damager rule still delivers no explosion damage. Detached sound
continues unchanged. Session time drives effects outside the UI; presentation does
not advance clocks. Per-instance opacity multiplies actual texture alpha, with
existing world/model occlusion and no sprite depth writes.

Only the high-detail sprite branches for types 0, 1, 2, 3, 6 and 8 are connected.
They do not supply explosion models, particle rings/flames, fragments/trails or
lights. Other types and random alignment fail explicitly. Generic brush/weapon
FX callers and quality selection remain unavailable. An event ID is a retained
report, not a full-composite success. This is a source-backed harness comparison,
not a retail executable or complete visual-parity comparison.

All nine SubwayA emitters use type 3 (grenade) and create two sprites each, and all 30 combined explosion
and existing DetailSprite draws prepare successfully in the private test. Three
load/player/unload cycles check non-reused identities and cleared ownership.
These client visuals do not alter source-world counts: **203 created / four
transferred sounds / 372 editor-only / one unsupported Gideon / zero failures**.
Safe automatic NEW_LEVEL and required remaining FX/services keep **M2 incomplete**.

## Verification

Debug passes **518/518 headless checks** (64.12s)
and **31/31 GPU checks** (172.61s). Inspector-free Release passes
**518/518 headless checks** (11.03s) and **31/31 GPU checks**
(39.86s). Each configuration totals **549/549**, including
106 private-data checks (79 headless). Targeted Clang ASan/UBSan/leak validation
passes **72/72**: 65 sprite/effect/resource/ownership checks and seven shared-alignment
camera checks. Existing world/model, player, collision and audio checks stay enabled.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
25 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve. The source
ledger records exact reference hashes, and execution evidence retains the committed
project snapshot, patch and focused test outputs.

Initial development checks caught an incomplete signature edit, a fixture property
type mismatch and wrong expected values for the wrapped SPR clock / fade-3 endpoint.
Those were corrected against the existing clock and original formulas before final
validation; no original acceptance criterion or assertion was weakened.

Synthetic checks cover the other selected recipes; private visual validation here
is limited to the authored type-3 branch. The focused behavioral checks exercise
all scale/fade branches, creation/terminal updates, source cadence, normal alignment, partial resource failures, unknown types,
capacity, repeated triggers, independent lifetime, read-only rendering and unload.
GPU readback tests add per-instance opacity to existing texel alpha/depth/occlusion
coverage. The application additionally requires two evolving sprite submissions
with hidden UI and after reload. The full original emitter/audio tests remain enabled.

## Reproduction

From Blood2 (the existing local cache supplies the external content root):

```sh
cmake -P cmake/GenerateWorldShaders.cmake
cmake --build build/debug -j4
ctest --test-dir build/debug -L headless --output-on-failure -j4
ctest --test-dir build/debug -L gpu --output-on-failure
cmake --build build/release -j2
ctest --test-dir build/release -L headless --output-on-failure -j2
ctest --test-dir build/release -L gpu --output-on-failure
cmake --build build/sanitize -j2 --target explosion_sprite_tests scene_explosion_tests sprite_view_tests sprite_tests texture_tests world_view_tests model_view_tests object_contract_tests attachment_tests camera_tests light_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(explosion_sprites|sceneexplosion|sprite_view|sprites|texture|world_view|model_view|objects|attachments|lights)\.' --output-on-failure -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^cameras\.' --output-on-failure -j1
```

Open the Scene explosions tab and trigger an emitter to inspect its client sprites.
Folding/hiding the inspector leaves simulation and presentation running. The GPU
smoke selects an original emitter itself; these test commands do not alter assets.

[Inspector capture](evidence/m2-explosion-sprites-inspector.png) shows the real
active effect counters and source emitter dispositions; scroll for per-effect
resource/scale/alpha details and retained partial-composite reports.

[Inspector-free capture](evidence/m2-explosion-sprites-world.png) shows the unchanged
intro camera and a partial fire/smoke sprite at the right edge. Train-glass material
and broader lighting limitations are unchanged.
