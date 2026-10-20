# Systems and Entry Points

This map identifies source locations and connections visible in build files, declarations, and selected implementation excerpts. A listed system is present in the source tree; its completeness and fidelity have not been established. Paths in each table are relative to the linked directory named in its introduction.

## Original Blood 2 Gameplay

The following paths are under [sources/blood2](../../../sources/blood2). These are the starting points for understanding original game behavior before comparing prototype implementations.

| System | Source entry points | Responsibility visible in the source organization |
| --- | --- | --- |
| Client lifecycle | `ClientShellDLL/BloodClientShell.cpp`, `AppHeaders/clientshell_de.h` | Client initialization, input, updates, rendering coordination and engine callbacks |
| Server lifecycle | `ObjectDLL/BloodServerShell.cpp`, `AppHeaders/servershell_de.h` | Server shell creation, world/session state, client handling and level changes |
| Object lifecycle and properties | `AppHeaders/serverobj_de.h`, `ObjectDLL/B2BaseClass.h`, `ObjectDLL/WorldProperties.cpp` | Class registration, engine/object messages and level properties |
| Player and movement | `ObjectDLL/PlayerObj.cpp`, `ObjectDLL/BaseCharacter.cpp`, `ClientShellDLL/MoveMgr.cpp`, `Shared/SharedMovement.h` | Player state, character behavior and client movement |
| Camera and view weapons | `ClientShellDLL/PlayerCamera.cpp`, `ClientShellDLL/ViewWeapon.cpp`, `ClientShellDLL/GameWeapons.cpp` | Camera control, first-person weapon presentation and client weapon classes |
| Weapons and damage | `ObjectDLL/Weapon.cpp`, `ObjectDLL/GameWeapons.cpp`, `Shared/WeaponDefs.cpp` | Server weapon behavior and shared definitions |
| Inventory and pickups | `ObjectDLL/InventoryMgr.cpp`, `ObjectDLL/WeaponPickups.cpp` | Inventory and pickup objects |
| AI and paths | `ObjectDLL/AI_Mgr.cpp`, `ObjectDLL/PathMgr.cpp`, `ObjectDLL/CultistAI.cpp` | Shared AI behavior, path management and enemy-specific classes |
| Doors and moving objects | `ObjectDLL/Door.cpp`, `ObjectDLL/RotatingDoor.cpp`, `ObjectDLL/KeyFramer.cpp` | Door lifecycle and keyframed movement |
| Triggers and progression | `ObjectDLL/Trigger.cpp`, `ObjectDLL/ExitTrigger.cpp`, `ObjectDLL/ConversationTrigger.cpp`, `ObjectDLL/ObjectivesTrigger.cpp` | Object messages, exits, conversations and objectives |
| Menus and HUD | `ClientShellDLL/MainMenus.cpp`, `ClientShellDLL/Menu*.cpp`, `ClientShellDLL/StatusBar.cpp`, `ClientShellDLL/NewStatusBar.cpp` | Menu flow, settings and player display |
| Effects and sound | `ClientShellDLL/SFXMgr.cpp`, `ClientShellDLL/*FX.cpp`, `ObjectDLL/SoundFX.cpp`, client/server `Music.cpp` and `VoiceMgr.cpp` | Client visual effects, sound objects, music and voices |
| Save and load | `ClientShellDLL/LoadSave.cpp`, `ObjectDLL/PlayerObj.h`, `ObjectDLL/Door.h`, `ObjectDLL/AI_Mgr.h` | Client save flow and per-object serialization callbacks |
| Networking and hosting | `ClientShellDLL/NetStart.cpp`, `Shared/NetDefs.h`, `Blood2Serv/` | Session setup, shared network definitions and server application |

The original server shell exposes `CreateServerShell` and uses `DEFINE_CLASSES`. Object headers expose `EngineMessageFn`, `ObjectMessageFn`, `Save`, and `Load` hooks. These are concrete interfaces to trace during the audit; this inventory does not enumerate or validate every engine API call.

## Prototype Engine and Runtime Systems

