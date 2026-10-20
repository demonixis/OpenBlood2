# Workspace and Source Trees

Snapshot date: 15 October 2026. The six repositories are separate checkouts, not subdirectories of a common Git repository. Their identifiers below anchor the inventory to a reproducible source state. Exact hashes and file counts are available in [workspace.json](Inventory/workspace.json).

## Repository Baseline

| Repository | Commit | Commit date | Role |
| --- | --- | --- | --- |
| [OpenBlood2](../../../OpenBlood2) | `195dc9d9c100` | 2026-01-14 | Adapted original Blood 2 code and replacement C++ engine |
| [NoMadEngine](../../../NoMadEngine) | `6d9107368e3b` | 2026-01-22 | C++ engine and separate Blood 2 and Shogo implementations |
| [nomad](../../../nomad) | `58fc2e788856` | 2026-05-23 | Godot C# prototype derived from NoMadEngine work |
| [sources/blood2](../../../sources/blood2) | `006b4aabd3fb` | 2014-10-27 | Reference Blood 2 game code and engine API headers |
| [sources/Shogo_Aegis](../../../sources/Shogo_Aegis) | `e80c1a7ad0fa` | 2025-04-27 | Modified Shogo game source and separate widescreen changes |
| [sources/lithtech](../../../sources/lithtech) | `0eab18289bed` | 2012-08-27 | Larger engine tree, SDK, tools, libraries, and multiple games |

Commit dates describe these checkouts, not the original release dates of the games or engines. All six working trees were clean at inventory time. There is no shared build entry point at the workspace root.

## Blood 2 Reference Code

The [source release README](../../../sources/blood2/Readme.txt) describes game modules built against the engine API. Its `AppHeaders/` directory contains interface declarations; this source release alone does not provide the complete original engine implementation.

| Directory | Code files | Responsibility and useful entry points |
| --- | ---: | --- |
| [AppHeaders](../../../sources/blood2/AppHeaders) | 31 | Engine interfaces, object/class registration, messages, physics and common types; `server_de.h`, `client_de.h`, `serverobj_de.h`, `generic_msg_de.h` |
| [ClientShellDLL](../../../sources/blood2/ClientShellDLL) | 197 | Client shell, movement, camera, menus, HUD, view weapons, effects and session handling; `BloodClientShell.cpp` |
| [ObjectDLL](../../../sources/blood2/ObjectDLL) | 298 | Server shell, player, AI, weapons, inventory, triggers, doors and world objects; `BloodServerShell.cpp` |
| [Shared](../../../sources/blood2/Shared) | 15 | Weapon definitions, commands, client/server IDs, shared movement and network definitions |
| [Misc](../../../sources/blood2/Misc) | 42 | GUI helpers, containers, utility code, server interface and legacy support headers |
| [ClientRes](../../../sources/blood2/ClientRes) | 1 | Resource script and localization material; associated project and text files are counted separately |
| [Blood2Serv](../../../sources/blood2/Blood2Serv) | 22 | Standalone server application and hosting support |

Four Visual C++ `.dsp` projects are tracked: client shell, object module, client resources, and standalone server. The source README describes `CSHELL.DLL`, `CRES.DLL`, and `OBJECT.LTO` outputs. This is a historical build description, not a modern build recipe.

The tree also contains expansion conditionals such as `_ADDON` and `_ADD_ON`. Their presence is recorded; expansion behavior and build coverage are not established here.

## OpenBlood2

The root [CMake file](../../../OpenBlood2/CMakeLists.txt) connects the replacement engine, the adapted original game code, the application, and conversion tools.

