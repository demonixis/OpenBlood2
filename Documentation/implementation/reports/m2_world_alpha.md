# M2 playtest correction: train-window textures and alpha

SubwayA's pink train windows were a renderer placeholder. The 31 visible transparent
faces already resolved `textures/greys/dirtyglass.dtx`; the previous world renderer
replaced their material with a pink color. The actual 64x64 DTX contains four-bit
alpha values of 51–102, so these windows now show their original texture with
20–40% opacity instead of the marker.

World alpha is a separate sorted pass sharing the existing world buffers, images
and depth target. Opaque world and character models draw first, then glass without
depth writes, then the existing effect and close-weapon passes. Missing resources
still produce named errors and opaque magenta markers. Invisible brush sides stay
excluded; no material-name special case or game-rule change was added.

The 3D inspector lists transparent faces by world model and resource, including
hidden/unbound models, and separates prepared faces from submitted batches/passes.
The same path runs with the UI hidden and in the inspector-free Release build.

See [contracts](../../contracts/world_alpha_contracts.md),
[source ledger](../sources/m2_world_alpha_sources.json) and
[execution evidence](evidence/m2-world-alpha.json).

## Behavioral evidence

Synthetic CPU checks exercise face separation, shared-material blend selection,
missing/sky markers, stable camera-depth ordering, posed/rotating objects, malformed
orders and hidden/inactive/pending identity handling. The actual SubwayA session
loads twice and retains 31 correctly bound glass faces with no rejected material.

GPU readback checks transparent, partially transparent, opaque and opaque-black
texels; reversed face submission; opaque geometry between/in front of glass; no
alpha depth writes; current camera/object transforms; resize, reload and failure
recovery. The real glass12 resource/geometry is rendered after two independent
world loads. Its central RGB is checked against an independently computed original
surface UV and decoded DTX alpha. Full-scene and isolated-pane images stay in the
ignored build directory, not in source control. These are engine tests, not a
retail executable or human playthrough comparison.

## Validation

Debug and inspector-free Release each pass **630/630 checks**: 583 headless and
47 serial GPU, including 128 private-data cases (92 headless). Debug times are
80.86 s / 176.41 s; Release times are
12.34 s / 113.17 s.
Clang ASan/UBSan/leak checks pass **21/21** in 8.33 s, covering
objects, texture decoding/mapping and world view/lifetime/order, including private
world reloads and the DTX corpus. No sanitizer report or GPU validation error/VUID
was observed in the retained transcripts.

The private pane's center is exactly **RGB 31,33,36**, matching the independently
calculated compositing result in Debug and Release. Each actual app viewport run
submits 31 transparent faces per presented frame through hidden UI and world reload,
with only the original two world uploads. The local captures visibly replace the
pink windows with textured glass while retaining the train, characters and weapon.

Commands from Blood2: `cmake --build build/debug -j2`,
`cmake --build build/release -j2`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Existing `build/sanitize` builds world_view_tests, texture_tests and
object_contract_tests; regex `^(world_view|texture|objects)\.` runs with `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.

Nine changed/new C++ files pass clang-format 23.1.3; all 2,562 local Markdown links
resolve. The 10,923 inventoried reference files and six reference repositories are
unchanged. An initial CMake edit omitted its closing endif; that was repaired
before successful builds/tests. Initial missing-aggregate-initializer warnings in
new test code were corrected; final build logs have no compiler warnings. Logs,
selected detailed transcripts, source hashes and the committed project patch are
retained. Commercial captures remain local. Linux/Vulkan was tested; other
platforms and retail blending parity were not.

## Remaining scope

This fixes the reported train-window marker. Polygon-center sorting is bounded
replacement rendering; intersecting transparent faces and separate translucent FX
passes do not provide global ordering. Lighting, sky, liquid rendering, generic
object alpha and cross-backend shader artifacts remain incomplete. Required
original services, safe NEW_LEVEL and the actual M2 exit gate remain open; no
acceptance criterion has been relaxed.