Column paths are relative to [OpenBlood2/OpenLithTech/src](../../../OpenBlood2/OpenLithTech/src), [NoMadEngine/nomadengine/src](../../../NoMadEngine/nomadengine/src), and [nomad/src](../../../nomad/src), respectively.

| System | OpenBlood2 engine | NoMadEngine | Godot prototype |
| --- | --- | --- | --- |
| Lifecycle and orchestration | `core/engine.cpp`, application in `OpenBlood2/game/` | `game/GameBase.cpp`, `game/GameState.h` | `Nomad.Runtime/GodotRuntimeSceneComposer.cs`, scripts outside `src/` |
| Objects and classes | `core/class_manager.cpp`, `core/object.cpp`, `core/baseclass.cpp` | `core/components.*`, game entity factories outside the engine | `Nomad.Gameplay/LithObjectDefinition.cs`, per-game `GameplayWorld.cs` |
| Properties and messages | `core/property_manager.cpp`, `core/message_manager.cpp` | `core/property_manager.cpp`, components and system dispatch | `Nomad.Gameplay/LithMessageBus.cs`, `Nomad.Runtime/GodotLithMessageRuntime.cs` |
| World loading | `core/level_manager.cpp` | `core/level_manager.cpp` | `Nomad.Runtime/LevelLoader.cs`, `GodotLevelMeshBuilder.cs`, `GodotLevelSemanticBuilder.cs` |
| Models and animation | `formats/abc_parser.cpp`, `core/animation_controller.cpp` | `formats/abc_parser.cpp`, `core/animation_controller.cpp` | `Nomad.Formats/AbcParser.cs`, runtime mesh assembler, animation controller and model nodes |
| Rendering and materials | `renderer/renderer_3d.cpp`, `resource_manager.cpp`, post-process passes | `renderer/renderer_3d.cpp`, `shading/`, `postprocessing/` | `Nomad.Runtime/RuntimeMaterialFactory.cs`, mesh/model builders; rendering supplied by Godot |
| Collision and movement | `physics/collision_manager.cpp`, `physics_manager.cpp` | `physics/physics_world.cpp`, `systems/physics_system.cpp` | `Nomad.Formats/LithWorldQuery.cs`, Godot collision shapes and player controller scripts |
| Input | Application SDL handling and `OpenBlood2/game/sdl_to_vk.h` | `input/input_action_system.cpp` | Player/controller and camera scripts under `nomad/scripts/` |
| Audio | `audio/audio_manager.cpp` | `audio/audio_manager.cpp`, `systems/sound_system.cpp` | `Nomad.Gameplay/LithEffectIntent.cs`, `Nomad.Runtime/GodotLithEffectPresenter.cs`; actual playback remains to be traced |
| HUD and menus | Font/sprite renderer plus original client code and application UI | `ui/MainMenu.cpp`, `systems/hud_system.cpp`, game-specific HUD | `nomad/scripts/GameMenu.cs`, `CrosshairOverlay.cs`, player scripts and scenes |
| VR | `xr/` and application `RenderVR`/`RenderMirror` | `xr/`, `renderer/vr_ui_renderer.cpp`, `GameBase` VR routines | No dedicated OpenXR module identified in the indexed source tree |

For Godot animation, both [AbcAnimatedModelNode.cs](../../../nomad/src/Nomad.Runtime/AbcAnimatedModelNode.cs) and [AbcSkeletonModelNode.cs](../../../nomad/src/Nomad.Runtime/AbcSkeletonModelNode.cs) exist. Their coexistence is recorded without choosing a preferred path. The active route needs to be traced from the model builder for each use case.

## Resource Readers

The C++ reader names below occur in both engine format directories unless stated otherwise. Their presence does not establish support for every format revision or edge case.