| Area | Responsibility |
| --- | --- |
| [OpenLithTech/include/lithtech](../../../OpenBlood2/OpenLithTech/include/lithtech) | Public types, engine API, base class and direct game interface |
| [OpenLithTech/src/core](../../../OpenBlood2/OpenLithTech/src/core) | Engine lifecycle, classes, objects, properties, messages, levels, strings and animation |
| [OpenLithTech/src/api](../../../OpenBlood2/OpenLithTech/src/api) | `ServerDE` and `ClientDE` implementations and compatibility tables |
| [OpenLithTech/src/formats](../../../OpenBlood2/OpenLithTech/src/formats) | REZ, DAT, ABC, DTX, PCX, FNT and CFG readers |
| [OpenLithTech/src/renderer](../../../OpenBlood2/OpenLithTech/src/renderer) | OpenGL rendering, resources, lights, fonts, sprites and post-processing |
| [OpenLithTech/src/physics](../../../OpenBlood2/OpenLithTech/src/physics), [audio](../../../OpenBlood2/OpenLithTech/src/audio), [xr](../../../OpenBlood2/OpenLithTech/src/xr) | Collision and physics, SDL audio, optional OpenXR integration |
| [blood2](../../../OpenBlood2/blood2) | Adapted game source, compatibility header, added stubs and legacy support files |
| [game](../../../OpenBlood2/game) | Application entry, class registration, shell creation, event processing and game loop |
| [OpenLithTech/tests](../../../OpenBlood2/OpenLithTech/tests) | Five C++ test programs |
| [tools](../../../OpenBlood2/tools), [docs](../../../OpenBlood2/docs), [shaders](../../../OpenBlood2/shaders) | Conversion utilities, technical material and shader programs |

`Blood2` and `OpenLithTech` are declared as static libraries. The application calls `ObjectDLLSetup`, `CreateServerShell`, and `CreateClientShell` directly in [Blood2Game.cpp](../../../OpenBlood2/game/Blood2Game.cpp). The direct [game interface](../../../OpenBlood2/OpenLithTech/src/core/game_interface.cpp) coexists with API compatibility code and stubs. File presence therefore should not be read as full implementation of the old API.

`game_shell_manager.cpp` and `module_loader.cpp` remain in the tree but are commented out of the engine's source list. Other source files are filtered from the game library by its [CMake file](../../../OpenBlood2/blood2/CMakeLists.txt). A file inventory and the set of compiled files are different things.

## NoMadEngine

NoMadEngine supplies a common engine, two game applications, an editor and a resource exporter. Its application named `OpenBlood2` is distinct from the separate `OpenBlood2/` repository.

| Area | Responsibility |
| --- | --- |
| [nomadengine/src/core](../../../NoMadEngine/nomadengine/src/core) | Common types and ECS components, level management, properties, animation and strings |
| [nomadengine/src/game](../../../NoMadEngine/nomadengine/src/game) | `GameBase`, game state, configuration and command-line support |
| [nomadengine/src/systems](../../../NoMadEngine/nomadengine/src/systems) | Doors, triggers, keyframers, sound, physics, weapons and HUD |
| [nomadengine/src/formats](../../../NoMadEngine/nomadengine/src/formats) | Binary readers and game resource parsers |
| [nomadengine/src/renderer](../../../NoMadEngine/nomadengine/src/renderer) | OpenGL rendering, resources, shading and post-processing |
| [nomadengine/src/physics](../../../NoMadEngine/nomadengine/src/physics) | Jolt world integration, collision shapes and debug rendering |
| [nomadengine/src/input](../../../NoMadEngine/nomadengine/src/input), [audio](../../../NoMadEngine/nomadengine/src/audio), [xr](../../../NoMadEngine/nomadengine/src/xr), [ui](../../../NoMadEngine/nomadengine/src/ui) | Input actions, audio, OpenXR, menus and debug/editor UI |
| [openblood2/src](../../../NoMadEngine/openblood2/src) | Blood 2 application, entity factory, game components and HUD |
| [openshogo/src](../../../NoMadEngine/openshogo/src) | Shogo application, entity factory and game components |
| [nomadeditor/src](../../../NoMadEngine/nomadeditor/src) | Editor application, scene model, selection, transform gizmo and browsing panels |

The current source lists build these implementations rather than the original `ClientShellDLL` and `ObjectDLL`. The entity factory maps game class names to the prototype's components. The historical [migration document](../../../NoMadEngine/docs/FEATURE_MIGRATION.md) explicitly describes reconstruction from assets and data.

## Godot Prototype

[nomad/NoMAD.csproj](../../../nomad/NoMAD.csproj) declares Godot.NET.Sdk 4.6.2 and .NET 8. The project has 98 C# files, including its test program and editor tools.

