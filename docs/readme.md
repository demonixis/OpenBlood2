# Developer documentation

Start here when building or changing OpenBlood2. [M2 is closed with reservation
M2-GPU-001](contracts/m2_acceptance_contracts.md): the selected original session is
accepted, while NVIDIA/D-Bus GPU sanitizer qualification remains open. M3 is next.
The complete game and later milestones are not yet supported.

| Guide | Contents |
| --- | --- |
| [Getting started](getting_started.md) | Build, external game data, controls, platforms and test commands |
| [CI builds](ci_builds.md) | GitHub Actions, desktop artifacts, headless tests and dual-remote workflow |
| [Diagnostic logs](logging.md) | File locations, rotation, interrupted sessions and crash-analysis limits |
| [Crash capture](crash_capture.md) | Linux all-thread stacks, ELF cores, matching binaries and failure limits |
| [Architecture](architecture.md) | Code layout, target responsibilities and service boundaries |
| [Desktop launcher](launcher.md) | Compact menu, game folder, demo installation, REZ mods and validation |
| [Launcher plan](../../Documentation/Roadmap/06_launcher.md) | Launcher roadmap and revised scope; Nightmares excluded |
| [Contributing](contributing.md) | Original-source workflow, conventions and verification |
| [Service contracts](contracts/readme.md) | Subsystem rules, ownership and explicit limitations |
| [Implementation records](implementation/readme.md) | Import provenance, source ledgers, progress reports and measured results |

Keep this directory focused on developer guides. Put subsystem reference material
in `contracts/` and work-in-progress records in `implementation/`. Historical
workspace reports and raw evidence have their own dedicated directory, linked
from the implementation index.

- [Music selection contracts](contracts/music_selection_contracts.md): current section chord/mute and original pattern decisions; live sequencing remains incomplete.
