# Developer documentation

Start here when building or changing OpenBlood2. M2 is still in progress; working
features and test results do not imply that the complete game is supported.

| Guide | Contents |
| --- | --- |
| [Getting started](getting_started.md) | Build, external game data, controls, platforms and test commands |
| [Architecture](architecture.md) | Code layout, target responsibilities and service boundaries |
| [Contributing](contributing.md) | Original-source workflow, conventions and verification |
| [Service contracts](contracts/readme.md) | Subsystem rules, ownership and explicit limitations |
| [Implementation records](implementation/readme.md) | Import provenance, source ledgers, progress reports and measured results |

Keep this directory focused on developer guides. Put subsystem reference material
in `contracts/` and work-in-progress records in `implementation/`. Historical
workspace reports and raw evidence have their own dedicated directory, linked
from the implementation index.

- [Music selection contracts](contracts/music_selection_contracts.md): current section chord/mute and original pattern decisions; live sequencing remains incomplete.
