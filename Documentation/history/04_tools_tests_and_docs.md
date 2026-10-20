# Tools Tests and Existing Documentation

This catalog locates development aids already present in the workspace. It records their source and declared purpose without treating them as validated tooling or an accepted implementation plan.

## Prototype Tools

| Tool | Locations | Declared purpose and dependencies |
| --- | --- | --- |
| RezExporter | [OpenBlood2](../../../OpenBlood2/tools/RezExporter), [NoMadEngine](../../../NoMadEngine/tools/RezExporter) | C++ command-line resource exporter; ABC to GLB, texture to PNG, optional raw extraction. Links the corresponding engine, GLM, JSON and a checked-in stb image writer |
| Python format utilities | [OpenBlood2 Research](../../../OpenBlood2/tools/Research), [NoMadEngine Research](../../../NoMadEngine/tools/Research) | `convert_abc.py`, `convert_dat.py`, `convert_dtx.py`, `convert_pcx.py`, `convert_fnt.py`, `convert_sty.py`, `convert_smk.py`, plus inspection/cleanup scripts |
| Image conversion dependencies | `convert_dtx.py`, `convert_pcx.py` in the Research directories | Import Pillow through `PIL`; no version is pinned by these imports |
| Media conversion dependencies | `convert_smk.py`, `convert_sty.py` in the Research directories | Invoke or look for FFmpeg; invocation paths were identified, not run |
| Source packaging | [OpenBlood2 srcpack.py](../../../OpenBlood2/tools/srcpack.py), [NoMadEngine srcpack.py](../../../NoMadEngine/tools/srcpack.py) | Source packaging utility present in both trees |
| NomadEditor | [NoMadEngine/nomadeditor](../../../NoMadEngine/nomadeditor) | Separate C++ editor target with viewport, object/asset browsers, property panel, selection and gizmos |
| Godot editor plugin | [nomad/addons/nomad_editor](../../../nomad/addons/nomad_editor) | NoMAD dock, editor level loader, generated runtime scenes and an asset-inspector entry |
| Godot developer scenes | [nomad/scenes/dev](../../../nomad/scenes/dev), [nomad/scripts](../../../nomad/scripts) | Level viewer, asset inspector, fly camera and weapon-transform debugging |
| Local import helpers | [nomad/src/Nomad.Import](../../../nomad/src/Nomad.Import) | Cache/import policy and reference export; generated game-derived output stays local under the project's rules |

The two C++ tool trees have 20 matching code paths, 13 with identical bytes. The [copy comparison](Inventory/source_copies.json) records overlap without assessing which implementation should be retained.

The RezExporter README still uses an OpenBlood2-oriented build example in the NoMadEngine copy. The current CMake target, rather than that example, identifies the engine dependency in each repository.

## Historical LithTech Tools

The [larger source tree](../../../sources/lithtech/tools) contains tool families beyond the prototypes:

| Family | Directories |
| --- | --- |
| Level editing and preprocessing | `DEdit`, `PreProcessor`, `PropHelpGen` |
| Model editing and packing | `ModelEdit`, `Model_Packer`, `ltbhead`, `ltaview` |
| Texture inspection and conversion | `DtxOptimizer`, `DtxShellEx`, `DtxView`, `dtxutil`, `HeightMaptoNormalMap`, `FilterTest` |
| Effects and rendering styles | `FxED`, `RenderStyle_Packer`, `RenderStylesEditor` |
| Resource packaging | `LithRez`, `winpacker` |
| Audio, animation and configuration helpers | `WaveEdit`, `lipcompiler`, `ButeEdit`, `TSCompiler` |
| Other utilities and shared code | `LTC`, `WinLTC`, `ltdmtest`, `pma`, `recurse_run`, `shared`, `utils`, `vcvars` |

Their files and sizes are included in the module counts and file index. Tool behavior, buildability and compatibility with Blood 2 formats have not been reviewed in depth. Game-specific tools under NOLF2 and other game trees are also indexed, without assuming they are needed for this project.

## Existing Test Entry Points

