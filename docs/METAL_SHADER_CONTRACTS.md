# Packaged SPIR-V and Metal shaders

The renderer now bundles both SPIR-V and Metal Shading Language (MSL) for its
shared world, model, sprite and FX pipelines. This removes the Vulkan-only
world-entry rejection reported on macOS. M2 remains incomplete. Native Metal
compilation and presentation have not been executed on this Linux host; see
[verification](../../Documentation/Implementation/M2_METAL.md).

## Runtime contract

`WorldShaderFormats` advertises only SPIR-V and MSL to SDL_CreateGPUDevice. SDL
selects a supported backend; Metal receives MSL and Vulkan receives SPIR-V. Do not
advertise DXIL or metallib until corresponding artifacts actually exist. In
particular, Windows currently needs SDL's Vulkan path rather than D3D12.

`world_shader_info` supplies static owned code, the correct stage, entry point and
reflected sampler/uniform counts. SPIR-V exports `main`; ShaderCross-generated MSL
exports `main0`. Reject missing formats and invalid shader roles with cleared
output. If both formats are offered, prefer SPIR-V. A failed vertex/fragment or
pipeline creation returns the SDL error and releases already-created shaders.
There is no runtime shader-file lookup or translation dependency.

The vertex uniform is eight float4 fields (128 bytes), including the per-batch
texture offset. Its uniform buffer is slot 0 and vertex attributes are 0/1/2.
Both fragments sample texture/sampler 0 and use uniform buffer 0. The flashlight
continuation expands that fragment buffer to three float4 fields (48 bytes):
modulation, world position/radius and RGB light color. Both stages pass the
current posed world position for the bounded radial illumination. Storage buffers/textures are unused. Keep reflection and all
packaged artifacts synchronized when GLSL changes. Source texture coordinates,
SDL GPU depth/orientation conventions, current transforms, alpha policy and
world-clock panning are identical inputs on both paths; add no backend-only
axis flip or second animation clock.

MSL is compiled by SDL's Metal backend using the host Metal compiler when each
pipeline is created. This is distinct from shipping shadercross at runtime.
Dear ImGui already supplies its own Metal shaders; its backend is unchanged.
The existing inspector's Renderer field shows SDL's actual selected backend.
Shader failures remain explicit errors, not successful blank-world presentation.

## Offline regeneration

The optional standalone project [tools/shaders](../tools/shaders/CMakeLists.txt)
pins SDL_shadercross main and its exact SPIRV-Cross submodule with SHA-256 checked
archives. [Provenance](M2_METAL_SOURCES.json) records both revisions. Existing
SDL/ImGui/spdlog pins are unchanged; shader tools reuse the already-built SDL.
No reference repository or commercial resource is a shader input.

Current GLSL sources remain the source of truth. glslangValidator 16.2.0 creates
Vulkan 1.0 SPIR-V, then SDL_shadercross emits MSL 1.2 and reflection metadata.
Retaining these small existing GLSL shaders avoids an unrelated HLSL rewrite.
Future shader-language changes must preserve the same tested binding contract.
The generator rejects unsupported storage resources or an unexpected entry name.
Six headers are checked in, so normal builds require neither generation tools nor
network access beyond ordinary configured dependency fetching.

From Blood2, with glslangValidator available and SDL already built:

```sh
cmake -S tools/shaders -B build/shader-tools -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSDL3_DIR="$PWD/build/debug/_deps/sdl3-build"
cmake --build build/shader-tools -j3 --target regenerate
```

The optional toolchain uses no DXC and links no shader compiler into the game.
Its standalone CMake project is not part of ordinary application configuration.
To compare regeneration without overwriting checked headers:

```sh
cmake -DSHADERCROSS="$PWD/build/shader-tools/_deps/shadercross-build/shadercross" \
  -DOUTPUT_DIR="$PWD/build/shader-regeneration" \
  -P cmake/GenerateWorldShaders.cmake
```

## Verification boundary

Headless tests exercise both packaged formats, stages, generated entry names,
resource bindings, static storage, uniform size and unsupported-format failures.
Byte-for-byte regeneration checks all six artifacts. These checks cannot validate
Apple's Metal compiler or driver. NVIDIA Vulkan readback covers actual world/model
occlusion, alpha, sprites, panning, viewport reload and device lifetime.

On macOS, rebuild normally, configure the external content root, then launch:

```sh
SDL_GPU_DRIVER=metal ./build/debug/blood2 --view-world
```

`SDL_GPU_DRIVER` is an optional explicit test selection; automatic selection
should also choose Metal. Check the startup/inspector backend, enter the authored
intro, then exercise mouse capture, world/model depth, transparent windows, tunnel
panning, resize and unload/re-entry. With private/GPU tests enabled, run the same
focused suite documented in the evidence under `SDL_GPU_DRIVER=metal`. Record the
actual Apple hardware, macOS/SDK and results before claiming native validation.
iOS, D3D12, full lighting and complete M2 acceptance remain outside this change.

On 17 October 2026 the user confirmed the updated build works on macOS. Record this
as user-reported execution; hardware/OS/SDK and a native automated GPU test run
remain unrecorded. Local evidence remains Linux/NVIDIA Vulkan.
