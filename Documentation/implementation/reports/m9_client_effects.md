# M9 — local client visual-effects replica

Implemented against `a82b7855`, following the authorized
[multiplayer contract](../../contracts/multiplayer_contracts.md).
This lot separates **effect presentation** from the local server registry. M9 is
still in progress; it does not add network effect delivery or accept M2/M3.

## Delivered boundary

`ClientSession::effects` owns a passive `ClientEffectsFrame`. Publication joins the
existing `publish_client_world` boundary, once per presenter update and before
standalone scene helpers. No publication step advances particles, animations,
physics, sound, original callbacks or RNG. Rendering another eye or receiver
consumes the accepted values without re-entering the server.

The copied state covers every current scene-effect family:

| State | Client representation and preserved behavior |
| --- | --- |
| Explosion, warp, blood, weapon and ballistic sprites | Immutable decoded sprite plus copied playback/pose/scale/color/alpha; mark polygons copied. Negative unaligned warp sprites still use the current eye's half-turn. |
| Sparks, lightning, blood drops, rings, flames, smoke/trails and particle streams | Owned particle arrays and evaluated values; existing individual versus whole-system batching and diagnostic tags retained. |
| Explosion models/fragments, flesh/destruction gibs, beams and casings | Immutable decoded model plus copied animation, transform, skin and visibility/alpha. Existing flesh opacity and destruction translucency remain distinct. |
| PolyGrids | Copied heights, palette, pose, dimensions, pan/scale, current texture and checked owner identity; current user-flag visibility. |
| Ballistic tracers | Copied center/axes; the existing three hardware lines remain a client renderer detail. |
| Flashlight | Copied checked point-light state, retaining its separate draw path and owner/source visibility gates. |
| Dynamic/projectile lights | Evaluated light values/flags in original order, including unchanged unsupported fog-light and shadow-request counters. Per-receiver selection/shader budgets are unchanged. |
| World/volume fog | Copied parameters and cached, independently owned posed volume shapes; first-containing-volume, hidden-volume masking, fog-off fallback, truncation and per-eye depth rules unchanged. |

Sprites before and after view-weapon muzzle flashes retain their exact insertion
order. The muzzle/held-weapon path is already client shell presentation and keeps
its existing desktop/XR placement. Scene helpers call engine-core-only
`append_effect_*`/`gather_scene_effects` consumers. They do not read original FX
instances, the server registry, or gameplay classes. GPU buffers, shaders,
transparency sorting, Original/Enhanced rendering and post-processing are untouched.

Each complete frame has a world epoch, increasing sequence and nondecreasing time.
Family-qualified effect IDs distinguish independent legacy pools and component
indices. Old worlds/sequences, duplicate/invalid IDs, invalid poses and aggregate
record/particle/height budgets are rejected before replacing the accepted frame.
Absent entries disappear on the next publication; unload clears the frame and
shape cache. A copied frame and its immutable assets remain renderable after the
source objects, pools and session are destroyed.

Limits are 65,536 records per category, 1,048,576 particles and 1,048,576 height
samples per frame, with bounded strings and mark polygons. Existing effect pools
and renderer budgets still apply first in normal play. This is an **in-process
presentation frame**, not a wire codec: immutable decoded model/sprite resources
are shared locally, and fog shapes are client-owned copies. No HOBJECT, original
effect instance, gameplay callback or GPU object enters the frame. A network
receiver will still need resource-name resolution and complete untrusted-field
validation. Existing per-entry cosmetic/resource diagnostics remain visible;
invalid authored fog remains a reported disabled interval rather than a gameplay
mutation.

## Preserved simulation and remaining extraction

The existing effect producers and advancing pools remain in the local game adapter
for this lot. Their collision queries, child-effect publication and cosmetic audio
calls still use that legacy bridge. This is deliberately explicit: the lot removes
all scene-consumer dependence on those pools, but does **not** yet move every
original FX update to an independent remote-client simulation or event protocol.
Their current order, recipe branches and independent cosmetic RNGs are unchanged.
A future event/runtime extraction must preserve these dependencies and headless
server behavior instead of merely relocating owning fields.

