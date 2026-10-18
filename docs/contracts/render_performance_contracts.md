# Presentation performance (phases 0–4)

This continuation reduces CPU submission and GPU pass cost of the existing SDL_GPU
presentation without changing simulation or original object rules. Phases 0–1 keep
the shaders unchanged; phase 2 adds GPU skinning (one new vertex shader) and
frame-command-buffer uploads. It is replacement-engine work: no retail renderer
parity is claimed. Remaining work is listed at the end. Reference
corroboration is recorded in the [source ledger](../implementation/sources/render_performance_sources.json).

## Scene presenter and passes

`ScenePresenter` (`src/app/scene_presenter.*`) owns the read-only presentation
snapshots and renderers used by both the application and `--bench`. `update()`
reads current checked session state exactly as the former inline `main.cpp` code
did, including world-revision rebuilds and the same error-stage prefixes;
`record()` encodes one frame. Neither advances simulation.

All in-world presentation is recorded into **one scene render pass**: sky, opaque
world, opaque models, sorted world alpha, sorted model alpha, explosion models,
PolyGrids, sprites/particles, then view models. This is the former pass order.
The application then records its separate ImGui/HUD color-load pass, so a frame
uses two passes instead of up to ten. The pass clears color and one presenter-owned
D32 depth target; depth is stored with `DONT_CARE` because nothing reads it after
the pass. View models no longer clear depth: the scene draws with viewport depth
`[1/256, 1]` and view models with `[0, 1/256]`, so they still cover everything
drawn before them while keeping their near-plane projection and self-occlusion.

`WorldPipelines` creates the opaque, alpha, sky and sky-alpha pipelines for a color
format together, before any draw. Presenter renderers share one set and record into
the externally owned pass/targets (`WorldRenderer(device, style, shared)`); they
allocate no depth texture. Standalone renderers keep `draw()`, which begins/ends one
pass around `record()` with the previous load/clear semantics and owns its depth.
Validation, culling and alpha ordering happen before a pass is begun, so a failed
record leaves no half-encoded pass; the presenter ends its pass before reporting the
failing stage.

## Batching and redundant state

`WorldMesh::build` groups each DAT model's opaque triangles by material state
(material, sky portal, pan rate) in first-appearance order, preserving polygon order
inside each group. Translucent faces are unchanged: one batch per base-ring polygon,
in authored order, after the opaque groups ([world alpha](world_alpha_contracts.md)).
CityHub's primary model goes from 3,243 batches to 153.

Within one `record()`, a vertex uniform is pushed only when its bytes change and a
texture/sampler is bound only when the texture changes; nothing is assumed across
records because other renderers share the pass and command buffer.

`SpriteView::batch()` runs after sorting. Consecutive sorted draws with the same
texture and opacity are pre-transformed to world space and appended as one range
drawn with an identity transform. Per-entry ranges, draws and entries remain the
unchanged inspection snapshot; only the presenter submits `batched_draws`. Later
LithTech builds world-space particle quads on the CPU in the same spirit.

Each PolyGrid surface is now one translucent batch ordered by its origin among other
surfaces; its cells draw in lattice order. This supersedes the per-cell far-to-near
sort of the [PolyGrid contracts](polygrid_contracts.md); later LithTech also submits
a grid as one indexed list and sorts translucent grids as objects.

## Visibility

Every mesh builder records local bounds per range (`WorldRange::min/max/bounded`):
world models, posed model instances (conservatively from the posed vertex set,
cached with reused geometry), sprite quads, merged sprite ranges and PolyGrids.
`record()` skips draws whose bounding sphere lies outside the 90-degree diagnostic
frustum (near 1, far 65,536, letterboxed aspect). Sky and view-model draws keep their
dedicated projections and are never culled; unbounded ranges are never culled. The
translucent order is computed over the visible draws only. The primary world range
contains the camera and is effectively always drawn; spatial world clustering is
pending. Culling never changes object state or inspection entries.

## Textures

DTX decoding now keeps every authored mip level (`Texture::mip_rgba`, levels
1..n-1) decoded with the shared palette and 4-bit masks, and GPU textures receive
the full chain. Samplers keep nearest filtering with point mip selection
(`max_lod` unbounded); the former SDL default limited sampling to mip zero. This
supersedes the mip-zero limitation of the [texture contracts](texture_contracts.md).
Distant surfaces alias less; close surfaces are unchanged. No generated mips are
used, so transparent edges keep authored alpha.

## Measurement and inspection

`blood2 --bench [--world NAME] [--frames N] [--bench-size WxH] [--bench-sweep]`
enters the world like `--view-world`, renders the presenter offscreen without a
window and reports per-stage CPU time, GPU fence completion and counters. Each frame
waits for its fence, so stages are serialized; the first 30 frames (uploads and
pipeline creation) are excluded. `--bench-sweep` rotates only the presented view.
`gpu.bench_01_SubwayA` / `gpu.bench_02_CityHub` keep the mode working; they report
timings and assert nothing about them. The inspector's 3D view shows the last
frame's pass, draw-call and culled-record counts and CPU stage times. A draw skipped in
both an opaque and an alpha record counts once per record.

