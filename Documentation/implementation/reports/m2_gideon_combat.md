# M2: original Gideon combat decisions and model-key actions

Gideon's original combat methods now select states/weapons, grant shield armor,
fire from his own model nodes and publish his powerup effect. They reuse the
existing original weapons and typed services. The base model-key dispatcher now
reaches these virtual overrides. A separate fix removes an out-of-bounds access
in NPC weapon initialization: newly supported weapon IDs exceeded an older
player-sized duplicate array.

This increment supplies selected boss methods; **it does not register a production
Gideon**. Tests explicitly create a diagnostic actor and initialize its inventory
and animation table. Normal SubwayA accounting stays 203 created / 4 transferred
sounds / 372 editor-only / 1 unsupported Gideon / 0 creation failures. Original
boss initialization, health-message/damage dispatch, attack/dodge sequencing,
fade/teleport, required FX and safe NEW_LEVEL remain open. M2 stays incomplete.

See [contracts](../../contracts/gideon_combat_contracts.md),
[source ledger](../sources/m2_gideon_combat_sources.json) and
[retained evidence](evidence/m2-gideon-combat.json).

Project commit: `f11f13657e6a932394b4f71f4a530e2d340ac80e`.

## Preserved behavior and inspection

- Decisions retain strict sight/health/threat/shield thresholds and the original
  `lastPanic+15 >= now` comparison.
- Shield selection adds 100 armor before starting the rifle animation. The weapon
  itself only emits its visual/audio effect. A missing required animation fails
  explicitly while retaining preceding selection/armor effects.
- Far attacks choose Energy Blast with a 70% draw and the MAGIC animation column;
  other attacks choose Wind with a 50% draw; remaining outcomes choose Naga. There
  is no timed firing in Gideon's override. Completion retains the original ignored
  request for an absent Soul Hook, with the actual failed result inspectable.
- Fire uses the selected node (initially rr_gun) or current body position plus the
  movement aggregate's cached forward. Generic AI clearance is bypassed, while
  actual weapon collision/damage/lifetime checks remain active.
- Extra keys preserve the case-sensitive powerup prefix, node-name assignment and
  immediate firing. Fire-key values do not change the selected node. Branch
  priority, no-target handling and deferred callback removal remain observable.
- The Skeleton inspector displays these decisions, armor grants/timestamps,
  selected node, shot result, powerup history/errors and completion selection.
  It identifies diagnostic boss methods separately from production AI_Update.

The NPC array correction stores up to five requested weapon IDs by value. A real
Cultist initialization regression uses 41/27/38/42 plus a duplicate 41, verifies
four correctly ordered weapons and repeats removal three times. Authored defaults
and other classes' aiming rules are unchanged.

## Verification

Debug and inspector-free Release each pass **673/673 checks**: 618 headless and
55 serial GPU, including 138 private-data cases (98 headless). Debug takes
89.27s headless / 181.39s GPU;
Release takes 13.77s / 119.92s.
Clang ASan/UBSan with leak detection passes **59/59** in 76.01s,
covering Gideon methods, original AI keys/fire/runtime, object lifetime and
destination rehearsal/lifetime including private callbacks. The final retained
logs contain no sanitizer report or GPU validation error/VUID.

Commands from Blood2: `cmake --build build/debug -j4`,
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Clang `build/sanitize` builds gideon_combat_tests, ai_runtime_tests, ai_fire_tests,
ai_model_key_tests, object_contract_tests and world_tests with `-j2`.
Sanitizer CTest selects `^(gideon_combat|ai_runtime|ai_fire|ai_keys|objects)\.`
plus `^world.destination_(trials|lifetime|trial_private)$`, with `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

After a brace-only aggregate-initialization warning cleanup in the test fixture,
the eight Gideon checks also pass again in all three configurations.

All 11 changed/new C++ files pass clang-format 23.1.3. All 2678 local Markdown
links resolve; all 10923 inventoried reference hashes and six reference repositories
remain unchanged. The local private GPU capture was visually inspected.

The private action test loads external gideon.abc/gideon.dtx and its original
animation table. The 3333 ms shield clip contains play_sound:build,
extra_key:powerup and fire_key:head; its actual callbacks produce one shot and
100 armor, with no extra timed shot. The test also drives the original magic clip,
checks its three shots (fire_key values rr_gun/ll_gun/rr_gun are ignored by
the original dispatcher, so all three keep the selected rr_gun), and exercises all four weapons from the actual
Gideon node. Three creation/unload cycles verify object/effect/message cleanup.
These are diagnostic original-method runs, not an authored boss fight.

GPU shield/powerup readback now starts through Gideon's actual Fire and extra-key
caller, retaining the prior real texture/alpha/particle assertions, repeated
read-only rendering and owner-removal checks. Commercial resources and captures
remain external/ignored. Existing session, destination isolation, AI keys/fire,
object lifetime and GPU checks guard the shared dispatch changes.

A sanitizer caught an error in the new lifetime test: it inspected its diagnostic
actor after advance_models had already flushed the deferred removal batch. The
fixture now records final counts during destruction and verifies a missing handle,
one destruction and no second marker. No production removal timing was changed.

Other platforms, XR, retail RNG/physics/rendering parity and a human playthrough
are not validated by these Linux checks. Required unsupported services keep the
unchanged milestone gate open.
