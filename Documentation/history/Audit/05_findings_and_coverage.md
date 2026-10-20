# Findings, Coverage, and Unresolved Questions

## Finding Register

Severity describes the effect on a faithful original-code rebuild, not an implementation schedule. **Critical** means memory/dispatch failure or a fundamental execution barrier. **High** means a major functionality, fidelity, or build defect. **Medium** means a significant robustness, portability, performance, or validation gap. The roadmap may later assign priorities and dependencies separately.

| ID | Severity | Finding | Evidence | Detail |
| --- | --- | --- | --- | --- |
| A01 | Critical | Removing an object during its update frees memory still used by the engine loop | Executed: ASan heap-use-after-free | [Lifetime contract](02_original_code_and_contracts.md#removal-reproduced-use-after-free), [log](evidence/engine_contract_remove.log) |
| A02 | High | Engine update overwrites explicit next-update scheduling with the callback return value | Executed: 0.01 seconds becomes 1 second | [Scheduling](02_original_code_and_contracts.md#scheduling-callback-return-value-is-not-an-update-interval), [log](evidence/engine_contract_values.log) |
| A03 | High | Precreate mutations are not applied to internal object state; initial-update reason is wrong for world creation | Executed with synthetic world-property context | [Creation](02_original_code_and_contracts.md#object-creation-confirmed-contract-mismatch) |
| A04 | Critical | Adapted aggregate forwarding re-enters virtual object-message dispatch recursively | Executed: one message enters callback eight times before explicit probe guard | [Dispatch](02_original_code_and_contracts.md#recursive-aggregate-forwarding), [log](evidence/aggregate_dispatch.log) |
| A05 | Critical | Allocated ServerDE compatibility table leaves virtual/common/physics prefix zero while game headers still declare virtual calls | Source-confirmed active setup path; full ABI execution not attempted | [Server interface](02_original_code_and_contracts.md#server-table-without-virtual-interface-state) |
| A06 | High | Level object creation is restricted to four classes; original player/AI/combat/session execution is not demonstrated | Source-confirmed whitelist and application lifecycle tracing | [Active integration](02_original_code_and_contracts.md#what-static-integration-actually-provides) |
| A07 | High | Original-game collision, animation, named lookup, lists, links and attachments reach incomplete services | Source-confirmed adapter-to-facade traces | [Service matrix](02_original_code_and_contracts.md#services-required-by-the-original-game) |
| A08 | High | Save and restore report success without implementing persistence | Source-confirmed implementations and original callers | [Service matrix](02_original_code_and_contracts.md#services-required-by-the-original-game) |
| A09 | High | Adaptations include gameplay changes and bypasses, beyond compiler/platform fixes | Source comparison: aggregate forwarding, Door callbacks and message fallback; build exclusions | [Preservation impact](02_original_code_and_contracts.md#preservation-impact-of-existing-adaptations) |
| A10 | High | Fresh C++ generation fails on the tracked ImGuizmo dependency's changed source layout | Executed for both projects; temporary source-directory override permits generation | [Build verification](01_build_verification.md) |
| A11 | High | NoMadEngine fails compilation with VR disabled | Executed; `m_vrEnabled` is declared only under `OPENXR_ENABLED` | [Failure](evidence/nomadengine_build_override.log) |
| A12 | High | C++ ABC section traversal can fail to terminate | Executed synthetic cycle, timeout 124; loop inspected | [Parser analysis](03_formats_and_animation.md#confirmed-parser-termination-defect), [log](evidence/abc_cycle.log) |
| A13 | Medium | REZ recursion, count validation and partial DAT recovery lack a consistent strict failure contract | Source-confirmed examples; no broad fuzzing or retail corpus | [Parser analysis](03_formats_and_animation.md#confirmed-parser-termination-defect) |
| A14 | High | Visual animation is disconnected from original model APIs/events; culling also freezes controller time | Source-confirmed stubs, controller and renderer paths; game impact inferred | [Animation](03_formats_and_animation.md#animation-is-part-of-gameplay) |
| A15 | High | Godot sound/effect intents and projectile representation are not equivalent to original playback/projectile behavior | Source-confirmed effect presenter and combat resolver | [Godot runtime assessment](04_prototypes_and_portability.md#godot-prototype-testable-domain-logic-partial-runtime-presentation) |
| A16 | Medium | C++ test discovery is empty; C# local-asset skips appear as passes and native Godot runtime is outside console tests | Executed discovery, test output, and harness inspection | [Checks and limits](01_build_verification.md#test-discovery-and-probe-qualification) |
| A17 | Medium | Multiple interface/type definitions, raw pointer messages, x86 feature flags and OpenGL-specific XR bindings constrain portability | Source-confirmed; only Linux x86_64 built | [Portability](04_prototypes_and_portability.md#portability-assessment) |
| A18 | Medium | Linear world queries, whole-archive memory, and animation fallback allocations have unmeasured costs | Source-confirmed structure; no runtime performance measurements | [Performance](04_prototypes_and_portability.md#performance-identified-costs-no-benchmark-claims) |
| A19 | Medium | Animation frame-time truncation and NoMad physics time clamping can advance subsystems at different rates | Source-confirmed arithmetic/control flow; runtime divergence not measured | [Animation timing](03_formats_and_animation.md#animation-is-part-of-gameplay), [physics timing](04_prototypes_and_portability.md#nomadengine-useful-engine-work-independent-game-rules) |
| A20 | High | Later LithTech source is not a verified drop-in Blood 2 engine and cannot currently generate with tested CMake settings | Source generation/format differences; reproduced configuration failures | [Reference-tree build](01_build_verification.md#reference-lithtech-tree), [format boundary](03_formats_and_animation.md#how-the-larger-engine-tree-helps) |

The successful OpenBlood2 and VR-enabled NoMadEngine builds, successful C# builds, and completed synthetic C# checks are positive results alongside this register. None of the findings says that the original-code approach is intrinsically infeasible.

## Coverage by Area

| Area | Work performed | What is not established |
| --- | --- | --- |
| Workspace baseline | Rechecked six commits, Git status and all 10,923 indexed text hashes | Files outside inventory scope are not included in this integrity statement |
| Original engine API demand | Lexical scan across original C/C++ outside AppHeaders, using declarations from five engine headers; selected call sites read | No AST/type-resolved call graph, preprocessor configuration model, exhaustive API conformance or runtime reachability |
| OpenBlood2 build/integration | Build declarations, class registration, adapters, object lifecycle, properties, messages, application update/render and level creation traced | No complete gameplay session or complete audit of every original class |
| OpenBlood2 core behavior | Production-core creation/scheduling/removal probes, adapted aggregate dispatch probe | No full-application sanitizer run, multithreading stress test, or exhaustive ownership audit |
| Original game behavior | Selected lifecycle, Door, Trigger, AI, weapon, movement and persistence references; selected adapted changes compared | Not every enemy, weapon mode, boss, spell, pickup, multiplayer mode, save version, or campaign script |
| Formats | Selected REZ/DAT/ABC/DTX/PCX reader paths, C# test cases, cyclic ABC probe, coordinate conversions | No retail asset corpus, complete version support matrix, property-value differential, fuzz campaign or file-format specification proof |
| Animation/render connection | Controller updates, resource-manager culling, CPU/GPU paths, Godot builder/skeleton route and original event dependence inspected | No rendered frame comparison, GPU capture, animated attachment test, or native Godot animation execution |
| NoMadEngine | Successful VR-enabled build; failed non-VR build; GameBase, factory, physics, weapon/trigger integration sampled | No Jolt-versus-LithTech behavior comparison, complete ECS subsystem review, or running game |
| Godot prototype | Main/test builds; standalone tests; scene-composition entry points, formats/gameplay separation, effect/message and model paths sampled | No Godot editor/runtime, scene launch, audio output, export build, or complete C# gameplay equivalence |
| Portability | CMake/platform branches, type shims, feature flags and XR bindings inspected; Linux x86_64 execution | Windows, macOS, ARM, mobile, headset and release-mode validation |
| Larger source references | Later-engine provenance, build wiring, interfaces, model-format boundary and selected runtime references | No exhaustive review of NOLF, NOLF2, FEAR, tools, vendor libraries, or modified Shogo Aegis |
| Distribution/provenance | Repository identities and local source descriptions retained | No independent provenance or licensing determination; source availability alone is not treated as such a determination |

The inventory covers roughly 3.34 million physical code lines including vendor code and duplicates. **This phase is a targeted deep audit of the Blood 2 reconstruction path, not a claim that every line in those trees has been manually reviewed.** The cited functions, probes, logs, and scope table make that boundary explicit.

## Evidence Reliability

Evidence was collected in the current local checkout. [snapshot.json](evidence/snapshot.json) records the exact commits and fetched dependency revisions. [Command metadata](evidence/readme.md) records working directory, command arguments, timeout, time and exit status. Failed commands remain visible, including audit setup mistakes and expected defect reproductions.

The lexical [API candidate report](evidence/original_api_candidates.json) is a navigation aid. Its counts include same-name candidate calls and exclude non-arrow calls, macro-generated calls and semantic resolution. It must not be presented as “320 required APIs completely audited” or “12,596 runtime calls.”

No real gameplay inputs, original-game observations, proprietary asset contents, detailed asset manifests, screenshots, or performance captures are included. The absence of those artifacts limits fidelity conclusions. In particular, successful parsing, drawing, compilation, or helper-level tests does not establish a complete or faithful game.

## Questions Still Open for the Roadmap Discussion

These questions identify missing evidence and decisions; they do not prescribe milestones, implementation order, estimates, or a selected architecture.

1. What single authoritative definition or explicit bridge should connect original game objects and engine services while retaining source-level reuse?
2. Which original client/server session responsibilities are required for a local single-player run, and how will original messages and object identity be represented?
3. What exact creation, scheduling, deletion, property, link, attachment, save/load and animation-event contracts must be preserved?
4. Which existing parser/rendering/physics components can satisfy those contracts with bounded adaptations, and which carry behavior that must be replaced or corrected?
5. What original-game observations and synthetic/differential cases will establish fidelity for movement, doors, combat, AI, transitions, save/load and presentation?
6. How will animation/event time and physics time remain consistent under frame-rate variation and visibility culling?
7. What platform and performance targets will be measured first, and how will the later SDL_GPU and OpenXR boundaries be kept explicit?
8. How will unsupported file variants, discarded/recovered properties and unavailable services be reported so they cannot silently resemble success?

The main technical conclusion is that the compiler-porting barrier is already partly solved in OpenBlood2. The remaining critical work is semantic integration: faithfully implementing and connecting the engine services that allow the preserved game code to execute.
