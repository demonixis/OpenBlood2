# Inventory Coverage and Audit Handoff

The inventory covers the entire set of tracked files in the six repositories at the commits recorded in [workspace.json](Inventory/workspace.json). It provides structural coverage across the workspace, with targeted reading of build files, entry points, interfaces, and representative systems. It is not a line-by-line review of all source code.

## Coverage Levels

| Level | Meaning | Completed scope |
| --- | --- | --- |
| Enumerated | Path and extension obtained from Git | All 11,455 tracked files |
| Indexed | Text bytes counted and hashed, classified by path/extension | 10,923 files: 10,588 code, 127 build, 172 documentation, 32 configuration, 4 archived text copies |
| Located | Filenames, declarations or search matches connect an area to a system | All main project/module families in the authored maps; deeper entries in the generated index |
| Read | Selected file contents or implementation excerpts inspected | Evidence register below; no claim that unlisted implementation bodies were reviewed |
| Executed | Code built, tests run or application behavior exercised | None of the game projects or existing tools |

Hashes and physical line counts require reading file bytes automatically. They do not imply semantic inspection. The generated documentation titles likewise do not imply that every document's body was reviewed.

## Scope and Exclusions

The input is `git ls-files -z` in each repository. The script reads current working-tree bytes; exact HEAD commits and working-tree change counts are recorded alongside them. All six working trees were clean, with no missing tracked files and no nonignored untracked files at the snapshot.

The catalog intentionally excludes detailed paths and contents for game assets/local data, compiled binaries and editor metadata. These account for the other **532 tracked files**: 121 classified as asset/local data, 100 as editor metadata, and 311 as other/binary. No REZ archive was opened. Ignored installations, caches, build outputs, Git internals and symlink contents are outside the scan. The script does not follow symlinks.

The code category includes C/C++ headers and implementation files, C#, Python, shaders, resource scripts, lexer/parser inputs and Bison skeletons. It includes third-party and generated source. It does not establish that each file is compiled. Recognized backup source/project files are indexed separately as archived text.

The root `README.md`, root `AGENTS.md`, and this `Documentation/` directory are not part of a root Git repository and are outside generated repository counts. Only `Documentation/` was created or modified for this phase.

## Evidence Register

This register identifies the main files whose contents support the authored inventory. “Selected sections” includes relevant declarations and search results; it does not imply full implementation review. Related historical source descriptions already examined during the preceding workspace documentation work were reused where applicable.

