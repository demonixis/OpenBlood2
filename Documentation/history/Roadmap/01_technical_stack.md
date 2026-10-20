# Technical Stack Recommendation

**Updated: 15 October 2026.** Start a new project using C++ and optionally Rust. Existing projects are references or selective donors. SDL `main`, a small in-game inspection editor and a simple architecture are explicit project requirements.

M0 now implements the foundation in [Blood2/](../../../README.md). The [dependency manifest](../../../cmake/Dependencies.cmake) records the actual SDL/ImGui/spdlog revisions and checksums; [execution evidence](../../implementation/reports/m0.md) covers Linux/Wayland/Vulkan. Original-game integration and the additional systems below remain future work.

## Language Choice

Use **C++20 for the new engine**, with original gameplay in a separate target. Import original sources and only the portability fixes they need; do not inherit a prototype's flags or build graph. Isolate any necessary legacy language mode/options on that target. Shared headers and compile definitions must remain consistent across game and adapter code.

| Approach | Original-code preservation | Main cost | Recommendation |
| --- | --- | --- | --- |
| C++ engine + original C++ game | Direct compilation, typed calls and existing class hierarchy | Ownership, undefined behavior and legacy assumptions need explicit tests | **Default** |
| Rust engine + original C++ game | Feasible if the game stays in C++ behind a defined bridge | Callbacks, object lifetimes, strings, messages and physics cross the language boundary; two toolchains | Alternative only after a narrow integration experiment demonstrates a concrete benefit |
| C++ runtime + isolated Rust component | Keeps the gameplay boundary unchanged | One extra build/runtime boundary for each introduced component | Optional for an offline validator/converter, or a bounded parser with a well-defined API |
| Rewrite original gameplay in Rust | Replaces the preservation baseline | Revalidating game rules, AI and all interactions | Outside this roadmap |

The audit already obtained a linked C++ OpenBlood2 executable. Its most serious failures concern engine contracts, not the absence of a modern language. C++ is therefore the shortest route to testable original behavior. Rust's safe-code guarantees can help a new parser, but do not repair the C++ game or unsafe FFI layer automatically. The [Rust FFI documentation](https://doc.rust-lang.org/nomicon/ffi.html) describes the ownership, layout and unwinding obligations at that boundary.

