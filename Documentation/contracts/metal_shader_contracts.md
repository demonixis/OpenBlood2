# Packaged SPIR-V and Metal shaders

The renderer now bundles both SPIR-V and Metal Shading Language (MSL) for its
shared world, model, sprite and FX pipelines. This removes the Vulkan-only
world-entry rejection reported on macOS. M2 remains incomplete. Native Metal
compilation and presentation have not been executed on this Linux host; see
[verification](../implementation/reports/m2_metal.md).

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
current posed world position for the bounded radial illumination. Storage textures are unused; only the skinned vertex shader reads storage buffers. Keep reflection and all
packaged artifacts synchronized when HLSL changes. Source texture coordinates,
SDL GPU depth/orientation conventions, current transforms, alpha policy and
world-clock panning are identical inputs on both paths; add no backend-only
axis flip or second animation clock.

MSL is compiled by SDL's Metal backend using the host Metal compiler when each
pipeline is created. This is distinct from shipping shadercross at runtime.
Dear ImGui already supplies its own Metal shaders; its backend is unchanged.
The existing inspector's Renderer field shows SDL's actual selected backend.
Shader failures remain explicit errors, not successful blank-world presentation.

## Offline regeneration

The optional standalone project [tools/shaders](../../tools/shaders/CMakeLists.txt)
pins SDL_shadercross main and its exact SPIRV-Cross submodule with SHA-256 checked
archives. [Provenance](../implementation/sources/m2_metal_sources.json) records both revisions. Existing
SDL/ImGui/spdlog pins are unchanged; shader tools reuse the already-built SDL.
No reference repository or commercial resource is a shader input.

HLSL sources (`src/engine/shaders/*.hlsl`) are the single source of truth; the
earlier GLSL sources are removed. They follow SDL_GPU's HLSL register conventions:
vertex storage buffers `t#, space0` and uniforms `b#, space1`; fragment textures and
samplers `t#/s#, space2` and uniforms `b#, space3`; vertex inputs and varyings use
`TEXCOORD` semantics in declaration order. The pinned SDL_shadercross, built with DXC
(an installed `dxcompiler` with `dxcapi.h`, such as the Vulkan SDK's), compiles each
source to Vulkan SPIR-V, MSL 1.2 and reflection metadata; CMake turns the SPIR-V
module into a word array. The generator rejects storage textures, read-write
storage or an unexpected entry name. Eight headers are checked in, so normal builds
require neither generation tools nor network access beyond ordinary dependency
fetching. DXIL is not generated: on hosts without DXC's signing library (macOS) it
would be unsigned, and D3D12 stays unadvertised until signed artifacts are tested.

From Blood2, with DXC installed and SDL already built:

```sh
cmake -S tools/shaders -B build/shader-tools -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSDL3_DIR="$PWD/build/dev/_deps/sdl3-build"
cmake --build build/shader-tools -j3 --target regenerate
```

DXC is used only by this optional tool project; no shader compiler is linked into
the game. To compare regeneration without overwriting checked headers:

```sh
cmake -DSHADERCROSS="$PWD/build/shader-tools/_deps/shadercross-build/shadercross" \
  -DOUTPUT_DIR="$PWD/build/shader-regeneration" \
  -P cmake/GenerateWorldShaders.cmake
```

## Verification boundary

Headless tests exercise both packaged formats, stages, generated entry names,
resource bindings, static storage, uniform size and unsupported-format failures.
Byte-for-byte regeneration checks all eight artifacts. These checks cannot validate
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

The [presentation performance continuation](render_performance_contracts.md) adds `skinned.vert`, the first shader reading read-only storage buffers (node palette and packed deformation, MSL `[[buffer(1)]]`/`[[buffer(2)]]`). The generator now records each shader's storage-buffer count; all eight artifacts were regenerated together with glslangValidator 16.3.0, and existing SPIR-V/MSL bytes are unchanged apart from the version comment.

The [presentation performance continuation](render_performance_contracts.md) moves the four shaders to HLSL compiled by SDL_shadercross with DXC (Vulkan SDK 1.4.350, dxcompiler 1.9.0.5347). Bindings, vertex attribute locations, varying locations and uniform layouts are unchanged; standalone renderer readbacks and application captures are compared against the GLSL-generated artifacts in that contract.

The [baked lighting continuation](baked_lighting_contracts.md) changes both fragment shaders to add the radial light to the face color before the albedo multiply. Only the four fragment artifacts changed when all eight were regenerated; bindings and uniform layouts are unchanged.

The [lightmap continuation](lightmap_contracts.md) adds vertex attribute/varying 3 (lightmap page coordinates; skinned models output -1) and fragment texture/sampler 1 (the lightmap page, a bilinear clamped sampler) to all shaders. The fragment uniform's `modulation.y` carries the lightmap scale. All eight artifacts were regenerated.

The [model shadow continuation](model_shadow_contracts.md) adds `shadow.frag` (caster heights, no resources) and gives both world fragments texture/sampler 2 (shadow tiles) and uniform buffer 1 (`ShadowUniform`). `skinned.vert` treats `forward.w > 0.5` as an orthographic projection for the tiles. Ten artifacts are now regenerated together.

The [enhanced renderer](enhanced_renderer_contracts.md) adds `pbr.frag.hlsl`, compiled
four times with `-DPBR_HIGH`/`-DPBR_ALPHA` defines (`pbr_frag`, `pbr_high_frag`,
`pbr_alpha_frag`, `pbr_high_alpha_frag`). These are the first fragment shaders reading
read-only storage buffers: after four sampled textures, at `t4`/`t5` in space2. They
also use uniform buffers 2 and 3. `world.vert` gains vertex attribute and varying 4 (the
face-plane normal). Eighteen artifacts are now regenerated together. Every original
fragment artifact is byte-identical after regeneration. Only Metal execution on Apple M5
was verified.

[Post-processing](post_processing_contracts.md) adds eleven shaders: `-DGLOW_TARGET` variants
of `world.frag`, `sprite.frag` and the four PBR fragments (a second output, MSL `[[color(1)]]`),
and `post.vert` (`SV_VertexID`, no inputs), `bloom_down.frag`, `bloom_up.frag`,
`composite.frag` and `fxaa.frag` (one or two sampled textures, one uniform buffer). The
generator maps `<stem>_<vert|frag>` to `<stem>.<stage>.hlsl` unless a `<name>_source` variable
names a shared source. Forty-four artifacts, including the four XR line artifacts, are now
regenerated together; the 22 earlier ones are byte-identical. Only Metal execution on Apple M5
was verified.
