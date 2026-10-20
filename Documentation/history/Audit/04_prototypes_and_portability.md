# Prototype Reuse, Portability, and Performance

## Suitability for the Preservation Objective

| Source tree | Strongest contribution | Material limitation |
| --- | --- | --- |
| `sources/blood2` | Original game rules, object classes, weapon/AI behavior, client/server contracts, save/load callbacks | Needs a functioning compatible engine service layer; no standalone modern build was attempted |
| `OpenBlood2` | Existing modernized original code, static registration, a linked application, resource/rendering work, several working utilities | Only four level-instantiated classes; broken core contracts, manual ABI tables, bypasses and substitute implementations |
| `NoMadEngine` | C++ format/rendering work, EnTT components, Jolt integration, actions, doors, triggers, weapons, audio, two game frontends | Gameplay is a separate implementation; non-VR build is broken; full fidelity and runtime operation unverified |
| `nomad` | Godot integration, independently testable C# formats/gameplay, synthetic tests, model and world composition | Separate gameplay implementation; tests omit native Godot runtime; important presentation behavior is placeholder-based |
| `sources/lithtech` | Reference algorithms and implementations for later engine systems | Different generation, large platform-specific dependency surface, modern CMake generation failures |
| `sources/Shogo_Aegis` | Modified Shogo reference material for later comparative work | No deep fidelity audit or build in this Blood 2-focused phase; modifications must be distinguished from original behavior |

The existing work supports recovering components and contracts. It does not establish that any prototype can already replace the original game. Selecting one as the sole foundation would require a separate architecture decision; this audit leaves that decision for the roadmap discussion.

## NoMadEngine: Useful Engine Work, Independent Game Rules

[GameBase](../../../../NoMadEngine/nomadengine/src/game/GameBase.cpp) coordinates input, physics, doors, keyframers, triggers, audio, weapons, and HUD updates. [PhysicsWorld](../../../../NoMadEngine/nomadengine/src/physics/physics_world.cpp) contains Jolt integration, collision layers, character movement, raycasts, and entity synchronization. These are more than empty interfaces and deserve consideration as reusable implementation work.

However, [Blood2EntityFactory](../../../../NoMadEngine/openblood2/src/Blood2EntityFactory.cpp) maps selected DAT classes into new ECS components; it does not instantiate the original `CPlayerObj`, `AI_Mgr`, or `CWeapon` classes. [WeaponSystem](../../../../NoMadEngine/nomadengine/src/systems/weapon_system.cpp) implements its own ammo, cooldown, reloading, raycasting, and viewmodel behavior. [TriggerSystem](../../../../NoMadEngine/nomadengine/src/systems/trigger_system.cpp) checks overlaps and emits its own events. These systems may be useful prototypes or adapters, but they are not evidence that original semantics have survived.

An example of approximation is the factory mapping of `SubmachineGunPU` to its Beretta weapon type. The audit does not evaluate every weapon mapping, but this makes the distinction concrete: matching class names or loading an appropriate-looking model is weaker than preserving behavior.

Jolt's integration offers a modern collision/character foundation. It does not automatically reproduce LithTech stepping, slope rules, dimensions, moving brushes, standing-on relationships, collision filters, force calculations, or event ordering. Original [MoveMgr.cpp](../../../../sources/blood2/ClientShellDLL/MoveMgr.cpp), [PlayerObj.cpp](../../../../sources/blood2/ObjectDLL/PlayerObj.cpp), and [physics_lt.h](../../../../sources/blood2/AppHeaders/physics_lt.h) remain the behavioral references.

The inspected [PhysicsWorld::Update](../../../../NoMadEngine/nomadengine/src/physics/physics_world.cpp), lines 358–371, clamps elapsed time to 1/30 second and advances Jolt once with that value. [GameBase::Update](../../../../NoMadEngine/nomadengine/src/game/GameBase.cpp) gives other systems its scaled frame delta. During slow frames, physics can therefore advance less time than other systems unless an external policy compensates. No accumulated-time catch-up is present in this inspected physics function. This is a source-based timing risk, not a measured desynchronization report.

## Godot Prototype: Testable Domain Logic, Partial Runtime Presentation

The C# code separates formats, shared data, gameplay definitions, runtime composition, and Godot scripts. Its [test project](../../../../nomad/tests/Nomad.Tests/Nomad.Tests.csproj) links many non-Godot sources directly, enabling inexpensive synthetic checks. The successful checks cover useful behaviors: parser primitives, archive extraction, model geometry, coordinate mirroring, CPU animation, texture decoding, object classification, weapon cooldown/ammo, doors, triggers, conversations, and AI state decisions.

The gameplay models remain separate implementations. [Blood 2 AiRuntime](../../../../nomad/src/Nomad.Gameplay/Blood2/AiDefinition.cs) chooses states using a compact set of perception/death/default-state conditions; it does not execute original enemy subclasses, their animation-driven routines, or original scripts. [WeaponRuntime](../../../../nomad/src/Nomad.Gameplay/Blood2/WeaponDefinition.cs) is likewise a new state/cooldown implementation, even where catalog values refer to original concepts.

Two concrete runtime limits matter:

