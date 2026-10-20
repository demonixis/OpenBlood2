# Blood 2 Technical Audit

**Phase 2 — 15 October 2026.** This audit builds on the [phase 1 inventory](../01_workspace.md). It assesses original-code integration, build viability, engine contracts, formats, animation, prototype reuse, portability, and validation gaps. It does not select the final architecture or define the roadmap.

**Reusing the original game code remains technically credible.** The adapted OpenBlood2 game and engine compile and link with GCC 16 on Linux after a temporary dependency-path override. The more substantial obstacle is the incomplete and sometimes incorrect implementation of the engine services used by that code. Static linking has simplified packaging, but the current implementation still contains manually reconstructed interfaces and incompatible lifecycle behavior.

OpenBlood2 currently instantiates only `Light`, `DirLight`, `Door`, and `RotatingDoor` from level data. Its ability to display worlds and enemy models must not be interpreted as evidence that the original player, AI, combat, triggers, or campaign are running. NoMadEngine and the Godot prototype contain useful independent systems, but neither executes the original Blood 2 gameplay classes.

## Read the Audit

| Document | Purpose |
| --- | --- |
| [Builds and executed checks](01_build_verification.md) | Reproducible commands, successful builds, failures, diagnostic overrides, and test limitations |
| [Original game integration and engine contracts](02_original_code_and_contracts.md) | Active integration path, lifecycle defects, interface hazards, API gaps, and behavioral changes |
| [Formats, coordinates, and animation](03_formats_and_animation.md) | Resource-reader suitability, parser robustness, animation behavior, and preservation constraints |
| [Prototype reuse, portability, and performance](04_prototypes_and_portability.md) | What each prototype contributes, what its tests prove, and platform/performance constraints |
| [Findings, coverage, and unresolved questions](05_findings_and_coverage.md) | Consolidated findings with severity and evidence, scope boundaries, and inputs for the later roadmap |
| [Execution evidence](evidence/readme.md) | Complete logs, exact commands, exit codes, source integrity, and fetched dependency revisions |
| [Probe reproduction](probes/readme.md) | Synthetic probes and how to repeat them without changing project sources |

## Verified Outcomes

| Subject | Outcome | Limit |
| --- | --- | --- |
| OpenBlood2, Linux/GCC 16 | Game and engine built and linked; `--help` executed successfully | Required an ImGuizmo source-directory override; VR and tools disabled; no game session exercised |
| NoMadEngine, Linux/GCC 16 | Blood 2 and Shogo executables built with VR enabled | Same ImGuizmo override; `ENABLE_VR=OFF` fails to compile; editor/tools disabled; no game or headset session exercised |
| Godot/C# project | `NoMAD.csproj` and standalone test project built with zero reported warnings/errors | No Godot executable was available for scene/runtime validation |
| C# test program | 59 non-asset checks completed successfully in a controlled synthetic root | Five asset-dependent entries printed `PASS` after skipping their work; .NET 10 major roll-forward used because .NET 8 runtime was absent |
| OpenLithTech object creation | Precreate position changes were lost; world creation received normal initial-update reason | Synthetic callback using the unchanged production engine core |
| OpenLithTech scheduling | A requested 0.01-second delay became 1 second | Reproduced with the original-style callback return value of `1` |
| OpenLithTech self-removal | AddressSanitizer detected a heap use-after-free in `Engine::Update` | Isolated production-core probe, not a full-game sanitizer run |
| Adapted aggregate dispatch | One object message re-entered its callback eight times | Probe deliberately stops at eight; unbounded production path is recursive |
| C++ ABC parsing | Self-referencing section chain did not terminate within three seconds | Synthetic 12-byte file; timeout and source control flow support the finding |

## How to Read the Conclusions

**Executed** means a recorded build, command, or synthetic probe was run. **Source-confirmed** means the relevant implementation and its connection were inspected. **Inference** identifies a likely implication requiring further validation. **Unverified** means no supporting execution evidence was obtained.

There is no percentage of game completeness in this audit. Counts of source files, registered classes, API names, successful compilation units, and passing synthetic checks cannot establish playable or faithful behavior.

The audit changed only files under `Documentation/`. All six repository commits still match the inventory, all six working trees are clean, and all **10,923 indexed text-file hashes** still match. Build products and fetched dependencies reside in a temporary directory; Godot's SDK also emitted ignored products under `nomad/.godot/`. No commercial archive was parsed for the audit, and no extracted game content or derivatives are included in these reports. See [snapshot.json](evidence/snapshot.json).
