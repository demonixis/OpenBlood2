# M2 — Original sprite presentation

The 12 original SubwayA DetailSprites now draw their actual blood-splat frames in
3D, using existing original placement and SPR clocks. Rotatable objects preserve
their authored quaternion; other sprites face the camera. The Sprites inspector
shows decoded size, current frame, alpha counts, draw mode and failures separately
from submitted GPU passes.

See [contracts](../../contracts/sprite_view_contracts.md),
[source selections](../sources/m2_sprite_view_sources.json) and
[execution evidence](evidence/m2-sprite-view.json).

## Behavior and boundaries

A small owned SpriteView snapshot reuses the bounded DTX decoder and existing
SDL_GPU renderer. Texture dimensions times object scale form half extents, using
an examined later-generation renderer as a reference; retail raster parity is not
claimed. Source-alpha blending, stable far-to-near order and world/model depth
preserve occlusion. Sprites do not write depth and draw before view weapons.

The original SDK says FLAG_SPRITECHROMAKEY is disabled. No arbitrary RGB color is
removed: opaque black remains opaque, and actual DTX alpha controls transparency.
The private 128x128 bloodsplat3 texture contains 14,828 zero-alpha and 1,556
partial-alpha texels. Glow/bias/no-Z/sky modes remain explicit errors. Lighting,
object tint/alpha services, clipping polygons and additive/multiply effects remain
unavailable. This does not change opaque train-glass rendering.

Caches clear at world revisions; snapshots own no original pointers, do not advance
clocks and run independently of UI visibility. Missing frames stay inspectable
without guessed dimensions. Source-class accounting remains **203 created / four
transferred sounds / 372 editor-only / one unsupported Gideon / zero failures**.
Safe automatic NEW_LEVEL, required FX emitters and other services keep M2 open.

## Verification

Debug passes **514/514 headless checks** (105.11s)
and **31/31 GPU checks** (156.23s). Inspector-free Release passes
**514/514 headless checks** (10.77s) and **31/31 GPU checks**
(102.61s). Both total **545/545**, including 105 private-data
checks (78 headless). Targeted Clang ASan/UBSan/leak checks pass **44/44**
(60.36s). Existing opaque world/model and player tests remain enabled.

`--inspect-player` exits 0; `--start-game` still exits 6 for incomplete gameplay.
13 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; 2,218 local Markdown links resolve.
The initial development compile exposed unsupported DVector subtraction syntax,
which was corrected. An early focused GPU app launch raced the linker; the
post-build rerun passed all three cases. No acceptance assertion was weakened.

Commands from Blood2:

```sh
cmake -P cmake/GenerateWorldShaders.cmake
cmake --build build/debug -j4
ctest --test-dir build/debug -L headless --output-on-failure -j2
ctest --test-dir build/debug -L gpu --output-on-failure -j1
cmake --build build/release -j2
ctest --test-dir build/release -L headless --output-on-failure -j2
ctest --test-dir build/release -L gpu --output-on-failure -j1
cmake --build build/sanitize -j2 --target sprite_view_tests sprite_tests texture_tests world_view_tests model_view_tests object_contract_tests attachment_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(sprite_view|sprites|texture|world_view|model_view|objects|attachments)\.' --output-on-failure -j1
```


Synthetic CPU checks verify authored/camera orientation, half extents, UVs, stable
ordering, frame changes/cache reuse, zero scale, malformed/missing frames, unsupported
flags, invalid transforms and the instance budget. Three private SubwayA cycles
exercise alive/inactive/hidden/pending/deleted sprites, unchanged clocks and full
unload/reload. Existing placement/repeat and decoder tests remain unchanged.

GPU pixel assertions cover transparent red, partial green, opaque red and opaque
black, overlapping sprites, world occlusion, no depth writes, resize and reupload.
A separate private capture draws one actual sprite at its authored pose/scale with
an explicitly chosen observation camera; this is not an authored gameplay view.
The application smoke requires 12 quads while UI is hidden and after reload.
[Private blood-splat capture](evidence/m2-sprite-view-bloodsplat.png) and
[inspector capture](evidence/m2-sprite-view-inspector.png) were visually reviewed.

Only Linux/Vulkan is tested. No original-executable visual comparison or other
platform support is claimed. References and external commercial files are preserved.
