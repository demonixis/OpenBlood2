# M2 — Original smell trail and AI search state

The original player now drops periodic SmellHints, and AI navigation can execute
`AI_STATE_SearchSmellTarget` instead of refusing a state already selected by Guard
and route recovery. **M2 remains incomplete.** No new NPC class, weapon, renderer,
pathfinder or dependency is introduced.

The selected code comes from [PlayerObj.cpp](../../../../sources/blood2/ObjectDLL/PlayerObj.cpp)
and [AI_Mgr.cpp](../../../../sources/blood2/ObjectDLL/AI_Mgr.cpp), retaining the existing
original [SmellHint](../../../../sources/blood2/ObjectDLL/SmellHint.cpp) list and timeout.
[Contracts](../../contracts/ai_smell_contracts.md) and the
[source ledger](../sources/m2_ai_smell_sources.json) record the boundaries.

## Behavior and lifetime

Player updates preserve the strict half-second deadline, inclusive 10-unit 3D
spacing from the global last hint, and clock advancement before creation. A
stationary player does not advance that clock just because a check ran. Hints
remain world-owned for their original 20 seconds, including after the player
leaves. Pending removal is skipped through bounded, checked traversal; unload
and destination preparation retain their existing ownership boundaries.

The AI body preserves decision order, the visible-hint query, double throttled
ledge query, cached movement height, inclusive arrival and Run command. Advancing
to the next hint faces it and advances phase 2 to 3 exactly as in the source.
Normal smell perception clears the combat target and does **not** initialize a
trail cursor. The search therefore requests another decision in that situation;
it does not invent a new ability to track the player automatically. Registered
NPC smell ranges remain zero.

The source's retained list pointer and inconsistent inter-object link updates
are replaced by a checked non-owning hint handle. A successor acquires its link
before releasing the previous one; hint expiry clears the correct cursor before
the original Idle transition. Failed perception, recursion, stale identities and
missing movement/model services remain explicit failures.

The Skeleton inspector exposes actual player check/drop counts, global hint
count, clock, last drop identity and error text. AI inspection shows cursor
identity, searches/advances/invalidations, cached position and last exit reason.
These values remain observations when the inspector is hidden; opening a panel
does not run the state or advance its clock. Ordinary object inspection includes
invisible SmellHints and their schedules.

## Verification

Synthetic cases check strict time boundaries, exact 3D spacing, another hint at
the global tail, pending removals, dead/spectator guards, invalid clocks, expiry,
player disconnect/reentry, target/stimulus/cursor guard priority, actual perception
failure, visible-query refusal/success, ledge throttling, Run, successor/sentinel
phases, link replacement, stale handles and nested decisions. The source route
body is exercised with a deliberately seeded cursor and actual dead-attacker
memory retaining the prior target; ComputeStimuli is not mocked. A ray-hit hint
is used only in the explicitly labelled positive visibility fixture.

Two private SubwayA cycles complete the original camera intro and move the real
player through MoveMgr: **6 drops, 841 checks and 6 live hints per cycle**, then
zero hints after unload. Six real NPC model/animation tables also exercise the
search state's no-target exit. These tests do not establish retail smell-tracking
reachability or a full authored encounter.

Targeted regressions cover player entry/movement, snapshots/restoration/keep-alive,
world destination preparation/NEW_LEVEL, AI perception/state/navigation/activation,
object lifetime and mouse combat. NVIDIA tests cover the existing character,
inspector lifetime/reload, restored player, combat/corpse, objectives and actual
CityHub transfer paths. These are graphics regressions, not a new visible scent FX.
The [evidence ledger](evidence/m2-ai-smell.json) retains commands, results, logs,
source integrity and the exact implementation patch.

| Validation | Passed after targeted rechecks | Private cases included |
| --- | ---: | ---: |
| Debug headless | 135 / 135 | 25 |
| Inspector-free Release headless | 135 / 135 | 25 |
| Clang ASan/UBSan/leaks | 135 / 135 | 25 |
| Debug NVIDIA | 7 / 7 | 6 |
| Release NVIDIA | 7 / 7 | 6 |

An additional Debug character GPU check passes after wrapping the inspector
legend and separating cursor identity from counters for narrow panels.

The sanitizer movement test reached its original 60-second harness deadline both
concurrently and serially. Direct execution of its unchanged three-cycle body
completed successfully in 58.52 seconds, including leak checks. The CTest deadline
now allows 120 seconds **only with sanitizers**; normal builds retain 60 seconds.
The final instrumented CTest run passes. Every intro tick, movement assertion and
reload remains; no production timing bound changes. Initial failures, direct
execution and final retries are retained in the ledger.

The initial strict-clock fixture used a 1-ms scheduler increment whose floating
representation could fall just before the next callback; it now uses 2 ms after
the exact half-second boundary. The production strict comparison is unchanged.
The inspector build caught an incorrect scalar assumption about ObjectId; it now
displays the actual slot and generation. Two existing tests expected the missing
search-state refusal; their assertions now require the real source guard/decision
and absence of a fabricated cursor. No production safety bound or required-service
failure was relaxed.

All reference sources and commercial assets remain external and unchanged. No
native macOS run is claimed for this increment; existing shaders/Metal support are
unchanged. Full AI/world saves, spectator/multiplayer, remaining required FX and
other M2 service gaps remain open. The continuation automation stays active.

Implementation commit: `d7b744394b6a9fde0969362b3b6374bd52981f8e` in the independent Blood2 repository.
