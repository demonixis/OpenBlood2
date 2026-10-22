# Architecture

Selected original Blood II game code runs in a C++20 engine. Prototypes are
references, not build dependencies. [CMakeLists.txt](../CMakeLists.txt) defines:

| Target / directory | Responsibility |
| --- | --- |
| `blood2_startup` | Config/REZ readers. |
| `engine` / [src/engine](../src/engine) | Resources, objects, collision, animation, audio and SDL_GPU. |
| `blood2_game` / [src/game](../src/game) | Session ownership, typed messages and service adapters. |
| [src/game/original](../src/game/original) | Selective original shells, objects, AI, weapons and effects. |
| [src/game/legacy](../src/game/legacy) | Source interface declarations; no retail DLL ABI. |
| `blood2_app` / [src/app](../src/app) | Input, application loop, presentation, HUD and inspector. Produces `blood2`. |
| [tests](../tests) / [cmake](../cmake) | Verification / pinned dependencies and build tooling. |

`blood2_app` → `blood2_game` → `engine` → `blood2_startup`.
`blood2_app` also holds the [setup screen](contracts/setup_screen_contracts.md): game data
selection, mods and the demo download (miniz, and the platform's HTTPS stack: libcurl on
Linux/Windows, NSURLSession on Apple, HttpsURLConnection on Android). The former standalone
launcher is [archived](history/launcher/launcher.md).

## Ownership

- [GameSession](../src/game/session.h) owns world/session state and original shell updates.
- [ServerAdapter](../src/game/server_adapter.h) implements the supported game services; required unsupported calls fail explicitly.
- [Objects](../src/engine/objects.h) owns identities, callbacks and deferred destruction. Never destroy an object inside its callback.
- Player motion has one integration. Rendering/inspection read state without advancing simulation or animation clocks.
- Device input and camera presentation remain separate from original game rules.

Use plain objects, containers and explicit returns. Record imports and replacement
policies in [provenance](implementation/provenance.md). Read the relevant
[contracts](contracts/readme.md) before extending a service.
