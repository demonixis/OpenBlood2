# Post-processing (M6, first increment)

Options > Display adds three replacement rows after NORMAL MAPS:

| Row (setting) | Values | Applies to |
|---|---|---|
| `TONEMAP` (19) | OFF / NEUTRAL / FILMIC | Enhanced renderer (disabled with ORIGINAL) |
| `BLOOM` (18) | ON / OFF | Enhanced renderer (disabled with ORIGINAL) |
| `ANTI-ALIASING` (17) | OFF / FXAA / MSAA 2X / MSAA 4X | Both renderers |

The defaults are anti-aliasing off, bloom on and the neutral tonemap. The original renderer
therefore keeps its byte-identical frame by default; the enhanced renderer gets an HDR scene,
bloom and the neutral tonemap. Everything here is replacement rendering: the 1998 game has no
post-processing. This increment covers the HDR target, bloom, tonemapping and anti-aliasing of
roadmap M6; GTAO, SSR and per-effect quality levels remain. The
[source ledger](../implementation/sources/post_processing_sources.json) lists the published
techniques.

## Frame structure

`PostProcess` (`engine/post_process.*`, owned by `ScenePresenter`) plans each desktop frame
before the scene pass and records the passes after it. The HUD, menus and inspector are drawn
afterwards, so no effect touches them.

| Settings | Scene pass | After the scene pass |
|---|---|---|
| Original, AA off; enhanced with bloom, tonemap and AA off | Output (swapchain) | Nothing: the frame of the earlier releases. |
| MSAA only | Multisampled colour and depth, resolved into the output | Nothing |
| FXAA, LDR scene | Intermediate in the output format | FXAA into the output |
| Enhanced with bloom or a tonemap | RGBA16F scene (+ RGBA16F glow with bloom) | Bloom chain, composite into the output (or into an intermediate for FXAA), FXAA |

- **MSAA.** Every multisampled attachment (scene, glow, depth) resolves with
  `SDL_GPU_STOREOP_RESOLVE` at the end of the scene pass. The sample count is the highest one up
  to the request that the scene format, `D32_FLOAT` and (with bloom) the glow format all support;
  `PostStats` keeps both. The shadow-tile pass stays single-sampled.
- **Targets.** Textures live in the presentation `GpuRegistry`. They are recreated when the
  output size or format or the plan changes, and all are released when the plan returns to the
  direct pass. The post pipelines (bloom down, bloom up, composite, FXAA) are created together
  for each output format, and the scene `WorldPipelines` are recreated with the glow target and
  sample count, so nothing is compiled lazily.
- **XR.** `record_eyes` keeps the single-sampled pass into the eye layers: post-processing is
  desktop only in this increment, and the settings are ignored in a headset session.

## HDR scene

With the enhanced renderer and bloom or a tonemap, every scene pipeline (sky, sprites,
PolyGrids, models and world faces) draws into an RGBA16F target. Values stay in the renderer's
display space but are no longer clamped, so specular highlights and emission above white
survive until the composite. The composite decodes with the sRGB transfer function (extended
above 1), adds bloom in linear light, tonemaps and encodes for the UNORM output.

A float target also stops clamping a translucent draw's colour before blending. Enhanced
translucent faces raise their coverage to carry reflections, so their straight colour can exceed
1: with the HDR target those highlights keep their energy (about 0.2% of the CityHub start
frame, the chrome pistol and a metal bar, is brighter than the clamped frame).

## Bloom

Bloom is driven by the manifest emission (roadmap 5.2), not by a luminance threshold:

- **Glow target.** Bloom passes give every scene pipeline a second colour target
  (`scene_glow_format`, RGBA16F). The enhanced shaders (`-DGLOW_TARGET` variants) write the
  linear light that emission adds to the pixel, `lin(c) − lin(c − e)`, premultiplied by coverage
  on translucent faces. The original shaders write no glow: opaque draws clear the glow they hide,
  and translucent draws (sprites, FX, original alpha faces) attenuate it by their coverage with the
  premultiplied blend `ONE, ONE_MINUS_SRC_ALPHA`.
- **Not blooming.** The DAT's fully bright faces, specular highlights and every surface without a
  manifest emissive class. Without emission the glow stays empty and bloom adds nothing
  (`gpu.post`).
- **Chain** (Jimenez 2014). The glow is downsampled to half resolution and then halved while both
  sides keep two texels, at most 6 levels: 13 bilinear taps per texel, the first pass with Karis'
  1/(1 + luma) box weighting against flicker. Each coarser level is then added to the finer one
  through a 3×3 tent. The composite samples level 0 through another tent and adds it scaled by
  `bloom_strength / levels` (1.5, replacement).

## Tonemaps