| Area | C# files | Responsibility |
| --- | ---: | --- |
| [src/Nomad.Core](../../../nomad/src/Nomad.Core) | 6 | Game profiles, discovery, configuration paths and coordinate conversion |
| [src/Nomad.Formats](../../../nomad/src/Nomad.Formats) | 14 | Binary reading, resource formats, world semantics and geometry queries |
| [src/Nomad.Gameplay](../../../nomad/src/Nomad.Gameplay) | 22 | Shared combat/messages/effects, and separate Blood 2 and Shogo gameplay definitions |
| [src/Nomad.Runtime](../../../nomad/src/Nomad.Runtime) | 32 | Archives, level composition, materials, models, animation, runtime objects and settings |
| [src/Nomad.Import](../../../nomad/src/Nomad.Import) | 3 | Local import/cache policy and reference export helpers |
| [src/Games](../../../nomad/src/Games) | 2 | Blood 2 and Shogo profiles |
| [scripts](../../../nomad/scripts) | 14 | Startup, menus, loading, player controllers, viewers and debug UI |
| [addons/nomad_editor](../../../nomad/addons/nomad_editor) | 4 | Dock, editor plugin, level loader and scene generation |
| [tests/Nomad.Tests](../../../nomad/tests/Nomad.Tests) | 1 | Console test harness with 64 registered checks |

Eleven tracked `.tscn` files provide the common menu/game flow, developer scenes, and game scenes. The entry scene is `scenes/common/nomad_startup.tscn`. Local generated scenes and game data are governed by [nomad/AGENTS.md](../../../nomad/AGENTS.md) and are outside the asset inventory.

## Other Reference Trees

[Shogo_Aegis/Readmetoo.txt](../../../sources/Shogo_Aegis/Readmetoo.txt) identifies an unfinished modified version of Shogo. Its `ClientShellDLL`, `ObjectDLL`, `Shared`, resource modules and server application follow the older game-module layout. `widescreen/` contains six additional source files. The [changelog](../../../sources/Shogo_Aegis/changelog.txt) distinguishes Aegis gameplay changes from the original game's behavior.

The [LithTech README](../../../sources/lithtech/README.md) describes a Jupiter Ex origin and work on porting games to that engine. Its main areas are:

| Area | Inventory role |
| --- | --- |
| [Blood2](../../../sources/lithtech/Blood2) | Duplicate reference game source text, as detailed below |
| [Shogo](../../../sources/lithtech/Shogo) | Separate Shogo source tree; not assumed equivalent to Aegis |
| [NOLF](../../../sources/lithtech/NOLF), [NOLF2](../../../sources/lithtech/NOLF2), [FEAR](../../../sources/lithtech/FEAR) | Other game code; NOLF2 includes TO2 and TRON build branches |
| [runtime](../../../sources/lithtech/runtime) | Client/server, world, models, physics, rendering, sound, UI and platform/kernel services |
| [sdk/inc](../../../sources/lithtech/sdk/inc) | Later `ILT*` interfaces, object/message types and SDK helpers |
| [libs](../../../sources/lithtech/libs) | Resource, memory, UI, configuration, network/service and third-party libraries |
| [tools](../../../sources/lithtech/tools) | Editors, resource tools, preprocessors, converters and shared tool code |

Every indexed runtime, library and tool module has a separate row in [counts.md](Inventory/counts.md). The presence of these systems does not establish compatibility with Blood 2's older `ServerDE` and `ClientDE` contracts.

## Overlapping Source Copies

The following comparisons match code files by exact relative path. CRLF normalization changes only line endings; names are case-sensitive and renames are not detected. Full results and unmatched paths are in [source_copies.json](Inventory/source_copies.json).

| Trees | Matching paths | Identical raw bytes | Identical after CRLF normalization |
| --- | ---: | ---: | ---: |
| `sources/blood2/` and `sources/lithtech/Blood2/` | 606 | 0 | 606 |
| `sources/blood2/` and `OpenBlood2/blood2/` | 602 | 0 | 375 |
| `OpenBlood2/tools/` and `NoMadEngine/tools/` | 20 | 13 | 13 |

The two reference Blood 2 code copies are text-equivalent under this normalization. OpenBlood2 has 227 matching files whose text differs after normalization, plus 17 paths found only on its side and four found only in the reference. Those unmatched sets include filename case changes and `engineobjects.c` becoming `engineobjects.cpp`; they are not all additions or deletions of functionality. Interpreting the behavioral impact of these changes is deferred to phase 2.
