# M2: SubwayA tunnel and rail panning

SubwayA's apparent train motion now uses its 60 authored `pan` surface effects.
The DAT reader previously discarded their names/parameters and the renderer kept
their UVs static. The correction preserves this data and evaluates the original
constant-speed mapping through SDL_GPU. It does not move the train, player or
collision world, activate happytrain early, or alter the intro cameras.
**M2 remains incomplete.**

See [contracts](../../contracts/surface_effect_contracts.md) and
[source/binary provenance](../sources/m2_surface_effect_sources.json).
The previous [playtest follow-up](m2_playtest_input.md#follow-up-stationary-train-appearance)
is superseded for the selected Pan subset.

## Original behavior and implementation

The supplied SDK defines SurfaceData O/P/Q and surface-effect callbacks, but the
compatible built-in Pan implementation is not in the available game sources.
Read-only examination of the externally installed original Client.exe identifies
its Pan initialization/update functions and numeric conversion. The implementation
uses the two parameters as P/Q origin speeds. The labels `up`, `left`, `down` and
`right` undergo numeric conversion to zero; they are not axis-selection keywords.
The zero-initialized accumulators advance by frame delta times each speed, and
O becomes original O + P*accumP + Q*accumQ.

SubwayA has 24 `up -1024`, 18 `left -1024` and 18 `left 1024` effects, on tunnel
concrete `brushcrete2X.dtx` and rails `tracks2X.dtx`. Their Q bases reverse direction
between surfaces, so all 60 texture origins move approximately -1024 units/second
along Z. Retaining this basis/sign behavior restores the missing visual cue while
keeping the actual original KeyFramer-controlled objects independent.

The renderer adds per-batch UV offsets to its shared vertex shader. Textures and
geometry still upload once per world revision. Static and animated faces may share
one image, while their batches preserve independent rates. Both opaque and sorted
transparent world passes receive session time; model and sprite callers default to
zero offset. The simulation, geometry and current object transforms are unchanged.

Absolute session time and modulo-one UV reduction replace incremental retail
float state. This makes repeated draws deterministic, freezes panning when session
time stops and restarts it on world reload. It is an explicit numerical/scheduling
adaptation, not a claim of bit-exact retail rendering. Malformed/unknown effects
remain named errors with static authored mapping. Rotate/Warble, sky animation,
lighting and complete retail presentation remain unavailable.

The 3D inspector retains effect records even for unused/invisible/hidden/unbound
surfaces and displays original text, source surface, parsed speeds, authored-basis
origin velocity and texel rates. Prepared batches report exact resources, rates
and offsets; actual opaque/alpha submissions and clock are counted separately.

## Verification

Blood2 commit: `8ddaa851e7ebb8ddcc5d0caf3caf8c0971bb831d`.

Debug and inspector-free Release builds each pass **738/738 headless tests**,
including **119 private-data cases**. The focused Clang ASan/UBSan/leak suite passes
**45/45**, including five private cases, without reported findings. Each build also
passes **9/9 GPU tests** on **NVIDIA GeForce RTX 3060 Laptop GPU**: lifecycle,
viewport, world depth/alpha, model depth, sprite alpha and both new panning cases.
Logs confirm the actual device and 60 effects / 208 submitted opaque Pan batches.

[Machine-readable evidence and logs](evidence/m2-surface-effects.json) retain exact
commands, hashes and private-capture metadata. Both intro reload cycles change
113,808 pixels over 1/64 second, retain identical train-front pixels and exactly
reproduce the original image when the phase is restored. Rendering adds no upload
or simulation tick. The waiting-for-player clock stays zero; reload resets the
world clock while the underlying object clock remains monotone.

All 14 changed C++ files pass clang-format. All 10,923 inventoried reference files
and six reference repositories remain unchanged. Local Markdown links and authored
whitespace checks pass. Private images were visually inspected and remain local.

Commands from `Blood2/`:

```sh
cmake --build build/debug -j4
cmake --build build/release -j4
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
cmake --build build/sanitize -j3 --target surface_effect_tests world_view_tests \
  texture_tests world_tests object_contract_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -j2 --output-on-failure \
  -R '^(surface_effect|world_view|texture|world|objects)\.'
# Repeat sequentially with build/release; the driver path is a local test choice.
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --test-dir build/debug --output-on-failure \
  -R '^gpu\.(surface_effect|surface_effect_private|world_depth|world_alpha|world_alpha_private|model_depth|sprite_alpha|viewport|lifecycle)$'
```

The initial private reload assertion exposed use of the monotone object clock;
it was corrected to the existing per-world origin through a const accessor,
without weakening the reset assertion. An early test helper name collided with
C `clock`; it was renamed. Final runs above use the corrected code.

The focused tests exercise the real authored intro camera at a fixed pose: a
1/64-second material advance changes tunnel/rail pixels with identical geometry
and draw transforms, and returning to the earlier time reproduces identical
pixels without another upload. This isolates texture motion from camera/object
motion. Separate application viewport smoke exercises real session-clock delivery,
hidden inspection and world reload. These automated checks are not a full retail
playthrough or a human visual speed comparison. Private captures remain local;
no commercial asset or executable/disassembly payload is committed.

Required unsupported services, broader campaign behavior and safe NEW_LEVEL keep
M2 open. The source-object class counts and exit criteria are unchanged.
