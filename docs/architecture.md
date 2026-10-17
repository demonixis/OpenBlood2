# Architecture

OpenBlood2 integrates selected original Blood II game code into a new C++20
runtime. Original game rules and replacement engine services are documented
separately. Earlier prototypes are references, not build dependencies.

## Code layout

The targets are defined in [CMakeLists.txt](../CMakeLists.txt).

| Target / directory | Responsibility |
| --- | --- |
| `engine` / [src/engine](../src/engine) | Resources and format readers, object storage, collision, animation, audio and SDL_GPU rendering |
| `blood2_game` / [src/game](../src/game) | Session ownership, typed messages and engine service adapters |
| [src/game/original](../src/game/original) | Selected original shells, objects, AI, weapons and effects with documented adaptations |
| [src/game/legacy](../src/game/legacy) | Bounded source-level interface declarations; no retail DLL ABI |
| `blood2_app` / [src/app](../src/app) | SDL input and application loop, scene preparation, HUD, inspection and smoke scenarios |
| [tests](../tests) | Synthetic behavior, private-content integration and optional GPU checks |
| [cmake](../cmake) | Pinned dependencies, generated build information and bundled shader tooling |

`blood2_app` produces the `blood2` executable and links `blood2_game`, which links
`engine`. The engine shares the small legacy type declarations; these directories
are not independent replacements for the full historical SDK.

## Boundaries to preserve

[GameSession](../src/game/session.h) owns the local world/session and coordinates
original shell updates through [session.cpp](../src/game/session.cpp).
[ServerAdapter](../src/game/server_adapter.h) implements the supported original
service interface. Unsupported required services must remain explicit failures.

[Objects](../src/engine/objects.h) owns original instances, stable identities,
scheduled callbacks and deferred destruction. Do not free objects inside their
callbacks or give the player a second physics integration. Read the
[object](contracts/object_contracts.md), [movement](contracts/movement_contracts.md)
and [level-change](contracts/level_change_contracts.md) contracts before changing
these paths.

Rendering and inspection consume checked runtime state; drawing must not advance
gameplay or animation clocks. Keep device/input concerns separate from original
game rules. Model preparation currently uses CPU skinning with cached local
geometry; see [model preparation](contracts/model_preparation_contracts.md).

Use plain objects, containers and explicit functions. Dependency pins are in
[Dependencies.cmake](../cmake/Dependencies.cmake); adding a framework or dependency
requires a concrete need. External game data stays outside the repository.
