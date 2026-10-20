# M2 continuation: destination preparation without leaving the current world

The engine can now parse a destination DAT, build its primary collision index and
check every registered source object's property schema while the original current
session remains intact. Both the Session inspector and blocked authored ExitTrigger
requests expose this report. **Automatic NEW_LEVEL and M2 remain incomplete.**

This addresses the first safe-transition prerequisite. The original source orders
KeepAliveSave, engine LoadWorld, KeepAliveLoad and RunWorld. Current player restoration
can enter a separately loaded destination, but destination original callbacks/start
triggers can still fail after changing state. A metadata scan cannot authorize
teardown or guarantee a playable level. No successful stub or fresh player replaces
that missing transaction. The original commented-out revisit-save branch stays off.

Fresh NEW_GAME loading and read-only inspection now share the same bounded CPU
preparation. Schema checking reuses creation's const validation, with no factory,
callback, live resource cache or diagnostic mutation. Reports distinguish known
schemas from editor-only classes, missing classes and invalid properties, retain
source/destination revisions and own their rows through unload. A queue of 256 exit
requests retains every source capture/refusal but performs one final destination scan.

See [contracts](../../contracts/destination_contracts.md),
[source selections](../sources/m2_destination_sources.json) and
[validation records](evidence/m2-destination-preparation.json).

Inspection of SubwayA reports **207 schema-checked, 372 editor-only, one unknown**
source entry, with 116 models and 14,300 primary collision triangles. CityHub reports
**247 schema-checked, 541 editor-only, 207 unknown and 31 invalid** entries, with
100 models and 39,724 primary triangles. All 31 schema failures are old Sound
VoiceType properties. These differ deliberately from live creation totals: original
callbacks can transfer sounds, filter objects or fail later.

## Validation

Debug passes **554/554 headless checks** (73.53 s)
and **41/41 GPU checks** (165.03 s). Inspector-free Release passes
**554/554 headless checks** (7.08 s) and **41/41 GPU checks**
(108.55 s). Each totals **595/595**, including 118 private-data
checks (86 headless). Targeted Clang ASan/UBSan/leak checks pass **50/50**
(48.47 s). Neither GPU matrix emits Vulkan validation errors/VUIDs.

Commands: `cmake --build build/debug -j3`, `cmake --build build/release -j2`,
then `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use existing Clang `build/sanitize` and targets world_tests, object_contract_tests,
exit_tests, player_tests, player_restore_tests and player_keepalive_tests, followed
by `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest`
with regex `^(world|objects|exits|player|player_restore|player_keepalive)\.` and `-j2`.

`--inspect-player` exits 0 and `--start-game` retains exit 6 for incomplete gameplay.
14 changed C++ files pass clang-format 23.1.3; 2,413 local Markdown links resolve.
All 10,923 reference files and six repositories remain unchanged. Evidence includes
the committed project patch/hashes, test selection and detailed destination/GPU logs.
The initial new-test compile omitted a required null callback-data argument; it was
fixed before behavioral tests, and both build logs are retained.

Private checks inspect SubwayA and CityHub during three live player/world cycles,
without advancing NPC updates, audio or simulation time, replacing handles, changing
inventory/health or touching the world revision. Malformed 16_bridge rejects without
losing the source session. The existing authored exit GPU scenario now checks the
CityHub report with UI hidden and after reload; captures stay in ignored build files.
No retail execution comparison or other platform/backend validation is claimed.

Destination original callbacks, filtering, resource loads, mover binding, start-point
selection/triggers and final player clearance remain unverified by this report.
CityHub is inspected, not transferred to or declared playable. Source class coverage
in SubwayA remains 203 created, four transferred sounds, 372 editor-only, one unknown
Gideon and zero failures. Required services/FX and safe NEW_LEVEL still block M2;
M3–M8 remain later milestones.