If Rust is introduced, start with a separate command-line tool so it is not required to launch the game. For an in-process component, use an explicit C ABI or a small [CXX bridge](https://cxx.rs/reference.html), fixed-width fields, opaque owned handles, documented buffer lifetimes, and explicit destroy functions. Keep Rust collections and C++ class layouts private. Return status enums plus output parameters; no exception or panic may unwind across the boundary. CXX supports shared/opaque types, but its [`Result<T>` mapping](https://cxx.rs/binding/result.html) involves exception translation, which is unsuitable for this project's default error policy. Pin the Rust toolchain and Cargo lockfile if this optional path is adopted.

## Recommended Components

| Area | Choice | Scope and reason |
| --- | --- | --- |
| Engine and game | C++20 engine; separately configured original-game target | One main language and toolchain; preserve original behavior behind typed services |
| Platform | SDL3, latest `main` commit | Window, events, input, timing, audio and the selected GPU/OpenXR integration; record the resolved SHA |
| Rendering | SDL_GPU | Compact forward renderer for world geometry, lightmaps, models, sprites and original effects |
| Shader pipeline | Current GLSL → SPIR-V → SDL_shadercross offline; HLSL remains an option | Generate backend-specific assets before packaging; avoid shipping a shader translation toolchain unless demonstrated necessary |
| Logging | spdlog with a single, consistent fmt configuration | Structured subsystem diagnostics; avoid high-volume per-frame formatting |
| Build | CMake, Ninja, FetchContent | Per-target settings, presets, exact dependency revisions and reproducible builds |
| Tests | CTest with small purpose-built contract/probe executables | Reuse the audit reproducers; add a framework only if test maintenance warrants it |
| Diagnostics | ASan/UBSan where supported; optional Tracy | Lifetime/parser checks and measured CPU/frame/allocation behavior |
| Physics | Small collision/movement layer implementing Blood 2 contracts | Reuse corrected prototype geometry and queries; keep authoritative player/object rules compatible |
| Audio | SDL3 audio with SDL3_mixer behind an engine voice interface | Reuse decoding/mixing; implement game sound handles, lifetime, spatial parameters and priorities |
| Data formats | Hardened existing REZ, DAT, DTX/PCX and ABC readers | Preserve original content; use explicit supported-variant checks |
| UI | Original client/HUD/menu logic with a minimal 2D/font/surface backend | Preserve gameplay-facing behavior |
| In-game editor | Dear ImGui with SDL3 and SDL_GPU backends | Runtime inspection, selection and debug overlays; included in development builds |
| Entity model | Original classes, simple records, containers and handles | **No EnTT and no ECS by default**; no duplicate entity hierarchy |
| VR later | OpenXR + validated SDL_GPU integration | A separate presentation/input module, optional in builds |

SDL's GPU API provides Vulkan, D3D12 and Metal backends, with hardware/driver requirements that must be checked for the selected devices. This is a capability basis, not evidence that this game runs on those devices. [Official GPU documentation](https://wiki.libsdl.org/SDL3/CategoryGPU).

The [Metal implementation](../../implementation/reports/m2_metal.md) now packages SPIR-V and MSL from the existing small GLSL shaders through an optional pinned SDL_shadercross toolchain. Ordinary builds consume checked artifacts. HLSL migration is unnecessary for this fix; DXIL and native Apple execution remain pending. Existing SDL/ImGui/spdlog pins are unchanged.

[SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) accepts HLSL/SPIR-V and provides offline translation. The project should package validated SPIR-V for Vulkan, DXIL for D3D12, and the supported Metal shader representation for Apple builds. Keep binding layouts and coordinate conventions consistent; test each output through the actual pipeline. This recommendation does not assume every shader artifact can be compiled on any host without platform tools.

[SDL3_mixer](https://wiki.libsdl.org/SDL3_mixer/FrontPage) provides common-format decoding and mixing. Validate Blood 2's actual samples, streaming, loops and music requirements before selecting enabled codecs. Positional sound semantics, voice completion and game handle lifetime still belong to the engine. Cinematic and legacy music formats need a separate corpus check; do not assume the mixer decodes every original resource.

[spdlog](https://github.com/gabime/spdlog) and [Tracy](https://github.com/wolfpld/tracy) fit the existing C++ direction. Keep profiling optional and logging bounded. Configure error handling consistently: avoid application `try`/`catch`, check recoverable failures explicitly, and document the fatal policy for allocation or dependency failures. Compiling selected targets without exceptions requires checking their dependencies first.

## SDL Main and OpenXR

Use the **latest commit of SDL's `main` branch**, as requested, to use its working OpenXR integration. This replaces the previous stable-release recommendation and the prerequisite feasibility decision. The upstream [OpenXR header](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_openxr.h) contains the GPU session/swapchain integration used by this plan.

Resolve `main` when bootstrapping the project and when updating SDL, then record that exact SHA in the dependency manifest. M0 resolved and built [`6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1`](https://github.com/libsdl-org/SDL/commit/6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1), which reports SDL 3.5.0. Resolve the latest `main` again for future dependency updates and retain repeatable revisions between updates.

Build the flat-screen path first, with OpenXR optional at runtime/build time. Perform ordinary integration checks for views, swapchains and session lifecycle as the new renderer grows. Full headset gameplay remains M8; there is no requirement to wait for SDL 3.6 or another stable release. This documentation update does not claim a headset test of the new project.

## In-Game Inspection Editor

Use Dear ImGui for a compact inspection UI; upstream provides an [SDL_GPU renderer backend](https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_sdlgpu3.h). Add only the panels and overlays needed to inspect the live world and engine. Start with selection, object lists, properties and resource status; extend coverage with each subsystem. See the [editor scope](02_architecture.md#in-game-inspection-editor).

No separate editor application, generic reflection framework, plugin system or ImGuizmo dependency is required initially. Explicit inspection functions and basic transform fields are sufficient. Add gizmos only if they materially help verification.

## Physics and Engine Size

Do not replace original movement with a stock Jolt character controller by default. The existing prototype's use of Jolt is not evidence of equivalent stepping, sliding, touch messages, moving-door interaction, projectile collision or grounded behavior. Prefer a small compatibility layer using the world geometry, spatial acceleration and swept queries needed by original callers. Reuse existing implementations only after comparison tests.

[Jolt](https://github.com/jrouwe/JoltPhysics) remains a possible C++ collision/rigid-body component if measured requirements justify it. Keep it optional until its behavior is validated for the chosen use. Decorative physics can be isolated from authoritative original objects. Remove unconditional x86 feature assumptions from any imported configuration; ARM acceptance requires compilation and execution.

Use ordinary classes/structs, vectors/maps and explicit update loops. **Do not add EnTT or another ECS by default.** The small inspection editor reads those structures directly through controlled inspection functions. Avoid general scripting, an asset database, advanced PBR and multiple gameplay implementations. Add libraries and abstractions only to solve a current need; preserve original math conventions and optimize measured costs.

## Build and Dependency Policy

Recommend a **CMake 3.28 minimum**, tested also with a selected current 4.x version, and Ninja for normal development. This is a proposed compatibility floor to confirm in M0. Use MSVC on Windows, GCC and Clang on Linux, Apple Clang on macOS/iOS, and the Android NDK's Clang. Exact compiler/SDK versions become tested pins in M0 and the platform milestones, not unsupported compatibility promises in this document.

Use one dependency manifest and `FetchContent_MakeAvailable`, immutable commit hashes or verified release archives, and target-scoped compile options. CMake explicitly recommends hashes for remote Git content. Allow documented local/system overrides while recording the resolved versions. A populated dependency cache should permit subsequent offline builds. [FetchContent reference](https://cmake.org/cmake/help/latest/module/FetchContent.html).

Development builds include the in-game inspector; release builds may disable it. VR remains optional. Test those options independently. Separate host tools from cross-compiled targets. Keep one spdlog/fmt configuration, avoid duplicate symbol suppression and global permissive flags, and document only adaptations actually imported into the new project.

## Platform Intent

| Platform | Initial graphics path | Validation that cannot be replaced by compilation |
| --- | --- | --- |
| Linux desktop | SDL_GPU Vulkan | Driver/device startup, input, audio, real gameplay and frame capture |
| Windows desktop | SDL_GPU D3D12 | MSVC build, filesystem behavior, input/audio and gameplay |
| macOS | SDL_GPU Metal | Apple Silicon execution, packaged resources and application lifecycle |
| Android | SDL_GPU Vulkan on explicitly supported devices | Touch/controller input, suspend/resume, audio focus, storage and sustained thermals |
| iOS | SDL_GPU Metal on explicitly supported devices | Device execution, sandboxed content import, lifecycle, input and memory pressure |
| OpenXR headsets | SDL `main` GPU/OpenXR integration with a supported runtime binding | Session lifecycle, tracked views/controllers, frame timing and comfort |

Choose minimum OS versions and named devices after the capability smoke tests. Do not promise every Android GPU, simulator or headset. The target list expresses intended coverage; [phase 2 only executed Linux x86_64 builds/probes](../Audit/01_build_verification.md).