The `dev` preset (RelWithDebInfo with inspector) is the recommended playtest build.
Measured before this change, the Debug build took 17.5 ms mean / 30 ms p95 per
CityHub frame on the same machine, mostly unoptimized simulation and CPU model
posing; that is a build-type cost, not a renderer limit.

## Verification

Apple M5, Metal, 3440x1440 offscreen, RelWithDebInfo, 600 measured frames, serialized
with a fence. "Before" replays the previous `main.cpp` sequence against unmodified
libraries built from commit 3b5882d.

| CityHub | Before | After |
| --- | --- | --- |
| Scene render passes | 7 | 1 |
| SDL draw calls | 4,275 | 235 (364 culled draw records) |
| CPU uploads + encode + submit | 0.85 ms | 0.12 ms |
| GPU completion | 1.00 ms | 0.56 ms |
| Total serialized frame, mean / p95 | 3.25 / 4.88 ms | 2.03 / 3.65 ms |

SubwayA: 1,625 → 177 draw calls, encode 0.33 → 0.07 ms, GPU 0.49 → 0.37 ms, total
1.24 → 0.88 ms. With a 360-degree yaw sweep, CityHub draw calls now vary with the view
(299 mean, 536 max) instead of a constant 4,275.

On this Mac, headless tests give the same six pre-existing failures as the baseline
(`world.destination_private`, `player_controls.combat`/`corpse_clearance`,
`player_voice.pickup`, `train_session.private_30`, `music_assets.catalog`); GPU tests
pass 96/98 with the same two pre-existing failures (`gpu.mouse_combat`,
`gpu.corpse_clearance`), whose output is identical in the baseline. Standalone
renderer readbacks are pixel-identical to the baseline before the mip change; with
authored mips, differences are confined to minified distant textures. Application
captures otherwise differ only in inspector text lines, plus `--view-world` captures
that already differ between two baseline runs because they follow wall-clock time.

The macOS GPU test suite previously crashed in 64 of 96 tests: Metal debug devices
query an uninitialized sampler property ID, and SDL's property query validated its
output argument instead of the lookup result. `cmake/PatchSdlProperties.cmake` now
fixes the query the same way as the existing assignment fix; upstream SDL main still
has both defects. `platform.destroyed_properties` covers the absent-ID reads.
Linux/NVIDIA and native D3D12 were not re-run for this change.

## Phase 2: GPU skinning and frame uploads

The presenter's model views run in `ModelView::gpu_skinning` mode. For each shared
immutable ABC asset, `build_skinned_vertices` validates node, face, UV and packed
deformation references once and builds a static corner stream: authored node-local
position, face-corner UV, the vertex's node slot plus the face's three corner slots,
and the last packed-deformation slot writing that vertex (repeated indices keep file
write order). `ModelRenderer` uploads that stream once per asset, keyed by pointer
with the shared-owner check of the prepared cache, and releases it when the owner
expires or the world revision clears. Limits: 256 assets and 2,000,000 static corners.

Each frame, `sample_skin` computes the same interpolated node poses and node-local
deformation as `sample_vertices`, without transforming vertices. The view publishes
one palette entry per node (rotation, translation and a hidden flag) and one
deformation entry per packed slot; limits are 262,144 palette vec4 and 1,000,000
deformation entries per view. `skinned.vert` applies the bone transform, object
scale and instance transform in the CPU path's order. A face with any hidden
corner node collapses to a degenerate triangle, exactly matching the CPU face
omission. Entry triangle/hidden counts, pose failures, missing skins and magenta
fallback remain as before; borrowed (non-shared) models are refused in this mode
with an inspectable error. Instance bounds are conservative spheres around posed
node boxes that include each node's full packed-deformation range; culling,
translucency sorting and close-view projection follow the world renderer.

The CPU path, the prepared-geometry cache and `WorldRenderer` model drawing are
unchanged and remain what standalone tests and inspection use. `gpu.model_skinning`
renders a synthetic hierarchy with rotations, repeated packed deformation,
non-uniform scale, an instance transform and every node-hide combination through
both paths: all pixels are identical. `gpu.model_skinning_private` does the same for
all 54 distinct SubwayA and CityHub assets at three clip times: 821,148 covered
pixels, none differing.

Renderer and model uploads are now copy passes recorded in the frame's command buffer
before the scene pass (`prepare(..., frame)`), so a presented frame is one submission
apart from the HUD's change-only texture upload. Standalone `prepare()` calls keep a
dedicated submission.

