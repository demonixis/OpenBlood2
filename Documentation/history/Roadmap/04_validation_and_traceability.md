# Validation, Performance and Audit Traceability

This is the acceptance plan for the new project. Audit defects guide prevention and selective-import checks. [M0's executed results](../../implementation/reports/m0.md) now cover startup/configuration and the inspection shell on Linux/Wayland/Vulkan; gameplay, broader platform and performance gates below remain planned.

## Evidence Rules

Each milestone report records the repository commit, dependency/compiler versions, build options, platform/device, input scenario, expected outcome, observed outcome and artifact location. Use distinct statuses: **passed**, **failed**, **skipped because input/device is unavailable**, and **not run**. A successful compile, discovered test, parsed resource or visible model proves only that specific result.

Use three complementary oracles:

1. Original source and header contracts for callback order, API semantics and game rules.
2. Synthetic cases with explicit expected behavior for engine ownership, parsers, messages and time.
3. Observations of a specified original game build using the same content and actions for engine behavior absent from available source.

Keep disagreements visible. The released game may expose behavior that is not obvious from source, and a prototype can reproduce the same mistake as another prototype. Differential tests are useful, but agreement between two readers alone is not proof. Record uncertainties and intentional fixes instead of silently redefining the fidelity target.

## Regression Scenarios

| Area | Minimum meaningful scenario | Acceptance evidence | Gate |
| --- | --- | --- | --- |
| Interface dispatch | Call real virtual methods and callback members through the shared game headers; derived/base/aggregate forwarding | Correct target, arguments and exactly the intended callback count; no recursion or undefined cast | M1 |
| Creation | World, runtime-string and restore reasons; precreate mutates flags/position/model; nested creation with independent properties | Final internal state matches mutations; outer context restored; defaults/inheritance correct | M1, restore completed M5 |
| Lifetime | Self-removal, removal of next object, repeated removal, link/attachment cleanup and queued messages | No invalid access; one destruction; correct notification order and stale-handle rejection | M1–M2 |
| Scheduling | Explicit 0.01-second update, zero disabling, inactivity and a callback returning `1` | Requested schedule survives; callback return is not used as an interval | M1 |
| Formats | Truncated headers, invalid offsets/counts, repeated sections, recursive archive entries, known valid variants | Bounded error with context; no infinite loop, excessive allocation or accepted partial gameplay world | M2 |
| Resource lookup | Mixed case, duplicate names across mounted archives, missing resources and repeated load/unload | Documented precedence, deterministic lookup and correct ownership | M2 |
| Coordinates | Known plane/box, model root and child node, scaled model, rotating world model | Rendered, collidable and attachment transforms agree in the declared conventions | M2–M4 |
| Session | Enter level, attach local client, spawn original player, exchange commands/messages, unload | Original shell callbacks run in the required order; no hidden free-camera substitution | M2–M3 |
| Movement | Wall slide, corner, step, slope, jump, floor transition and moving door | Positions/contacts match source/reference expectations within stated tolerances | M3 |
| Collision/combat | Hitscan filters and nearest hit; projectile travel/contact; damage/ammo and death | Queries reach world service; original weapon/projectile/enemy paths determine outcomes | M3, breadth M5 |
| Trigger/pickup | Named target, delayed activation, locked/rotating door as applicable, inventory change | Original object messages and state changes; no fallback command on malformed data | M3, breadth M5 |
| AI/animation | Acquire/lose target, attack/death, loop/finish, model-string keys; repeat with enemy culled | Same logical progress and gameplay events despite visibility changes | M3–M5 |
| Time | Repeat at 30/60/144 Hz; add a 100 ms stall; cross animation loop boundaries | No lost/doubled events or unexplained time loss; differences explained by tested scheduling policy | M2–M6 |
| Presentation | Same camera/content compared with original-game observations and each SDL_GPU backend | Required lightmaps, alpha, colors, models, sockets and HUD are correct; differences recorded | M4–M5 |
| In-game inspection | Select visible, hidden, inactive and non-rendered elements; inspect properties/resources; pause/step; destroy selected object; reload | Complete, current state and useful overlays; missing content visible; no stale selection or input/simulation side effects | M0 shell, M1–M5 coverage |
| Audio | Positional movement, loop/start/stop, simultaneous voices, completion and level unload | Audible behavior plus correct handle state; no placeholder-only success | M3–M5 |
| Persistence | Save amid a moving door, active enemy/projectile, delayed trigger and inventory change; restart process and restore | Equivalent observable state, repaired identities and consistent future events | M5 |
| Campaign | Start-to-finish progression, transitions, bosses, death/restart, selected character/difficulty variants | Coverage matrix and actual playthrough results; no required unsupported class | M5–M6 |
| Mobile lifecycle | Pause/background/resume, audio focus, content import and memory pressure | Valid session/resource state and usable controls on actual devices | M7 |
| XR | Stereo submission, tracked actions, session loss, recenter and repeated mode startup | Valid frame lifecycle and defined comfort/input behavior on a named runtime/headset | M8 |

Save compatibility across supported platforms is an M6/M7 test; compatibility with original retail save binaries remains a separate decision. Floating-point tolerances must be chosen per quantity and scenario, with units and reasons, rather than one broad epsilon that masks drift. For random behavior, record seeds and the RNG policy/version and compare controlled cases; do not promise bit-identical arbitrary playthroughs across platforms without evidence.

## Test Organization

Keep CTest groups for contracts, formats, integration and private-asset scenarios. Adapt the requirements demonstrated by the [engine](../Audit/probes/engine_contract_probe.cpp), [aggregate](../Audit/probes/aggregate_dispatch_probe.cpp) and [ABC](../Audit/probes/abc_cycle_probe.cpp) probes into assertions against the new services as they are implemented. Do not require an old prototype build or import its implementation just to run a test.

Run portable synthetic tests without a window, GPU, retail files or headset. Configure asset tests to use the [supplied local installation](readme.md#local-reference-installation); its directory and key files have been confirmed, but parsing/gameplay still need execution evidence. On other machines a missing root produces a skip, not a pass. Keep commercial content outside test fixtures. Use small synthetic malformed inputs for portable regression tests.

Use ASan/UBSan for ownership/format/integration cases where supported; pin a working sanitizer toolchain and retain failures. The audit's sanitizer setup constraints are recorded in [build verification](../Audit/01_build_verification.md) and should not be confused with game defects. Add bounded fuzzing of parsers/message readers after their failure contracts exist. Sanitizer/fuzzer success complements, rather than replaces, behavior comparison.

The existing C# test suite is useful reference material. Its 59 executed non-asset checks and five misleading asset-skip `PASS` lines must not become a claim of 64 gameplay tests or native Godot execution. Import relevant cases only when they cover a current requirement.

## Inspection Coverage

Maintain a small category checklist matching the [editor scope](02_architecture.md#in-game-inspection-editor). For each category, verify discovery, selection, useful fields and visual diagnostics where applicable. Compare source-world entries with instantiated elements; report unknown classes, missing resources and rejected properties rather than hiding them from the editor.

Check selection after deletion/reload, pause/single-step behavior, safe temporary edits and input focus. Opening the editor must not itself change simulation state. Test both inspector-enabled development builds and inspector-disabled release builds. This is runtime inspection of existing data, not a requirement for an ECS, generic reflection system or full level-authoring suite.

## Performance Measurement

**Initial design target:** 60 fps at 1920×1080 on named representative desktop hardware, with a steady-play frame-time target of p95 ≤ 16.7 ms and p99 ≤ 25 ms. These are proposed budgets, not measurements or a promise for unspecified hardware. Confirm the device and settings in M0 and reassess the budget explicitly after M3's first baseline. Record load/transition times separately from steady-play percentiles.

For mobile, choose measured 30 or 60 fps tiers on real devices during M7. For XR, select the headset's supported refresh rate and corresponding frame budget during M8; for example, 90 Hz allows about 11.1 ms per display period. CPU/GPU headroom and compositor behavior need direct measurement, not extrapolation from the flat-screen result.

Capture these scenarios with a fixed camera/input sequence where possible:

| Scenario | Main measurements | Suspected cost from audit |
| --- | --- | --- |
| Cold launch and first level load | Wall time, bytes read, peak resident memory, decoded/uploaded bytes | Whole-archive loading and duplicated buffers |
| Busy visible combat | CPU simulation/queries/animation, GPU frame time, draws/uploads, allocations per frame | Linear world queries and repeated skinning/upload work |
| Same combat with actors offscreen | Event trace plus CPU pose/render work | Culling currently freezes animation time; optimization must preserve events |
| Repeated load/transition/restore cycles | Live objects/resources/handles and resident memory trend | Stale references, unreleased caches and lifetime errors |
| Large or malformed resource | Time to fail, allocation ceiling, error context | Cyclic traversal, unchecked counts and recursive structures |
| Same scene with inspection open/closed/disabled | Frame time, debug draws and allocations | Bound editor work and remove it from builds with inspection disabled |
| Sustained mobile/headset run | Frame-time distribution, thermal state and memory over time | Device limits and multi-view rendering cost |

For comparable desktop captures, use an optimized build with symbols, fixed settings, a warm-up period and at least three repeated runs of the same measured interval. Record duration, sample count, p50/p95/p99, CPU/GPU timing when available, memory and allocation totals. Choose the interval long enough to cover the actual workload; avoid presenting one average FPS number as a benchmark.

Set absolute memory/cache budgets from M3 measurements on the selected device tier, then enforce them in M6/M7. As an initial lifetime stress case, repeat at least 20 level load/unload or transition cycles and compare live ownership counters after returning to the same state. Investigate continued growth beyond bounded caches. A system allocator retaining pages is not by itself proof of an object leak; correlate resident memory with owned-resource counts.

Prioritize fixes in this order when measurements support them: avoid duplicate archive residency, accelerate spatial queries, reuse temporary buffers, reduce redundant pose work, batch compatible draws, and use verified GPU skinning. Add worker jobs only for an identified bottleneck with controlled ownership. Flag repeatable regressions above 10% in the same benchmark configuration for investigation, accounting for run variance; this is a review threshold, not permission to accept smaller fidelity regressions.

## Audit Finding Disposition

Every ID below refers to the [unchanged phase-2 register](../Audit/05_findings_and_coverage.md). Actions describe the new project's response. M0 has delivered the new dependency graph and test discovery (A10/A16 scope), initial provenance (A09) and a non-VR startup path (part of A11/A17); other gates remain planned. Preventing a defect or excluding a component does not fix the historical repository.

| ID | Planned response | Completion evidence | Stage |
| --- | --- | --- | --- |
| A01 | Defer deletion and make update traversal safe | Self/other removal under sanitizers, correct final destruction | M1 |
| A02 | Restore explicit scheduling semantics | 0.01 delay retained; zero disables; callback return independent | M1 |
| A03 | Apply precreate output and explicit creation/context rules | Mutation/reason/nested-property tests | M1 |
| A04 | Preserve original nonrecursive base/aggregate dispatch | Original callback path through the new adapter executes the expected number of times | M1 |
| A05 | Implement typed interfaces without importing manual tables | Actual virtual and callback calls through shared definitions | M1 |
| A06 | Connect original session and required level/player/AI objects | Original-code slice, then campaign class/service coverage | M2–M5 |
| A07 | Implement and connect required collision/model/lookup/link/attachment services | Service matrix plus original-caller integration cases | M1–M5 |
| A08 | Implement persistence and stop reporting false success | Process-restart restore with remapped references and failure tests | M5 |
| A09 | Review only selected imports/adaptations and exclude behavior bypasses | Import ledger with donor provenance and relevant tests | M0–M5 |
| A10 | Build a minimal new dependency graph; omit ImGuizmo initially; record SDL main SHA | Fresh standalone build with recorded dependency revisions | M0 |
| A11 | Keep imported feature flags self-contained; no need to repair unused donor executable | Selected runtime compiles/runs without VR; imported relevant code tested in both modes | M0, M4, M8 |
| A12 | Reject ABC cycles and invalid section traversal | Synthetic cycle returns bounded parse failure | M2 |
| A13 | Add strict bounds/count/recursion/error contracts | Malformed corpus and fuzz results; no hidden partial gameplay load | M2 |
| A14 | Connect model APIs/events; separate logical animation from visibility | Original attack/completion behavior stays correct while culled | M2–M5 |
| A15 | Do not import Godot placeholders as functioning original effects/projectiles | Real audio/effects plus original projectile object behavior | M3–M5 |
| A16 | Discover tests and report skips honestly in selected runtime | CTest report with separate passed/failed/skipped counts | M0 onward |
| A17 | Canonical definitions, IDs, portable flags and graphics/XR boundaries | Compiler/platform matrix, portable save/message tests, actual XR validation | M1–M8 |
| A18 | Measure costs, then reduce I/O duplication, query and animation overhead | Repeatable before/after captures with unchanged behavior | M3 baseline, M6–M8 |
| A19 | Share time policy and retain fractional/event progress | Rate/stall/loop tests and original-reference comparisons | M2–M6 |
| A20 | Treat later LithTech as a generation-specific donor, not required engine dependency | Component provenance/compatibility notes; no prerequisite to build the whole later tree | M0–M5 |

## Progress Reporting

For each milestone record status, delivered behavior, tests, skipped inputs/platforms, remaining blockers, related audit IDs and the next bounded task. Track inspection coverage with the relevant system. Do not edit the historical audit to erase old failures; record prevention or fixes and evidence in the new project's record.

The meaningful progress measure is **verified behavior and closed contracts**: a working original player, a correctly timed attack, a restored save, a complete campaign path, or a tested platform. File counts, lines modernized, class registration counts and the size of a new renderer are not substitutes.