Server `AudioTimeline`, client playback ownership, CMoveMgr and its single
integration path are unchanged. The replica cannot create sounds, apply damage,
move a body or consume random draws. No saved payload or source game rule changes.
Source attribution and renderer-code extraction are recorded in the
[source ledger](../sources/m9_client_effects_sources.json).

## Verification

Linux x86_64, GCC 16.2.1 / Clang 22.1.8 and native NVIDIA RTX 3060 Laptop
Vulkan. [Machine-readable results and trace hashes](evidence/m9_client_effects_validation_2026-10-30.json)
retain initial failures and the targeted reruns.

| Check | Result | Scope |
| --- | --- | --- |
| Selected headless regressions | **303/303 pass** after three runner-environment reruns | 222 synthetic, 81 private-data cases; objects, saves, movement/environment, camera/HUD/audio and FX |
| Native GPU regressions | **40/41 pass** after three fixture corrections | 13 synthetic, 28 private-data cases; **PolyGrid remains failed** |
| Clang ASan/UBSan boundaries | **22/22 pass** | Separate server-only tree; no GPU sanitizer claim |
| Preservation traces | **5/5 byte-identical** | 240 FX frames, 360 audio, 360 dry movement, 720 environment, 300 camera/HUD |

The three initial headless failures came from the runner's `SDL_AUDIODRIVER=dummy`
overriding each test's own `SDL_SetHint`; removing that runner override passes all
three. The flare/energy GPU fixtures directly inserted `CObjectParticleFX` without
an ID; they now provide the identity normally assigned by `ObjectParticles::publish`.
Receiver validation was not weakened and production recipes are unchanged.

