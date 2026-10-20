# M2: Original destructible models in CityHub

Project commit: `87b6c35b8af8c2322091615644401c3d3e88afab`.

The original CDestructableModel class now connects authored ABC/skin resources,
damage aggregates, gravity/pushable bodies, damage/destroy model replacement,
alpha commands, cascade delays and deferred destruction. It uses the existing
small engine and original shared damage/debris code. No new library or mutable
game global is added. The source client effect only sets physics dimensions;
direct integration uses the same checked body and exposes successful assignments.

[Contracts](../../contracts/destructable_model_contracts.md),
[source ledger](../sources/m2_destructable_model_sources.json) and
[retained evidence](evidence/m2-destructable-models.json) distinguish original
rules, replacement physics and remaining limitations. Objects inspects health,
body/dimensions, priority/friction, velocity, fade, replacement paths, cascade,
ground result, loop state and failures, including hidden/inactive instances.

CityHub now has **413 created / 541 editor-only / 37 unsupported / 35 failed**
source entries. Structural preparation has 417 schema-checked / 37 unknown /
31 invalid entries. The new class creates all 122 authored instances: 51 pushable,
98 with gravity, 7450 model triangles and zero ABC/skin failures. Two unload/reload
cycles verify stale identities and destructible-list cleanup. Eighteen ground rays
return no hit/start-inside and preserve the authored position; after a short physics
step, 47 bodies have recorded a collision block, including ordinary floor contacts.
These counters do not certify placement parity or a completed CityHub playthrough.

The real ceilingfan2.abc contains one unnamed animation with 19 keys/duration599.
The reader now accepts that name while retaining all structural/key/pose checks;
a synthetic unnamed fixture still rejects zero keys. No guessed clip is supplied.

Debug and inspector-free Release each pass **718/718 checks**: 659 headless and
59 serial GPU, including 148 private-data cases (105 headless). Debug takes
112.93s headless / 194.53s GPU;
Release takes 8.76s / 118.25s.
Clang ASan/UBSan with leak detection passes **96/96** selected checks in
74.83s. Final build logs contain no compiler warnings/errors;
final runtime logs contain no GPU validation error/VUID or sanitizer diagnostic.

New behavioral cases exercise automatic/authored dimensions, rotation and scale,
flags/priorities, failed initialization/replacement, original damage/destruction
and repeated surviving destruction, cascade delay, independent spawned pickup,
alpha parsing/fade, gravity, atomic pushes/riders, AI ray filters, real loop lifetime,
unnamed ABC clips and private world unload/reload. Existing preparation/rehearsal
checks retain all source-session/global/list preservation assertions. The old AI
filter absence assertion now expects registration; dedicated behavior tests prove
the new model-only exclusion and non-destructible obstruction.

GPU pixels verify real garbage-can, barrel and ceiling-fan models/skins with
submission omission/restoration and two reloads in both configurations. Diagnostic
cameras isolate these assets; captures were reviewed and remain ignored. The
inspector data path is compiled in Debug; no manual whole-level gameplay or retail
comparison is claimed.

Commands from Blood2: `cmake --build build/debug -j4` and
`cmake --build build/release -j4`, followed by
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer targets: `destructable_model_tests object_contract_tests services_tests
world_tests model_view_tests pushable_tests ai_fire_tests ai_sight_tests attachment_tests`.
Sanitizer CTest: `-j2 -R '^(objects|services|world|model_view|pushables|destructable_models|ai_fire|ai_sight|attachments)\.'`,
with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

All changed C++ files pass clang-format; local Markdown links resolve. All 10923
reference-file hashes and six reference repositories remain unchanged. Verification
is limited to this Linux machine and Vulkan.

The original mass/friction and push priorities use the existing bounded box solver.
Its collision/depenetration limits remain visible. Original sliding sound rules
are connected to real attached WAV loops, but ordinary contacts carry no force:
only explicitly labelled nonzero-force probes establish the required standing flag.
No successful production sliding-force implementation is claimed. Gib requests
remain diagnostic records; lighting/shadows, Chrome, partial destroy resource pairs
and model save/restore remain unavailable. A bounded parser fixes the source's
multi-command pointer loop. Checked deferred lifetime emits debris before removal.

Safe automatic NEW_LEVEL, remaining destination classes/services and the actual
M2 acceptance criteria remain open. Neither increased class coverage nor isolated
rendering tests certify a playable CityHub or retail physics/rendering parity.
