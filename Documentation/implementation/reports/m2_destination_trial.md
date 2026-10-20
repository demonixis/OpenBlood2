# M2 continuation: original destination initialization with source-session retention

The engine can now initialize the destination's supported original objects and
restore a copy of the current player at its authored arrival point, then destroy
that temporary world and resume the original source session. The Session inspector
exposes this explicit rehearsal and its missing/failed entries. **Automatic level
transfer and M2 remain incomplete.**

Unlike the preceding structural scan, this executes original precreate/initial
callbacks, resource binding, start triggers and named KeepAlive player entry.
It preserves the source's objects, client, intrusive lists, active camera and AI
animation/voice owners by temporarily exchanging the currently ported game globals.
All temporary destruction occurs under the temporary adapter before source globals
are restored. No destination tick, `RunWorld`, GPU scene or audible output is started.
The implementation is a bounded new engine adaptation around existing original
callbacks, not a replacement gameplay implementation or a successful transfer stub.

See [contracts](../../contracts/destination_trial_contracts.md),
[source ledger](../sources/m2_destination_trial_sources.json) and
[validation records](evidence/m2-destination-trial.json).

Private CityHub initialization observes **243 created, 541 editor-only, 207 unknown
and 35 failed** source entries. Player entry at `01_egressa` succeeds for the supported
snapshot and retains raw health/ammunition; these other failures remain visible.
SubwayA observes **203 created, four transferred sounds, 372 editor-only, one unknown
Gideon and zero failures**. Malformed Bridge rejects before world creation. None of
these observations reclassifies a partial destination as playable.

The private check repeats three source-world/player cycles. It compares all seven
intrusive chains (every node, previous/next/data pointer and count), active camera,
15 production NPC animation-table pointers, shared table owners, live voice identities
and sample cursors, client/player identities, source simulation time and revision.
A real decoded Cultist voice is started explicitly to test shared ownership even
while the intro camera suppresses autonomous AI. It is labelled a diagnostic action.
The process-wide monotonic audio ID allocator advances; it must not reuse IDs.
Source hardware audio already queued is not claimed paused or rewound.

Synthetic checks cover repeated success, start-camera activation, missing marker,
missing ground, missing sound, malformed/missing DAT, invalid start name, callback
and foreign-session refusal, original key removal only in the copied player, and
report ownership through unload. The GPU objectives scenario performs the temporary
CityHub callbacks/entry/cleanup with the UI hidden and after reload, while retaining
its existing authored ExitTrigger, source-state and safe-refusal assertions.

## Validation

Debug passes **556/556 headless checks** (76.42 s)
and **41/41 GPU checks** (163.64 s). Inspector-free Release passes
**556/556 headless checks** (10.51 s) and **41/41 GPU checks**
(113.36 s). Each totals **597/597**, including 119 private-data
checks (87 headless). Targeted Clang ASan/UBSan/leak checks pass **104/104**
(113.43 s). Neither GPU matrix emits Vulkan validation errors/VUIDs.

Commands: `cmake --build build/debug -j3`, `cmake --build build/release -j2`,
then `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use existing Clang `build/sanitize` and targets world_tests, object_contract_tests,
exit_tests, player_tests, player_restore_tests, player_keepalive_tests, ai_runtime_tests,
ai_perception_tests, ai_model_key_tests, camera_tests and pickup_tests, followed by
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest`
with regex `^(world|objects|exits|player|player_restore|player_keepalive|ai_runtime|ai_perception|ai_keys|cameras|pickups)\.` and `-j2`.

`--inspect-player` exits 0 and `--start-game` retains exit 6 for incomplete gameplay.
11 changed C++ files pass clang-format 23.1.3; 2,436 local Markdown links resolve.
All 10,923 reference files and six repositories remain unchanged. Evidence includes
the committed project patch/hashes, test selection and detailed destination/GPU logs.
The Debug capture shows the temporary entry and cleanup observations with the
source revision intact and unresolved entries visible; captures remain ignored.

Two new synthetic fixtures needed correction: the missing-ground point originally
sat above a reachable floor, and the source pickup originally intersected that
floor. The corrected fixture places the marker outside the floor and the pickup
above it, and verifies source stepping before any rehearsal. No production collision
rule or success assertion was relaxed. Initial failing logs are retained.

Reference sources/repositories and commercial assets are unchanged. No retail execution
comparison, other OS, graphics backend or XR validation is claimed. A future accepted
transition must retain/promote a validated destination and handle full client state
without replaying side effects or discarding a viable source on failure. Required
world/service coverage, Gideon, illumination and that safe transfer still block M2;
M3–M8 remain later milestones.
