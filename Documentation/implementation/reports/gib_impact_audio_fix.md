# Repeated gib impact audio correction — 2026-10-18

Implementation commit in the independent `Blood2/` repository: `53a5967`.

Shooting an NPC into flesh fragments could generate continuous overlapping impact
sounds and blood splats. `FleshGibs::advance` passed the next **position** to
`CastWorldRay`, whose second argument is a **direction** and whose ray extends to
the collision domain boundary. A falling fragment could consequently hit a distant
ceiling every frame. Each false contact started a new one-shot sound; the sounds
were not accidentally configured as loops. The blood-spurt emitter in
`CParticleExplosionFX::Update` had the same query error, producing premature
impacts before stopping emission.

The selected original [BounceMovingObject](../../../../sources/blood2/ClientShellDLL/client_physics.cpp)
sets `ClientIntersectQuery.m_From` to the current position and `m_To` to the next
position. Both adapted callers now use `IntersectWorldSegment(from, to, hit)`.
It validates finite endpoints and queries only the primary-world collision index,
retaining the existing exclusion of ordinary objects and moving world models.
The typed service is registered in the checked service catalog. Direction-based
`CastWorldRay` remains available for original `DetailSprite` placement.

Gravity, reflection damping, floor bounce counts, source random draws, impact
audio/splat recipes and one-shot ownership are unchanged. No renderer, shader,
dependency, commercial resource or reference source was modified. This is a
bounded bug correction; M2 remains closed with reservation M2-GPU-001 and M3 has
not been accepted. See the maintained [death-effect](../../contracts/death_effect_contracts.md)
and [blood-spurt](../../contracts/blood_spurt_contracts.md) contracts.

## Reproduction and regression coverage

The new synthetic room contains a floor, ceiling and wall. A fragment begins
100 units below the ceiling and moves downward. Over twelve .01-second frames:

| Observation | Before correction | After correction |
| --- | ---: | ---: |
| False impacts | 11 | 0 |
| Simultaneously retained one-shot voices | 9 | 0 |
| Blood splats | 11 | 0 |

Before switching the two callers, the query-boundary test passed and all three
effect tests failed. The [original failure log](evidence/gib-impact-audio-before.txt)
is retained. All four corrected regression modes pass: finite endpoint and
primary-world-only queries; distant surfaces versus real wall/floor hits;
the repeated-ceiling reproducer; and blood-spurt impact/voice completion.
Translated positive/negative room coordinates prevent accidental dependence on
the world origin. Resting fragments produce no later sounds or splats, while real
impacts still start the expected nonlooping, automatically removed voices.

The private-data death test uses original Cultist, gib models and impact WAVs,
removes the dead NPC and checks all fragments resting plus an empty voice table,
then advances another two seconds and requires unchanged impact counts and silence.
Three unload/reload cycles each produce seven fragments and eight real impacts.

The first strengthened private test failed because its old eight-second window
and 4000-unit diagnostic floor did not accommodate every original launch: one
fragment was still at Y782.541, moving downward at 1912.07 units/second and near
the floor edge. The fixture now spans 20000 units in each horizontal direction
and advances sixteen seconds before checking rest. No production velocity, rest
rule or lifetime was altered to satisfy this assertion.

## Validation scope

Debug and inspector-free Release application builds completed. The targeted test
set contains 67 tests, including 15 using external retail data. Final outcomes:

| Configuration | Outcome | Logs |
| --- | --- | --- |
| Debug | 66/67 initially; remaining device test passed on corrected invocation | [Initial](evidence/gib-impact-audio-debug-tests.txt), [device recheck](evidence/gib-impact-audio-debug-device-recheck.txt) |
| Release | 67/67 passed | [Run](evidence/gib-impact-audio-release-tests.txt), [ceiling result](evidence/gib-impact-audio-after.txt) |
| Clang ASan/UBSan, leak detection enabled | 64/67 initially; all eight tests in the affected groups passed after recheck | [Initial](evidence/gib-impact-audio-sanitize-tests.txt), [recheck](evidence/gib-impact-audio-sanitize-recheck.txt) |

The initial Debug and sanitizer device checks failed because the invocation set
`SDL_AUDIODRIVER=dummy`; this overrides the test's own `SDL_SetHint` call, which
then returns false. Removing that environment override passed both checks without
changing the test or mixer. The sanitizer run also retained the short-floor gib
assertion failure described above and a ten-second `objects.message_churn`
timeout while builds/tests ran concurrently. After the builds finished, an
unchanged serial message-churn recheck passed in 5.61 seconds under the original
ten-second deadline. No timeout or sanitizer suppression was added; all failed
attempts remain in the linked logs.

Commands run from `Blood2/` (the sanitizer build uses the existing Clang preset
directory, with `BLOOD2_SANITIZERS=ON`):

```sh
test_targets=(flesh_gib_collision_tests death_effect_tests blood_spurt_tests sprite_tests object_contract_tests service_coverage_tests cinematic_tests corpse_tests explosion_fragment_tests player_death_tests)
cmake --build build/debug --target blood2_app "${test_targets[@]}" --parallel 4
cmake --build build/release --target blood2_app "${test_targets[@]}" --parallel 4
cmake --build build/sanitize --target "${test_targets[@]}" --parallel 4
pattern='^(flesh_gib_collision|death_effects|blood_spurts|sprites|objects|coverage|cinematics|corpses|explosion_fragments|player_death)\.'
ctest --test-dir build/debug -R "$pattern" --output-on-failure -j 4
ctest --test-dir build/release -R "$pattern" --output-on-failure -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R "$pattern" --output-on-failure -j 4
ctest --test-dir build/debug -R '^cinematics\.device$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize \
  -R '^(death_effects\.|cinematics\.device$|objects\.message_churn$)' --output-on-failure -j 1
git diff --check
```

The first Debug/sanitizer commands used the extra audio environment override
documented above; the displayed commands omit it for reproducibility. The updated
private death test was rebuilt before its sanitizer recheck. Documentation links
were checked locally.

Checks run on Linux, with synthetic fixtures and the configured external retail
installation. They exercise simulation, decoded audio and voice completion, not
an interactive listening comparison against the original executable. No GPU test
or platform qualification is claimed by this fix.