`gpu.polygrid` still fails its original synthetic exact-pixel check (red 2, green 12,
black 2, clear 0, two red runs). The pixel stage extracted verbatim from `a82b7855`
reproduces those exact counts on the unchanged GPU engine, before any GameSession or
scene-adapter call. This is not a complete baseline game rebuild. Its private
CityHub GPU leg is consequently not reached; it remains an explicit coverage gap.
The failure was also [previously recorded](tester_feedback_2026_10_26.md#verification).
It is left unchanged rather than altering the renderer/sampler in this lot.

Additional checks:

- Clang ASan/UBSan server-only build and 22 selected network boundary tests, plus
  180 retail ticks each in SubwayA and CityHub with effect/audio publication.
- Inspector-free GCC Release build; native Vulkan movement and scene-explosion
  smoke runs, including unload/re-entry and detached audio.
- ASan/UBSan dedicated `multi/bb_crypt`, zero clients, 60 ticks with unavailable
  SDL video/audio drivers. It reported 263 discarded ticks under sanitizer/build
  load; this is correctness evidence, **not** a 60 Hz performance qualification.
- Engine-core-only effects receiver links without GameSession, ServerAdapter,
  Objects or server-shell symbols. Mutable particle/surface arrays are owned by
  the receiver, with fog geometry independently copied. Tests retain and consume
  state after source destruction and exercise atomic refusal of nine bad frames.
- Formatting, source hashes, local documentation links and 6,210 protected-file
  hashes checked, including all 39 pre-existing PBR files and earlier evidence.

The existing Ninja build reported `premature end of file; recovering`. A second
terminal build started in the same `build/dev` tree during final linking, producing
transient truncated-archive/link failures. The overlapping build is not counted
as a passing build; final selected objects/archives are copied into an isolated validation tree,
checked for stable copy size/timestamp, valid ELF objects and complete nonempty
archive members, then explicitly relinked there before validation. The two fixed
include users are recompiled in that isolated tree. The first isolated link pass
exhausted the 16 GiB `/tmp` tmpfs with debug executables; those owned temporary
executables were removed and the final pass retains the same compiled code while
omitting DWARF debug sections (`--strip-debug`). ASan/UBSan and Release validation
remain in their original separate build trees. Two tests had relied on a transitive `<algorithm>`
include removed by this extraction; they now include it directly. An earlier
broad build was interrupted deliberately after the flashlight frame was added to
avoid mixing old and new layouts.

Evidence: [headless run](evidence/m9_client_effects_headless_2026-10-30.txt),
[headless rerun](evidence/m9_client_effects_headless_retry_2026-10-30.txt),
[GPU run](evidence/m9_client_effects_gpu_2026-10-30.txt),
[GPU rerun](evidence/m9_client_effects_gpu_retry_2026-10-30.txt),
[PolyGrid baseline pixel stage](evidence/m9_client_effects_polygrid_reference_2026-10-30.txt),
[ASan/UBSan](evidence/m9_client_effects_asan_2026-10-30.txt),
[Release movement](evidence/m9_client_effects_release_movement_2026-10-30.txt),
[Release effects](evidence/m9_client_effects_release_explosions_2026-10-30.txt),
[dedicated](evidence/m9_client_effects_dedicated_2026-10-30.txt).

A pre-change 240-frame retail trace (120 SubwayA + 120 CityHub) was built and run
before changing the boundary. It includes ordinary and effect model/sprite/PolyGrid
geometry and draw transforms/counts, lights/fog, explicit explosion recipes and
server RNG draws. SHA-256:
`87b189ac5bbe44c4a835e58d04bd6579806286c0b29cc939aab4a55c4785aae0`.
The [baseline trace](evidence/m9_client_effects_trajectory_2026-10-30.txt) and
[oracle source](evidence/m9_client_effects_oracle_2026-10-30.cpp) are recorded here;
the final implementation reproduces it byte-for-byte. Audio, CMoveMgr/environment
and camera/HUD also match all four previous-lot traces byte-for-byte.
This is not a complete raster/alpha or campaign oracle; separate effect/GPU tests
exercise presentation details.

## Reproduction

Run builds in separate trees without another build writing to the same tree.
`BLOOD2_TEST_DATA_DIR` points to an external retail installation for private tests.
The selected test names and final exit codes are in the validation evidence.
[Headless names](evidence/m9_client_effects_headless_names_2026-10-30.txt) and
[GPU names](evidence/m9_client_effects_gpu_names_2026-10-30.txt) can be passed to
`ctest --tests-from-file` after building their targets; run the GPU list serially.

```sh
cmake --build build/release --target blood2_app -j2
cmake --build build/server-clang-sanitizers --target blood2_server network_effects_receiver_tests network_effects_local_tests -j4
ctest --test-dir build/server-clang-sanitizers -R '^network\.(effects_|audio_|movement_|environment_|collision_|view_(boundary|lifetime|rays))' --output-on-failure -j4
build/server-clang-sanitizers/tests/network/network_effects_local_tests private "$BLOOD2_TEST_DATA_DIR"
SDL_AUDIODRIVER=dummy build/release/blood2 --config build/dev/tests/empty.ini --data-dir "$BLOOD2_TEST_DATA_DIR" --movement-smoke
SDL_AUDIODRIVER=dummy build/release/blood2 --config build/dev/tests/empty.ini --data-dir "$BLOOD2_TEST_DATA_DIR" --scene-explosions-smoke
SDL_AUDIODRIVER=unavailable SDL_VIDEODRIVER=unavailable build/server-clang-sanitizers/blood2-server --data-dir "$BLOOD2_TEST_DATA_DIR" --world multi/bb_crypt --mode bloodbath --port 0 --ticks 60
```

The dedicated log still explicitly rejects player admission. The GPU smoke log
contains older status wording about unavailable illumination; evaluated scene
lights are now handled by the existing scene-light path, unchanged by this lot.

## Limits

Only local Linux validation is performed here. No LAN, remote effect delivery,
late-join playback, impaired-network session, human audible/visual comparison,
Windows/macOS/mobile/Quest or XR hardware qualification is claimed. No simulator
or emulator is used. The existing CityHub geometry-count test failure and
M2-GPU-001 remain separate. Existing PBR files and all earlier evidence/captures
are protected by SHA-256 and excluded from this commit.