| Resource | C++ reader or utility | C# location under `nomad/src/` |
| --- | --- | --- |
| REZ archives | `rez_archive_parser.*`, `rez_manager.*`, `rez_loader.*` | `Nomad.Formats/RezArchive.cs`, `Nomad.Runtime/GameArchiveSet.cs` |
| DAT world geometry and objects | `dat_parser.*`, `file_formats.*` | `Nomad.Formats/DatParser.cs`, `DatModels.cs`, `DatObjectExtensions.cs`, `DatWorldSemantics.cs` |
| ABC models and animations | `abc_parser.*` | `Nomad.Formats/AbcParser.cs`, `AbcModels.cs` |
| DTX textures | `dtx_parser.*` | `Nomad.Formats/DtxDecoder.cs` |
| PCX images | `pcx_parser.*` | `Nomad.Formats/PcxDecoder.cs` |
| Bitmap fonts | `fnt_parser.*`, font renderers | No standalone FNT reader identified in `Nomad.Formats` |
| Configuration | `cfg_parser.*` and JSON consumers | `Nomad.Formats/ButeDocument.cs`, runtime settings/material configuration |
| PNG images | `png_loader.*` in NoMadEngine; exporter helpers | Godot texture/runtime helpers |
| STY and SMK | Python utilities under both `tools/Research/` trees | No dedicated reader identified in `Nomad.Formats` |

The historical specifications live under [OpenBlood2/docs/formats](../../../OpenBlood2/docs/formats) and [NoMadEngine/docs/formats](../../../NoMadEngine/docs/formats). They include format-version claims and earlier implementation paths that must be checked during the audit. The C# DAT parser, for example, explicitly recognizes versions 56, 57 and 127 in its `IsLithTech1` predicate; that predicate alone does not prove complete support.

## Visible Application Connections

OpenBlood2's [Blood2Game.cpp](../../../OpenBlood2/game/Blood2Game.cpp) contains the application lifecycle and direct calls into the adapted game: compatibility setup, class registration through `ObjectDLLSetup`, shell creation, event processing, update and render methods. The engine's own object/message machinery and the old API adapters are therefore both part of the path to inspect.

NoMadEngine's [Blood2Game.cpp](../../../NoMadEngine/openblood2/src/Blood2Game.cpp) registers a [Blood2EntityFactory](../../../NoMadEngine/openblood2/src/Blood2EntityFactory.cpp) and connects its HUD to the ECS registry. Shared loading, timing, input and rendering are located in [GameBase](../../../NoMadEngine/nomadengine/src/game/GameBase.cpp). Shogo has its own application and entity factory using the same engine.

In Godot, [GameLoader.cs](../../../nomad/scripts/GameLoader.cs) passes the selected session to [LevelLoader.cs](../../../nomad/src/Nomad.Runtime/LevelLoader.cs). The loader locates archives, parses a DAT world, resolves textures, builds meshes and semantics, and returns `LoadedLevel`. [GodotRuntimeSceneComposer.cs](../../../nomad/src/Nomad.Runtime/GodotRuntimeSceneComposer.cs) then selects Blood 2 or Shogo gameplay composition, creates the message/runtime context, and adds the appropriate player controller and visuals. These connections were inspected in source; the full runtime path has not been exercised.

## Larger LithTech Engine Map

These reference modules are under [sources/lithtech/runtime](../../../sources/lithtech/runtime).

| Module | Source area to inspect |
| --- | --- |
| `client` and `server` | Client/server managers, engine interfaces, world state, class management and serialization |
| `world` | BSP/world structures, world loading, intersection queries and lighting data |
| `model` | `model_load.cpp`, `animtracker.cpp`, platform-specific model loading |
| `physics` and `shared` | Physics interfaces, collision, geometry, messages, object types and shared services |
| `render_a` and `render_b` | Direct3D rendering, model rendering, draw primitives and texture interfaces |
| `kernel` | Platform binding, streams/files, input, timing, memory, networking and video/music backends |
| `sound` | Sound manager and `s_dx8` driver |
| `ui` | UI implementation and bundled FreeType source |
| `controlfilemgr`, `info`, `esd`, `lithtemplate`, `winbuild` | Supporting services and build glue |

The SDK exposes later interfaces such as `ILTClient`, `ILTServer`, `ILTModel`, `ILTPhysics`, and `ILTMessage`. Whether a particular implementation can serve Blood 2's earlier interfaces is an audit question, not a conclusion of this map.