| Area | Files | Reading extent |
| --- | --- | --- |
| Workspace rules | [Root AGENTS](../../../AGENTS.md), [root README](../../../README.md), [nomad AGENTS](../../../nomad/AGENTS.md) | Current direction and applicable instructions |
| OpenBlood2 build | [Root CMake](../../../OpenBlood2/CMakeLists.txt), [engine CMake](../../../OpenBlood2/OpenLithTech/CMakeLists.txt) | Full declarations: dependencies, source lists, options and targets |
| Original game integration | [Blood2 CMake](../../../OpenBlood2/blood2/CMakeLists.txt), [application CMake](../../../OpenBlood2/game/CMakeLists.txt) | Source filtering and static-link sections |
| Direct entry points | [Blood2Game.cpp](../../../OpenBlood2/game/Blood2Game.cpp), [game_interface.cpp](../../../OpenBlood2/OpenLithTech/src/core/game_interface.cpp) | Includes, class-wrapper setup, lifecycle/function locations and direct-interface excerpts |
| OpenBlood2 engine declarations | [engine.h](../../../OpenBlood2/OpenLithTech/src/core/engine.h), [level_manager.h](../../../OpenBlood2/OpenLithTech/src/core/level_manager.h), [dat_parser.h](../../../OpenBlood2/OpenLithTech/src/formats/dat_parser.h) | Public declarations and loading/object-management entry points |
| NoMadEngine build | [Root CMake](../../../NoMadEngine/CMakeLists.txt), [preset](../../../NoMadEngine/CMakePresets.json) | Dependency/options/SIMD sections and preset contents |
| NoMadEngine targets | [Engine](../../../NoMadEngine/nomadengine/CMakeLists.txt), [Blood 2](../../../NoMadEngine/openblood2/CMakeLists.txt), [Shogo](../../../NoMadEngine/openshogo/CMakeLists.txt), [editor](../../../NoMadEngine/nomadeditor/CMakeLists.txt) | Full target/source/dependency declarations |
| NoMadEngine gameplay connection | [Blood2Game.cpp](../../../NoMadEngine/openblood2/src/Blood2Game.cpp), [Blood2EntityFactory.cpp](../../../NoMadEngine/openblood2/src/Blood2EntityFactory.cpp), [GameBase.h](../../../NoMadEngine/nomadengine/src/game/GameBase.h) | Startup/factory excerpts and lifecycle declarations |
| NoMadEngine loaders | [level_manager.h](../../../NoMadEngine/nomadengine/src/core/level_manager.h), [abc_parser.h](../../../NoMadEngine/nomadengine/src/formats/abc_parser.h) | Loading and animation-parser declarations |
| Godot configuration | [NoMAD.csproj](../../../nomad/NoMAD.csproj), [project.godot](../../../nomad/project.godot), [test project](../../../nomad/tests/Nomad.Tests/Nomad.Tests.csproj) | Full declarations |
| Godot runtime path | [GameLoader](../../../nomad/scripts/GameLoader.cs), [LevelLoader](../../../nomad/src/Nomad.Runtime/LevelLoader.cs), [scene composer](../../../nomad/src/Nomad.Runtime/GodotRuntimeSceneComposer.cs) | Loader bodies and initial/game-specific composition sections |
| Godot gameplay and formats | [Blood 2 GameplayWorld](../../../nomad/src/Nomad.Gameplay/Blood2/GameplayWorld.cs), [DatParser](../../../nomad/src/Nomad.Formats/DatParser.cs) | Gameplay object classification; parser entry and first object-parsing section |
| Godot effects and editor | [effect presenter](../../../nomad/src/Nomad.Runtime/GodotLithEffectPresenter.cs), [editor plugin](../../../nomad/addons/nomad_editor/NomadEditorPlugin.cs) | Effect-handler search matches; plugin registration and generation entry points |
| Test inventory | [Godot test program](../../../nomad/tests/Nomad.Tests/Program.cs), [C++ tests](../../../OpenBlood2/OpenLithTech/tests) | 64 test registrations, selected test bodies/local-data skip paths; C++ main-entry searches and CMake wiring searches |
| Original Blood 2 interfaces | [server shell](../../../sources/blood2/ObjectDLL/BloodServerShell.cpp), [client shell](../../../sources/blood2/ClientShellDLL/BloodClientShell.cpp) | Opening includes, globals, registration and server-shell creation |
| Original object contracts | [PlayerObj.h](../../../sources/blood2/ObjectDLL/PlayerObj.h), [AI_Mgr.h](../../../sources/blood2/ObjectDLL/AI_Mgr.h), [Door.h](../../../sources/blood2/ObjectDLL/Door.h), [cpp_server_de.h](../../../sources/blood2/AppHeaders/cpp_server_de.h) | Lifecycle/serialization declarations and compatibility-header contents |
| Larger engine build | [Root CMake](../../../sources/lithtech/CMakeLists.txt), [client CMake](../../../sources/lithtech/runtime/client/CMakeLists.txt), runtime/library CMake files | Full root; selected client source/platform/link sections; target declaration searches |
| Source provenance | [Blood 2 Readme](../../../sources/blood2/Readme.txt), [Shogo Readme](../../../sources/Shogo_Aegis/Readme.txt), [Aegis Readmetoo](../../../sources/Shogo_Aegis/Readmetoo.txt), [changelog](../../../sources/Shogo_Aegis/changelog.txt), [LithTech README](../../../sources/lithtech/README.md) | Source descriptions and historical modifications |
| Tools and documentation | Both exporter CMake files, tool READMEs, Python import/invocation searches, format/API indexes and historical reboot/migration plans | Build declarations, documented purposes, dependencies and direction conflicts |

The large NOLF, NOLF2, FEAR, Shogo, runtime, library and tool trees are fully enumerated and indexed within the stated rules. Their implementation bodies are mostly **not read** in this phase. No percentage of semantic audit completion is assigned.

## Handoff to Phase 2

These are unanswered audit questions, not implementation milestones or architecture decisions:

1. Which original Blood 2 engine calls and object lifecycle contracts are required by the client and server code, and which are implemented, approximated or stubbed in OpenBlood2?
2. What behavior changed in the adapted Blood 2 files, after separating line endings, case changes and compiler/platform fixes from gameplay changes?
3. Which declared builds can be reproduced, with which resolved dependencies, compiler versions and platform configurations?
4. Which format and animation paths agree across the prototypes and the original engine references, particularly for transforms, units, model scale and world semantics?
5. Which gameplay systems are connected to an executable runtime path, and which are only definitions, placeholders, isolated code or historical plans?
6. What do the existing tests actually exercise, including skipped local-data paths and Godot functionality outside the console harness?
7. Which later-generation LithTech implementations match the earlier Blood 2 contracts, and where do formats or engine semantics differ?

The inventory does not rank the prototypes, select a new implementation directory, recommend replacing systems, estimate project duration, or define a playable milestone. Those decisions remain for the requested audit and subsequent roadmap.

## Inventory Verification

The generated files were checked against the Git-tracked file sets and their working-tree bytes. Category totals reconcile with the repository totals. Local Markdown links and explicitly listed source paths were checked for existence. Repository commits and clean working-tree state were checked again after writing the documentation. No source repository was modified.
