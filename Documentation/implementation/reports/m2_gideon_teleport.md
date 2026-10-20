# M2: original Gideon fade and teleport states

Gideon's original fade, Teleport and Special1 now execute through the state
dispatcher. Model objects have checked RGB/alpha services, and SDL_GPU renders the
actual disappearance/reappearance with shared depth. The existing player engine
snapshot preserves these new values during supported restoration.

This is a dependency for the boss lifecycle, **not a production Gideon encounter**.
The tests register a diagnostic actor with an explicitly initialized inventory and
table. Normal SubwayA remains 203 created / four transferred sounds / 372 editor-only /
one unsupported Gideon / zero creation failures. Boss initialization, health and
damage callbacks, attack/dodge sequencing, required FX and safe NEW_LEVEL remain
open. The observable M2 acceptance gate is unchanged and incomplete.

See [contracts](../../contracts/gideon_teleport_contracts.md),
[source selections](../sources/m2_gideon_teleport_sources.json) and
[retained evidence](evidence/m2-gideon-teleport.json).

Project commit: `8eedff348da3f3b2b490e276c56eb84f27a61cc4`.

## Behavior and engine boundary

The first fade call clears SOLID and enters command 101; the following 16 calls
subtract 1/16 alpha, independent of time, without changing animation or RGB.
Teleport uses cached movement position and an unnormalized X/Z direction times
1500. Special1 uses current target position, Y=.2 and distance 400–600. Both restore
SOLID before the ray and move through the existing collision solver. Hit offsets
are 100/300; the miss branch's unused offset is deliberately not applied. The
inspector distinguishes the requested point from the position actually achieved.

Original teleport audio requests a real attached WAV without retaining its handle.
Playback starts/errors are inspected separately from the helper's normally zero
return. Missing audio still advances the original command. A missing Special1
target changes to Teleport and starts the fade in the same call. Required query,
movement, cache, target and color failures remain explicit. Contact removal or
reentrant teleport cannot finish a stale command.

Live model RGB/alpha is validated and stored per instance. Original RGB defaults
to black and brightens shading; it is not a multiplicative tint. RGB stays visible
as lighting metadata while additive model illumination remains unavailable. The renderer keeps
opaque objects in the existing depth-writing pass, then blends translucent models
with the existing alpha shader and no depth writes. Fully faded models contribute
no pixels; hidden and missing instances remain inspectable. Sorting is limited to
object origins within the model pass, separately from transparent world faces and
other FX. This does not claim retail lighting/transparency or collision parity.

## Verification

Debug and inspector-free Release each pass **683/683 checks**: 626 headless and
57 serial GPU, including 140 private-data cases (99 headless). Debug takes
92.46s headless / 180.83s GPU;
Release takes 12.13s / 116.6s.
Clang ASan/UBSan with leak detection passes **71/71** in 117.07s.
The final logs contain no sanitizer report or GPU validation error/VUID.

From Blood2: `cmake --build build/debug -j4` and `cmake --build build/release -j4`,
then `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer targets are gideon_combat_tests, gideon_teleport_tests, player_restore_tests,
ai_runtime_tests, ai_fire_tests, ai_model_key_tests, object_contract_tests and
world_tests, built with `-j2`. Sanitizer CTest uses `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1` and
`^(gideon_combat|gideon_teleport|player_restore|ai_runtime|ai_fire|ai_keys|objects)\.`
plus `^world.destination_(trials|lifetime|trial_private)$`.

All 20 changed/new C++ files pass clang-format 23.1.3. All 2700 local Markdown
links resolve; all 10923 inventoried reference hashes and six reference repositories
remain unchanged. The private GPU capture was visually inspected.

The new behavior checks cover exact fade call counts, unchanged animation/RGB,
completion overflow, cached versus current ray origins, source miss endpoints,
both hit offsets, nonsolid ray targets, floor-clipped movement, no-target state
fallthrough, unavailable collision, reentrant contacts, pending removal, model
color validation and hidden presentation. The player roundtrip adds color/alpha
preservation and rejects malformed color snapshots.

Three private cycles load the actual external Gideon ABC/DTX and teleport WAV,
run fade/move/reappearance and unload completely. GPU readback covers both a
synthetic triangle and the external model: repeated rendering does not advance
clocks, half alpha changes pixels, zero alpha gives an empty frame, restoring alpha
reproduces the original frame, and removal clears presentation. The synthetic
single-triangle blend also checks exact linear half-opacity within rounding.
Commercial resources and screenshots remain external/ignored.

An initial private assertion incorrectly expected a retained sound ID. Inspection
confirmed that original PlaySound without GETHANDLE returns zero after successful
playback. The production inspection and regression now check actual starts,
failures and the decoded auto-removing mixer voice; the sound ownership contract
was preserved. Source review also corrected an initial multiplicative-RGB assumption:
the SDK specifies black-default additive model lighting, now retained as explicit
metadata until that shading service exists.

Next: integrate original Gideon attack/dodge sequencing and then initialization,
boss-health/damage dispatch and its shared table with destination-isolation checks.
Only after required services really pass can the normal session gate advance.
