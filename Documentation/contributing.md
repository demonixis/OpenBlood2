# Contributing

Read [AGENTS.md](../AGENTS.md), [architecture](architecture.md) and the relevant
[contracts](contracts/readme.md).

1. Check the original Blood II caller and engine interface. Preserve reference sources and commercial assets.
2. Reuse original rules; identify portability fixes and replacement engine policies separately.
3. Use C++/Rust, the pinned stack, explicit returns and simple ownership. Follow `.clang-format`/`.editorconfig`; owned C++ namespaces use `OpenBlood2`.
4. Expose state and failures in the inspector, including hidden/inactive objects and missing resources.
5. Run relevant [checks](testing.md). Behavior needs observable assertions; lifetime changes need sanitizers; application/rendering changes also need Release and GPU checks.
6. Record results/limits in English and commit completed work in each affected repository. Compilation alone does not accept a milestone.

## Documentation

| Content | Location |
| --- | --- |
| Build, controls, diagnostics and architecture | This directory |
| Milestones and future features | [roadmap.md](roadmap.md) only |
| Subsystem behavior, ownership and limits | [contracts/](contracts/readme.md) |
| Imports and adaptations | [implementation/provenance.md](implementation/provenance.md) |
| Source hashes / level gaps | [implementation/sources/](implementation/sources/readme.md) / [level_gaps.md](implementation/level_gaps.md) |
| Results and raw evidence | [implementation/reports/](implementation/reports) / [evidence/](implementation/reports/evidence) |

Write short instructions: command first, explanation only when needed. Keep one
maintained description of each topic and link to it. Preserve raw logs, patches
and hash snapshots; update maintained links after moves. Historical reports keep
their dated scope. Filenames are lowercase except repository entrypoints.

AI-assisted changes require the same source review and verification. Imported
code keeps its notices; see [LICENSE](../LICENSE) and [provenance](implementation/provenance.md).
