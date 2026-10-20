# M2 playtest fix: preserve NPCs until player entry

The reported non-attacking NPCs led to a reproduced interactive startup defect:
the app used to tick the world while waiting for the user to enter a player. In
SubwayA, three original NPCs then selected themselves through the empty-opponent
fallback and became permanently suspended by the checked link contract. Other
NPCs could select their own faction. The existing isolated combat test had entered
the player before ticking, so it did not cover this application workflow.

The app now holds a loaded world's simulation until original player entry, and
again after player leave. Inspection remains responsive; explicit removals drain.
An Enter player button and waiting status appear in the header, and Session shows
NPC ready/failed/active counts. `--view-world` still enters immediately. GameSession's
explicit diagnostic stepping and original AI, targeting, activation, weapons and
camera rules are unchanged. No runtime failure is cleared to make the test pass.

See [contracts](../../contracts/session_playback_contracts.md),
[provenance](../sources/m2_npc_entry_sources.json) and
[execution evidence](evidence/m2-npc-entry.json).

## Evidence and scope

The preliminary trace reproduced three suspended NPCs after delayed entry. It also
confirmed autonomous Cultist attacks in an immediate-entry session when the player
was placed inside a real train encounter. Remaining stationary at the initial spawn
did not provoke an attack; this is not itself proof of broken combat. These probes
are retained as diagnostic evidence, not retail playthroughs.

The new regression waits 180 application frames without advancing game time, enters
the player and runs the original camera1/camera2 intro to completion. It then moves
only the player to (-32,170,-1880) in actual SubwayA. NPC position, authored state,
weapons and targeting remain untouched. Across two fresh world loads an original
Cultist autonomously fires **15 shots** in five simulation seconds and reduces
player health from 100 to **39.9175**. All fifteen NPCs remain runtime-ready. Leaving
the player holds simulation again; reload invalidates old identities and cleans
faction lists, strings/messages and client ownership.

The real application GPU test draws twelve pre-entry frames with **simulation time
0, 15 ready NPCs, 0 failed NPCs and 0 AI_Update calls**. Its capture verifies the
visible waiting status and entry button. Existing diagnostics still exercise raw
no-player stepping and retain the original three-failure regression.

## Validation

Debug and inspector-free Release each pass **627/627 checks**: 582 headless plus
45 serial GPU, including 127 private-data cases (92 headless). Debug times are
81.73 s / 172.31 s; Release times
are 14.01 s / 114.03 s.
Targeted Clang ASan/UBSan/leak checks pass **38/38**, in 74.61 s.
No sanitizer error, leak or GPU validation error/VUID was observed. The final private
entry scenario gives the same 15 shots / 39.9175 HP in Debug, Release and sanitizers.
The complete Release headless summary is retained, but a later CTest metadata-list
command reset its detailed LastTest log. The two new playback cases were rechecked
successfully to retain their output; that focused transcript is included separately.

Commands from Blood2: `cmake --build build/debug -j2`,
`cmake --build build/release -j2`,
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`, then
serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
The existing Clang `build/sanitize` builds session_playback_tests,
object_contract_tests, ai_runtime_tests and ai_perception_tests, then runs regex
`^(session_playback|objects|ai_runtime|ai_perception)\.` at `-j2` with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.

Five changed/new C++ files pass clang-format 23.1.3. All 2,542 local Markdown links
resolve; the 10,923 inventoried reference files and six reference repositories are
unchanged. The initial compilation exposed a missing inspector AI header; the
include was corrected before the successful final builds and matrices. Evidence
retains the logs, selected transcripts, source hashes, project snapshot and patch.
Linux/SDL/Vulkan was exercised; other platforms and retail parity were not tested.
No commercial files or new dependencies are committed.

This fixes the startup corruption demonstrated above. It does not establish every
NPC encounter, human control path, retail PVS activation or full combat parity.
The missing/pink train-window material is next, before more cosmetic FX. Required
services, safe NEW_LEVEL and the actual M2 gate remain incomplete.

Read-only follow-up: `Blood2/src/engine/world_view.cpp` currently assigns pink to
transparent surface flags and keeps their texture unbound even after successful
decoding. Match the exact train-window surfaces/resources before concluding that
the reported color is an absent texture. No glass rendering change is included here.