- [GodotLithEffectPresenter.PresentSound](../../../../nomad/src/Nomad.Runtime/GodotLithEffectPresenter.cs) creates a temporary node with sound-path metadata, then frees it. It does not create an audio stream or play sound. Model/particle/decal intents are presented as generic marker meshes. Passing effect-queue tests proves intent production, not sound or original visual effects.
- [LithCombatResolver](../../../../nomad/src/Nomad.Gameplay/LithCombat.cs) raycasts against `LithWorldQuery` for hitscan and creates a model intent for projectiles. That path alone does not simulate an original projectile object with movement, lifetime, touch/damage callbacks, explosions, and save/load state. Other runtime logic must be evaluated separately; the audit does not claim every damage path is absent.

[GodotLithMessageRuntime](../../../../nomad/src/Nomad.Runtime/GodotLithMessageRuntime.cs) handles a selected command vocabulary such as visibility and sound intents. Its existence is useful, but it is not an implementation of all original object messages, target semantics, and campaign scripts.

The main C# build establishes compile-time consistency. The 59 completed non-asset tests establish their specified assertions under .NET 10 roll-forward. Neither establishes scene correctness, complete retail-level semantics, Godot physics equivalence, animated socket behavior, exported builds, or a playable campaign.

## Portability Assessment

| Target | Evidence obtained | Remaining uncertainty |
| --- | --- | --- |
| Linux x86_64 | OpenBlood2 and VR-enabled NoMadEngine compiled/linked after dependency override; OpenBlood2 CLI launched; C# compiled and synthetic tests ran | Interactive graphics, audio, input, game sessions, release builds and packaging were not tested |
| Windows | Build branches and compatibility shims inspected | No MSVC/Windows build or execution; manual vtables and `/FORCE:MULTIPLE` behavior require particular care |
| macOS / ARM64 | Apple branches and prior compatibility work present | No Apple compiler/runtime test; C++ ABI, graphics backend and OpenXR assumptions remain unresolved |
| Android / iOS | Future project targets documented at the workspace root | No mobile package/export build or device test; filesystem access, controls, lifecycle, ABI, graphics and memory budgets remain open |
| OpenXR headsets | NoMadEngine OpenXR-enabled code compiled on Linux | No headset/session test; current bindings are OpenGL/WGL or Xlib/GLX-oriented, not proof of mobile or universal headset support |

The current C++ renderers use OpenGL, GLAD, GLSL shaders, GL resources, and OpenGL-specific XR swapchains. The target stack in the workspace instructions is SDL3/SDL_GPU. **SDL3 usage does not mean SDL_GPU is implemented.** A renderer/backend change affects resource ownership, pipelines, shaders, synchronization, and XR graphics binding; it is not just a windowing-library substitution. This audit does not perform that migration.

NoMadEngine's [root CMake](../../../../NoMadEngine/CMakeLists.txt), lines 140–143, forces SSE4/AVX/AVX2 options for Jolt, and the Linux build command contains AVX2/FMA-related flags. That establishes an x86 feature baseline for the tested build. It does not show that the executable can run on older x86 CPUs or that all ARM/mobile configurations are correctly gated. The disabled-VR compile failure also shows that optional features are not yet cleanly isolated.

Original Windows APIs, multibyte-string helpers, filesystem conventions, integer widths, C++ class layouts, and message identities have been partially adapted. [LithTechCompat.h](../../../../OpenBlood2/blood2/Shared/LithTechCompat.h) and fixed-width type changes are useful work. Remaining `%lu`/32-bit argument warnings and competing class/interface definitions show that a successful Linux build is insufficient proof of portability.

## Performance: Identified Costs, No Benchmark Claims

No frame-time, GPU-time, memory, or loading benchmark was taken. The audit cannot justify calling any prototype “highly optimized.” The following are source-visible costs or correctness-sensitive optimizations:

| Area | Source-visible behavior | Implication |
| --- | --- | --- |
| OpenLithTech collision | [CollisionManager](../../../../OpenBlood2/OpenLithTech/src/physics/collision_manager.cpp) traverses world models and polygons for geometry queries | More queries and larger levels increase work; existing BSP data is not itself proof that queries use a spatial hierarchy |
| C# world queries | [LithWorldQuery](../../../../nomad/src/Nomad.Formats/LithWorldQuery.cs) scans models/polygons; point classification uses model bounds | Suitable synthetic reference utility; not evidence of a fast dynamic-world query service or exact BSP containment |
| Archives | Whole-file buffers plus copied extraction buffers | Peak memory can exceed archive size substantially when decoded resources are included |
| Resource lookup | C# linear entry/archive searches with path normalization | Repeated resolution can benefit from a measured cache/index policy; collision/override correctness comes first |
| Animation | CPU fallback rebuilds/uploads vertex data; GPU skinning also exists | Retain the distinction between paths when measuring; do not attribute all objects to the expensive path |
| Animation culling | Offscreen/distant controller updates are skipped | Visual savings must not suppress required gameplay events or advance game state inconsistently |
| Main timing | Frame-delta-driven updates and NoMad physics clamping | Deterministic comparison needs an explicit timing policy and recorded input/time traces |

These observations identify where measurement would be useful. They do not establish which system dominates runtime cost or justify replacing working code before profiling.

## Reuse Conditions

The original game code is the strongest source for rules and interactions. Existing platform shims, parsers, tools, renderer work, input abstractions, and physics integration are candidates for reuse after their contracts are checked. The C# synthetic fixtures are particularly useful as portable behavioral examples, provided their assertions are not mistaken for original-game equivalence.

The major unresolved design boundary is how original game objects call engine services without duplicated types and manual ABI reconstruction. Another is how rendering/physics data remain synchronized with authoritative gameplay state. These are architecture questions for phase 3; the present evidence supports addressing them explicitly rather than treating the prototypes' visible output as proof that they are already solved.