| Project | Test material | Build connection and limits |
| --- | --- | --- |
| OpenBlood2 | Five `.cpp` programs in [OpenLithTech/tests](../../../OpenBlood2/OpenLithTech/tests): classes, messages, engine messages, properties, and level loading | Each has a `main` entry. The engine declares `BUILD_TESTS`, but no `add_test`, `enable_testing`, or test executable wiring was found in the tracked CMake files |
| NoMadEngine | No dedicated test target found in the inspected project CMake files | This is not proof that every utility or embedded library lacks test code; the inventory makes no coverage claim |
| nomad | [Program.cs](../../../nomad/tests/Nomad.Tests/Program.cs), 64 registered checks | Standalone .NET 8 console harness. Tests core/format/import/gameplay code and selected runtime helpers, not the complete Godot application |
| Reference trees | Historical utilities and test-like files are included in the source catalog | No coherent modern Blood 2 regression suite was established from this inventory |

The Godot prototype harness includes synthetic checks for binary reading, coordinate conversion, REZ/ABC/DTX/PCX handling, animation, doors, triggers, weapons, AI, settings and content discovery. It also registers five local-data checks for archives, DAT worlds, scene semantics, textures and models. Those local-data functions can skip their work when installations or suitable resources are unavailable; a successful process exit alone would not establish that game data was tested.

The project documents these commands:

```sh
cd nomad
dotnet build NoMAD.csproj
dotnet run --project tests/Nomad.Tests/Nomad.Tests.csproj
```

They are recorded for the audit stage and were not run during this inventory. No code coverage, successful platform build, visual validation or campaign progression result is claimed.

## Documentation Map

The generated [documentation index](Inventory/documentation.md) lists all 172 indexed documentation files, including historical text files and source notices. Root workspace instructions and the new inventory documents are outside that generated index.

| Subject | Starting points | How to interpret them |
| --- | --- | --- |
| Current project direction | Root [README](../../../README.md) and [AGENTS](../../../AGENTS.md) | Current intent: Blood 2 first, preserve behavior through original source reuse |
| Godot local rules | [nomad/AGENTS.md](../../../nomad/AGENTS.md) | Applicable instructions for that subtree, including local data/cache and runtime code practices |
| Original source layout | [Blood 2 Readme](../../../sources/blood2/Readme.txt), [Shogo Readme](../../../sources/Shogo_Aegis/Readme.txt) | Historical module and toolchain descriptions |
| Modified source provenance | [Aegis Readmetoo](../../../sources/Shogo_Aegis/Readmetoo.txt), [changelog](../../../sources/Shogo_Aegis/changelog.txt), [LithTech README](../../../sources/lithtech/README.md) | Establish which tree is being discussed; not compatibility guarantees |
| Engine API | [OpenBlood2/docs/api](../../../OpenBlood2/docs/api) | Existing API extraction, analysis and implementation guidance; historical call counts were not recalculated |
| Binary formats | [OpenBlood2 formats](../../../OpenBlood2/docs/formats), [NoMadEngine formats](../../../NoMadEngine/docs/formats) | DAT, ABC and DTX descriptions to cross-check against current readers |
| Animation | [OpenBlood2 animation](../../../OpenBlood2/docs/animation_system.md), [NoMadEngine animation](../../../NoMadEngine/docs/animation_system.md) | Existing descriptions of transforms, hierarchy and skinning |
| Game and engine design | Both `docs/design/` trees | Game-design notes, technical designs and appendices; proposals may describe more than the code implements |
| Gameplay and progression | `GAMEPLAY_RULES.md`, `AI_SYSTEM.md`, `LEVEL_SYSTEM.md`, `CAMPAIGN_FLOW_SCHEMA.md`, `INPUT_MAPPING.md`, `UI_SYSTEM.md` under both `docs/` trees | Reference material, to validate against source and observed behavior |
| Historical planning | [OpenBlood2 ROADMAP](../../../OpenBlood2/ROADMAP.md), [reboot plan](../../../OpenBlood2/docs/REBOOT_PLAN.md), [NoMadEngine migration plan](../../../NoMadEngine/docs/FEATURE_MIGRATION.md), [priorities](../../../NoMadEngine/docs/PRIORITIES.md), [TODO](../../../NoMadEngine/TODO_Next.md) | Historical context; not the new roadmap |

Several old documents propose abandoning the original code or use earlier directory names such as `OpenLithTech` inside the NoMadEngine documentation. The inventory preserves their locations without adopting those plans or silently correcting the source documents.

Existing license/EULA material is included in the index, including `nomad/LICENSE.md` and source release EULAs. Repository README license statements are not treated as a license for the entire workspace.
