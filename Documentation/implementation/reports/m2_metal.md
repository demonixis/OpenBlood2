# M2: packaged Metal shaders

The macOS playtest could compile and start the application but could not enter
the world because WorldRenderer accepted only SPIR-V. The renderer now selects
bundled MSL on Metal, covering the shared world, model, sprite and FX pipelines.
Both opaque and alpha paths retain current transforms and surface panning.
**M2 remains incomplete. The user confirmed successful macOS execution on
17 October; native automated coverage remains pending.**

See [shader contracts](../../contracts/metal_shader_contracts.md),
[tool/source provenance](../sources/m2_metal_sources.json) and
[optional generator](../../../tools/shaders/CMakeLists.txt).

## Implementation

The existing three GLSL shaders remain the common source. glslangValidator emits
SPIR-V; pinned SDL_shadercross translates that bytecode to MSL and reflects its
bindings. All six generated headers are checked in. Normal CMake builds require
neither ShaderCross nor glslang, and there is no runtime file lookup or translation
library. SDL's Metal backend compiles the embedded MSL when creating a pipeline.

SDL_shadercross main resolves to `1ff05bec573988a98ef9e0260b4da44f512b8367` on
17 October 2026; its exact SPIRV-Cross submodule is
`1a6169566c73d3da552748fc372fe2bbb856e46e`. The optional standalone tool project
checks archive hashes, reuses the already-built SDL and disables DXC. The existing
SDL/ImGui/spdlog runtime pins are unchanged. This follows the roadmap's offline
shader pipeline while retaining the current GLSL instead of rewriting it in HLSL.

Runtime backend negotiation now advertises only the provided SPIR-V/MSL formats.
The shared selector supplies `main` for SPIR-V, `main0` for MSL, the correct stage
and reflected resource counts. It rejects unsupported formats and invalid roles
with empty output. Shader creation failure releases any preceding shader and
returns the SDL error. DXIL/metallib are not advertised without actual artifacts.

The MSL layout preserves the eight-float4 vertex uniform, three vertex attributes,
fragment sampler and alpha modulation uniform. No extra coordinate conversion,
clock, material policy or original gameplay logic is introduced. The existing
inspector's Renderer field and startup logs identify the selected SDL backend.
Dear ImGui's own Metal path is unchanged.

## Verification

Project commit: `01eb4e84e824cae3d4a9a4bd755078d9d4188922`.

Debug and inspector-free Release builds each pass **739/739 headless tests**,
including **119 private-data cases**. The focused Clang ASan/UBSan/leak run passes
**7/7**, including one private case. New shader-selection checks cover both formats,
all three stages/roles, resource bindings, entry names, static storage, uniform
layout and failure clearing. They do not execute a Metal compiler.

Each build also passes **9/9 GPU tests** on **NVIDIA GeForce RTX 3060 Laptop GPU**:
lifecycle, viewport, world depth/alpha, model depth, sprite alpha and synthetic/private
panning. Actual GPU logs identify NVIDIA; no Vulkan validation errors are recorded.
Three private GPU cases exercise authored content. These are Vulkan regression
results, not Metal execution results.

The repository's optional shader-tool project configures/builds successfully. Its
`regenerate` target passes, and an independent output directory reproduces all
six packaged headers byte-for-byte with glslangValidator 16.2.0 and pinned
SDL_shadercross. The three pre-existing SPIR-V headers remain byte-for-byte unchanged.
All five authored C++ files pass clang-format. The 10,923 inventoried reference
files and six reference repositories remain unchanged. No commercial assets or
private captures are committed.

[Machine-readable evidence](evidence/m2-metal-shaders.json) records commands,
regeneration hashes, build/test logs and the final project revision. Initial local
test compilation exposed mixed signed/unsigned initializer-list deduction and a
missing original BaseClass link from the layout-test include; explicit SDL format
types and the existing game target resolve those test-build issues. Final runs
use the corrected test without weakening its assertions.

Commands from Blood2:

```sh
cmake --build build/debug -j4
cmake --build build/release -j3
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
cmake --build build/sanitize -j3 --target world_shader_tests world_view_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -j2 --output-on-failure \
  -R '^(world_shaders|world_view)\.'
# Repeat sequentially with build/release; this driver path is a local test choice.
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --test-dir build/debug --output-on-failure \
  -R '^gpu\.(surface_effect|surface_effect_private|world_depth|world_alpha|world_alpha_private|model_depth|sprite_alpha|viewport|lifecycle)$'
cmake -S tools/shaders -B build/shader-tools -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSDL3_DIR="$PWD/build/debug/_deps/sdl3-build"
cmake --build build/shader-tools -j3 --target shadercross
cmake --build build/shader-tools --target regenerate
cmake -DSHADERCROSS="$PWD/build/shader-tools/_deps/shadercross-build/shadercross" \
  -DOUTPUT_DIR="$PWD/build/shader-regeneration" \
  -P cmake/GenerateWorldShaders.cmake
```

## User-confirmed macOS execution; automated Apple checks pending

The user subsequently confirmed that the updated build works on macOS. This is
user-reported execution of the new build, not a local automated Metal result.
Hardware, macOS/SDK versions and detailed native test outcomes were not supplied.
This Linux host cannot exercise Apple's Metal compiler/driver. For a reproducible
Apple check, rebuild normally, retain the external content path, then run:

```sh
SDL_GPU_DRIVER=metal ./build/debug/blood2 --view-world
```

Automatic selection should also choose Metal. Verify intro/world entry, mouse
capture, models, alpha windows, tunnel panning, resize and reload, then run the
focused GPU suite above with `SDL_GPU_DRIVER=metal` in place of the NVIDIA variable.
Record actual Apple hardware, macOS/SDK and results. No native automated Metal suite, iOS, Windows/D3D12 or pixel-identical retail
rendering result is claimed.
Required original services and safe NEW_LEVEL still keep the M2 exit gate open.