| Tonemap | Curve |
|---|---|
| OFF | Saturate, as the UNORM framebuffer did. |
| NEUTRAL | Khronos PBR Neutral's shoulder (compression start 0.76, desaturation 0.15) without its toe offset, so every channel below 0.76 linear (about 225 in display values) keeps its value. |
| FILMIC | Hill's fit of the ACES RRT and ODT, with its sRGB/working-space matrices, after an exposure of 2.2. |

The FILMIC exposure is a replacement value chosen on the CityHub and SubwayA captures: with the
1.462 that keeps mid-grey at 0.18, 78% of the CityHub start moved more than 16 levels darker. At
2.2, mid-grey maps to 0.25, shadows deepen and highlights saturate, and dark areas stay readable.
NEUTRAL was chosen as the default because it leaves the original look in place.

## Anti-aliasing

- **MSAA 2X/4X** smooths geometric edges only. Point-sampled textures stay sharp, which keeps the
  original pixel look. No alpha-to-coverage: alpha-blended sprite and texture edges are unchanged.
- **FXAA** follows the FXAA 3.11 quality algorithm (Lottes 2011), written for this project: edge
  threshold 0.125 (minimum 0.0312) on display-space luma, 10 search steps (1, 1.5, 2 ×6, 4, 8) and
  subpixel blend 0.75. It runs last, on display values, after the composite. Pixels under the
  edge threshold keep their value (`gpu.post` checks every one of them).

## Settings

- `preferences.cfg` version 7 adds `antialiasing` (0-3), `bloom` (0/1) and `tonemap` (0-2).
  Versions 1-6 load with the defaults (0, 1, 1); writes produce version 7.
- The bench and direct paths take `--aa off|fxaa|msaa2|msaa4`, `--no-bloom` and
  `--tonemap off|neutral|filmic`, with the same defaults.
- The inspector's world section and the bench log report the last frame's plan: direct or HDR
  scene, MSAA samples (and the request), FXAA, bloom levels, tonemap, passes and targets.

## Shaders

Eleven shaders are added: `world_glow_frag`, `sprite_glow_frag` and the four `pbr_*_glow_frag`
variants (`-DGLOW_TARGET`), and `post.vert` (a buffer-free triangle), `bloom_down.frag`,
`bloom_up.frag`, `composite.frag` and `fxaa.frag`. All 44 artifacts are regenerated together;
the 22 existing ones are byte-identical after regeneration.

## Cost

Headless bench, 1920×1080, Apple M5 Metal, CityHub start, mean GPU completion wait:

| Renderer and settings | GPU time | Passes after the scene |
|---|---|---|
| Original, AA off (direct) | 0.67 ms | 0 |
| Original, MSAA 4X | 0.74 ms | 0 (resolve in the scene pass) |
| Original, FXAA | 1.22 ms | 1 |
| Enhanced STANDARD, bloom and tonemap off (direct) | 1.43 ms | 0 |
| STANDARD, NEUTRAL without bloom | 1.75 ms | 1 |
| STANDARD, NEUTRAL with bloom (default) | 2.43 ms | 12 (6 down, 5 up, composite) |
| STANDARD, default with MSAA 4X | 2.59 ms | 12 |
| STANDARD, default with FXAA | 2.93 ms | 13 |
| HIGH, normal maps, FILMIC, bloom, MSAA 4X | 3.21 ms | 12 |

MSAA is cheap on the tile-based Apple GPU; desktop GPUs will pay more for the multisampled
RGBA16F targets. Bloom's cost is mostly the full-resolution glow target and its first
downsample.

## Verification

- `gpu.post` (CityHub and SubwayA, 960×540):
  - the original renderer's frame is identical with any bloom or tonemap setting and after
    returning from MSAA and FXAA;
  - MSAA and FXAA move fewer than 3% of pixels, and FXAA keeps every pixel under its edge
    threshold;
  - NEUTRAL keeps every pixel below 220 within one level and never brightens, apart from the
    unclamped translucent highlights (fewer than 1%);
  - bloom never darkens, brightens the emissive areas, and adds nothing without emission;
  - FILMIC differs from NEUTRAL; MSAA with bloom and FXAA after the composite run with the expected
    targets and passes;
  - the plain settings restore the direct frame and release every target.
- `gpu.surface` measures the enhanced shading with post-processing off.
- `world_shaders.formats` (glow and post shaders, resources, MSL `[[color(1)]]` and
  `[[vertex_id]]`), `menu` (the three rows, cycling, disabled rows) and `menu_preferences`
  (version 7, migration, ranges).
- Not verified: Vulkan (Linux, Windows), MSAA support on other GPUs and the Quest.

## Remaining (M6)

GTAO and SSR (they need a depth/normal prepass or MRT inputs), a quality setting per effect,
render scaling and FSR1 for both renderers, and bloom for the DAT's fully bright faces if the
user wants it.
