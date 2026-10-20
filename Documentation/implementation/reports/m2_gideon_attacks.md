# M2: original Gideon attack and dodge sequences

Gideon's original close/far attack phases now connect his existing weapon and
teleport methods with timed lateral dodges and the original Bold taunt. His Recoil
state dispatches directly to Special1. The in-game Skeleton inspector exposes the
last attack distance, dodge draw, elapsed time, calls and completions.

See [contracts](../../contracts/gideon_attack_contracts.md),
[source selections](../sources/m2_gideon_attack_sources.json) and
[retained evidence](evidence/m2-gideon-attack.json).

Project commit: `e34525eeaf6bb7d55dd59ce310e15e6ef3c6de3b`.

The one-second dodge timer ignores animation completion and retains lateral
velocity after finishing. Bold taunt writes lateral velocity only on entry and
also leaves it intact. Close/Far retain different distance thresholds and face
increment cancellation. Already-started firing continues outside the initial
range gate. Distances/bases use the original movement cache; rendering does not
advance actions, physics or animation. Failures propagate from the typed services.

These are selected original boss methods exercised by an explicitly initialized
diagnostic actor. Production Gideon construction, shared table, boss-health and
damage-message dispatch remain pending. Normal SubwayA still reports 203 created,
four transferred sounds, 372 editor-only, one unsupported Gideon and no creation
failures. Required FX and safe NEW_LEVEL also remain open. M2 is incomplete.

## Verification

Debug and inspector-free Release each pass **691/691 checks**: 634 headless and
57 serial GPU, including 141 private-data cases (100 headless). Debug takes
92.81s headless / 180.9s GPU;
Release takes 13.01s / 117.03s.
Clang ASan/UBSan with leak detection passes **85/85** in 61.36s.
The final build logs have no compiler warning/error; runtime logs contain no
sanitizer report or GPU validation error/VUID.

From Blood2: `cmake --build build/debug -j4` and `cmake --build build/release -j4`,
then `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer targets: gideon_attack_tests, gideon_combat_tests, gideon_teleport_tests,
ai_escape_tests, ai_locomotion_tests, ai_hit_tests, ai_runtime_tests,
object_contract_tests and world_tests, built with `-j2`. Sanitizer CTest uses `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1` and
`^(gideon_attack|gideon_combat|gideon_teleport|ai_escape|ai_locomotion|ai_hit|ai_runtime|objects)\.`
plus `^world.destination_(trials|lifetime|trial_private)$`.

All seven changed/new C++ files pass clang-format 23.1.3; all 2725 local Markdown
links resolve. All 10923 inventoried reference hashes and six reference repositories
remain unchanged. The private GPU capture was visually checked. These are Linux
checks, not a retail playthrough or cross-platform/XR verification.

Eight new headless cases cover timer equality, early clip completion, command
changes, cached transforms, trapped movement, single integration, source range
boundaries, fire continuation, both dodge choices, taunt/Beg separation, Recoil
virtual dispatch, missing resources/services and deferred target/owner removal.
Three private cycles load the actual external Gideon model/skin/table and run the
shield animation's powerup/fire keys into dodge, then the original taunt phase.
Repeated model preparation does not advance commands or animation clocks.

The existing synthetic/private GPU fade scenarios now also run dodge motion with
collision configured. They check displacement once per tick, changed pixels,
repeatable reads, one-second completion and retained velocity before removal.
The final private dodge capture contains 287 non-background pixels at x187–211 /
y156–188 in its 400x300 frame, identical in Debug/Release. The actual model capture
stays ignored, alongside external commercial assets.
No production factory or asset copy is introduced.

Initial fixture failures exposed the existing 0.5-second physics catch-up bound,
a missing collision index in the formerly render-only GPU scenario, and the real
ComputeStimuli clearing a lost target. The fixtures now supply valid ticks/index
and explicitly reacquire their target for the next independent scenario. Production
rules, the catch-up limit and acceptance assertions were preserved.

Next: original boss initialization, health/damage dispatch and shared-table ownership,
with destination rehearsal isolation checked before production registration.

Before that registration, audit the original damage call chain: Gideon's override
calls CBaseCharacter, then AI_Mgr's damage branch calls CBaseCharacter again. The
message-reader/aggregate semantics must be established before deciding how to
adapt that chain; do not assume two health deductions. Original client BOSSHEALTH
handling also hides the power bar at ratios <=0 or >=1. Gideon's nonzero-full-health
update branch keeps its create-health flag set. These source quirks require explicit
contracts and tests alongside the fifth class table's world/destination ownership.
