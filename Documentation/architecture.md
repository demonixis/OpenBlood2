# Architecture

Selected original Blood II game code runs in a C++20 engine. Prototypes are
references, not build dependencies. [CMakeLists.txt](../CMakeLists.txt) defines:

| Target / directory | Responsibility |
| --- | --- |
| `blood2_startup` | Config/REZ readers. |
| `engine_core` / [src/engine](../src/engine) | CPU resources, objects, collision, animation, audio and presentation preparation. |
| `engine` | Client-only SDL_GPU renderers, runtime, capture and optional XR. |
| `blood2_game` / [src/game](../src/game) | Session ownership, typed messages and service adapters. |
| `blood2_frontend` | Client-only original menus, loading screen and preferences. |
| `blood2_net` / [src/net](../src/net) | Bounded ENet transport and full mounted-content fingerprint. |
| `blood2_server` / [src/server](../src/server) | Zero-client dedicated simulation foundation; produces `blood2-server`. Admission is pending. |
| [src/game/original](../src/game/original) | Selective original shells, objects, AI, weapons and effects. |
| [src/game/legacy](../src/game/legacy) | Source interface declarations; no retail DLL ABI. |
| `blood2_app` / [src/app](../src/app) | Input, application loop, presentation, HUD and inspector. Produces `blood2`. |
| [tests](../tests) / [cmake](../cmake) | Verification / pinned dependencies and build tooling. |

`blood2_app` → `blood2_frontend` / `engine` → `blood2_game` / `engine_core` → `blood2_startup`.
`blood2_server` → `blood2_game` / `blood2_net` → `engine_core` / `blood2_startup`.
The [M9 ownership separation](contracts/multiplayer_contracts.md) is in progress:
`ServerSession` currently wraps shared `GameSession` storage and advances only
server services. Client shells and CPU FX still need extraction; this build split
does not establish a replica or a network gameplay session.
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