The skinned shader is the fourth bundled shader; the generator now emits each
shader's reflected storage-buffer count and accepts read-only storage buffers. All
eight artifacts were regenerated with glslangValidator 16.3.0 and the pinned
SDL_shadercross: existing SPIR-V and MSL are unchanged apart from the version comment.

Spatial clustering of the primary world was measured before implementation. Over a
360-degree sweep at 3440x1440, 21% (CityHub, 18,112 triangles) and 27% (SubwayA,
6,191 triangles) of primary-world triangles lie in the frustum. Out-of-view triangles
cost only vertex work, which the GPU clips before rasterization, while splitting the
world into clusters multiplies material batches and draw calls. The v56 leaf/PVS
lists are also unverified: later LithTech only skips them while loading. Clustering
is therefore deferred until lighting or shadow passes need per-region geometry.

| CityHub, 3440x1440 | Phase 1 | Phase 2 |
| --- | --- | --- |
| Model CPU stage | 0.38 ms | 0.09 ms |
| Model uploads per frame | ~3.3 MB of posed vertices | 67 KB of palettes/deformation |
| CPU uploads + encode + submit | 0.12 ms | 0.06 ms |
| Total serialized frame, mean / p95 | 2.03 / 3.65 ms | 1.68 / 3.37 ms |

SubwayA: total 0.88 → 0.67 ms. Simulation is now the largest CPU stage (CityHub
0.93 ms mean, 2.3 ms p95), mostly replacement collision sweeps.

## Phase 3: GPU resource registry

`GpuRegistry` (`src/engine/gpu_registry.*`) is the single owner of presentation
textures, buffers and samplers. Renderers hold `TextureHandle`/`BufferHandle`
values: index plus generation, with index 0 null. Releasing a slot or clearing a
world's textures changes its generation, so a stale handle resolves to nullptr
instead of a recycled object. SDL still performs deferred GPU destruction.

Material textures are content-addressed: the key is the resource path plus a hash
of dimensions and every decoded level, so equal resources are uploaded once and
shared by the world, model, explosion, PolyGrid and sprite renderers, while a
different payload under the same name stays distinct. Failed and animated-sprite
entries still have no texture, and an empty list still gets white.
`refresh_textures` keeps the handles of an unchanged, still valid material prefix
(views only append materials until `clear()`), so a new effect texture uploads
only itself instead of the renderer's whole material set. Two shared samplers
(repeat, clamp) replace one sampler per renderer.

Uploads are staged on the CPU and recorded at `flush()` as one copy pass in the
frame's command buffer. The presenter owns one registry, flushes it once after all
renderers are prepared and clears its material textures at each world revision.
Standalone renderers (tests, tools) own a private registry and flush it inside
`prepare()`, keeping their previous behavior. Static skinned corner streams are
shared by key and owner identity across both model renderers and released when the
owning ABC asset expires; presenter depth is an explicit registry target.
Inspection and `--bench` report textures, bytes, shared hits, buffers, shared assets
and staged bytes. The HUD and capture keep their own small GPU objects.

`gpu.registry` checks stale handles after release, world clears and slot reuse,
content sharing and same-name payload separation, null/white material rules,
invalid payloads, incremental tail acquisition, buffer growth, staging bounds,
owner-scoped shared assets and target survival. CityHub registers 276 textures
(23 MB with mips, 3 cross-view hits), 52 shared assets and 57 buffers; frame time
is unchanged (1.65 ms mean at 3440x1440).

## Phase 4: HLSL shader sources

The four shaders (`world.vert`, `skinned.vert`, `world.frag`, `sprite.frag`) are now
HLSL, the single source compiled by the pinned SDL_shadercross with DXC into the
checked-in SPIR-V and MSL; see the [shader contracts](metal_shader_contracts.md).
They use SDL_GPU's register spaces, so the same sources can later emit DXIL for
D3D12 on a host with DXC's signing library; no DXIL is generated or advertised yet.
Math, operation order, uniform layouts, attribute and varying locations and resource
bindings are unchanged from the GLSL versions; reflection confirms set 0 storage,
set 1 vertex uniforms, set 2 fragment textures/samplers and set 3 fragment uniforms.

## Pending phases

- DXIL artifacts and a D3D12 path, once they can be signed and tested on Windows.
- Instanced model draws, spatial world clusters and PVS when lighting/shadow passes
  need them; simulation and collision cost.

The [baked lighting continuation](baked_lighting_contracts.md) adds model light sampling to the models stage. Samples are cached per object, so CityHub's frame time and model stage are within measurement noise. `--bench --capture FILE.png` saves the last measured frame.

The [lightmap continuation](lightmap_contracts.md) splits opaque batches by lightmap page (CityHub: 238 → 296 draw calls) and adds one bilinear texture read per world pixel. CityHub at 3440x1440 measures 1.74 ms per frame (GPU 0.58 ms).
