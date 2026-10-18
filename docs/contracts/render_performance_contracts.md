# Presentation performance (phases 0–1)

This continuation reduces CPU submission and GPU pass cost of the existing SDL_GPU
presentation without changing simulation, original object rules or shader sources.
It is replacement-engine work: no retail renderer parity is claimed. Remaining
architecture work (GPU skinning, BSP/PVS world clusters, frame-command-buffer
uploads, resource handles, HLSL shaders) is listed at the end. Reference
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

## Pending phases

- GPU skinning: static indexed model buffers and per-instance node palettes instead
  of per-frame CPU vertex expansion (later LithTech uses hardware bone sets).
- World clustering by BSP leaves/PVS from the DAT tail, with per-cluster material
  ranges, so the primary world is culled spatially.
- Uploads in the frame command buffer and a GPU resource registry with generational
  handles shared by all renderers (textures are still duplicated per renderer).
- HLSL shader sources compiled by SDL_shadercross to SPIR-V, MSL and DXIL.
