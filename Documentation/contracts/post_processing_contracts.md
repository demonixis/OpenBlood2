# Post-processing (M6)

Options > Display adds four replacement rows after NORMAL MAPS:

| Row (setting) | Values | Applies to |
|---|---|---|
| `TONEMAP` (19) | OFF / NEUTRAL / FILMIC | Enhanced renderer (disabled with ORIGINAL) |
| `BLOOM` (18) | ON / OFF | Enhanced renderer (disabled with ORIGINAL) |
| `AMBIENT OCCLUSION` (20) | OFF / LOW / HIGH | Enhanced renderer (disabled with ORIGINAL) |
| `ANTI-ALIASING` (17) | OFF / FXAA / MSAA 2X / MSAA 4X | Both renderers |

The defaults are anti-aliasing off, bloom on, the neutral tonemap and HIGH ambient occlusion.
The original renderer therefore keeps its byte-identical frame by default; the enhanced renderer
gets an HDR scene, ambient occlusion, bloom and the neutral tonemap. Everything here is
replacement rendering: the 1998 game has no post-processing. The first increment covered the
HDR target, bloom, tonemapping and anti-aliasing of roadmap M6; the second adds GTAO. SSR remains.
The [source ledger](../implementation/sources/post_processing_sources.json) lists the published
techniques.

## Frame structure

`PostProcess` (`engine/post_process.*`, owned by `ScenePresenter`) plans each desktop frame
before the scene pass and records the passes after it. The HUD, menus and inspector are drawn
afterwards, so no effect touches them.

| Settings | Scene pass | After the scene pass |
|---|---|---|
| Original, AA off; enhanced with every effect off | Output (swapchain) | Nothing: the frame of the earlier releases. |
| MSAA only | Multisampled colour and depth, resolved into the output | Nothing |
| FXAA, LDR scene | Intermediate in the output format | FXAA into the output |
| Enhanced with bloom, a tonemap or ambient occlusion | RGBA16F scene, plus the auxiliary targets with bloom or occlusion | Occlusion, bloom chain, composite into the output (or into an intermediate for FXAA), FXAA |

- **Auxiliary targets.** With bloom or ambient occlusion every scene pipeline gets two more colour
  targets: the glow (RGBA16F, alpha holds the occlusion mask, see [Bloom](#bloom) and
  [Ambient occlusion](#ambient-occlusion)) and an R32F copy of the window depth.
- **MSAA.** Every multisampled colour attachment resolves with `SDL_GPU_STOREOP_RESOLVE` at the
  end of the scene pass. SDL_GPU cannot resolve or sample a multisampled depth buffer, which is why
  occlusion reads the depth copy instead. The sample count is the highest one up to the request
  that the scene format, `D32_FLOAT` and (with auxiliary targets) the glow and depth-copy formats
  all support; `PostStats` keeps both. The shadow-tile pass stays single-sampled.
- **Targets.** Textures live in the presentation `GpuRegistry`. They are recreated when the
  output size or format or the plan changes, and all are released when the plan returns to the
  direct pass. The post pipelines (bloom down/up, composite, FXAA, occlusion depth, GTAO, blur)
  are created together for each output format, and the scene `WorldPipelines` are recreated with
  the auxiliary targets and sample count, so nothing is compiled lazily.
- **XR.** `record_eyes` keeps the single-sampled pass into the eye layers: post-processing is
  desktop only in this increment, and the settings are ignored in a headset session.

## HDR scene

With the enhanced renderer and bloom, a tonemap or ambient occlusion, every scene pipeline (sky, sprites,
PolyGrids, models and world faces) draws into an RGBA16F target. Values stay in the renderer's
display space but are no longer clamped, so specular highlights and emission above white
survive until the composite. The composite decodes with the sRGB transfer function (extended
above 1), applies ambient occlusion and adds bloom in linear light, tonemaps and encodes for the
UNORM output.

A float target also stops clamping a translucent draw's colour before blending. Enhanced
translucent faces raise their coverage to carry reflections, so their straight colour can exceed
1: with the HDR target those highlights keep their energy (about 0.2% of the CityHub start
frame, the chrome pistol and a metal bar, is brighter than the clamped frame).

## Bloom

Bloom is driven by the manifest emission (roadmap 5.2), not by a luminance threshold:

- **Glow target.** The enhanced shaders (`-DAUX_TARGETS` variants) write to the glow target the
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

## Ambient occlusion

Ground-truth ambient occlusion (GTAO, Jimenez et al. 2016) darkens corners and contacts that the
baked light does not resolve. It is replacement shading, tuned to stay subtle.

- **Inputs.** The depth copy (target 2) is written like the depth buffer: by opaque, non-sky
  pipelines only, so translucent faces never occlude. The glow alpha is the share of the pixel that
  receives occlusion. Opaque enhanced faces write 1. The DAT's fully bright faces and the unlit,
  flat and `FLAG_NOLIGHT` models write 0, and translucent draws keep only what they leave
  uncovered (alpha blend `ZERO, ONE_MINUS_SRC_ALPHA`). Water and smoke therefore do not inherit the
  occlusion of the floor behind them.
- **Half size.** A first pass converts the depth copy to linear view depth at half size, taking the
  top-left pixel of each 2×2 block, so no depths are mixed. GTAO runs on that buffer:
  - per texel, a set of screen-space slices rotated by interleaved gradient noise;
  - on each side of a slice, the highest horizon within 48 world units (linear falloff over the
    outer 60%), its step distances squared and jittered;
  - the cosine-weighted visibility integrated analytically around the normal projected into the
    slice, the normal rebuilt from the nearer depth neighbours;
  - the result raised to the power 1.5.
  LOW uses 2 slices × 4 steps per side, HIGH 4 × 6. The screen radius is bounded to 96 half-size
  pixels, so near walls keep the taps local.
- **Blur and upsample.** A separable 9-tap blur (Gaussian weights cut by a 4% relative depth
  difference) runs across and down at half size. The composite then upsamples bilaterally: the four
  nearest texels in bilinear weights, divided by their relative depth difference to the pixel.
- **Application.** In linear light the composite keeps the glow and darkens the rest of the pixel:
  `c = g + (c − g) · lerp(1, ao, mask)`. Emission, the view models (the reserved near depth slice)
  and the background never darken, and the view models occlude nothing.
- **Limits.** Screen-space only: occluders off screen or behind the first surface are unknown,
  thin foreground objects darken the wall behind them within the radius, and with MSAA the resolved
  depth copy averages depths along silhouettes.

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

- `preferences.cfg` version 7 adds `antialiasing` (0-3), `bloom` (0/1) and `tonemap` (0-2);
  version 8 adds `ambient_occlusion` (0-2). Older versions load the defaults (0, 1, 1, 2); writes
  produce version 8.
- The bench and direct paths take `--aa off|fxaa|msaa2|msaa4`, `--no-bloom`,
  `--tonemap off|neutral|filmic` and `--ao off|low|high`, with the same defaults.
- The inspector's world section and the bench log report the last frame's plan: direct or HDR
  scene, MSAA samples (and the request), FXAA, bloom levels, tonemap, ambient occlusion, passes
  and targets.

## Shaders

Fourteen shaders are added:

- `world_aux_frag`, `sprite_aux_frag` and the four `pbr_*_aux_frag` variants (`-DAUX_TARGETS`,
  outputs 1 and 2);
- `post.vert` (a buffer-free triangle), `bloom_down.frag`, `bloom_up.frag`, `composite.frag`,
  `fxaa.frag`, `ao_depth.frag`, `gtao.frag` and `ao_blur.frag`.

All 50 artifacts are regenerated together; the 22 earlier ones are byte-identical after
regeneration.

## Cost

Headless bench, 1920×1080, Apple M5 Metal, CityHub start, mean GPU completion wait:

| Renderer and settings | GPU time | Passes after the scene |
|---|---|---|
| Original, AA off (direct) | 0.57 ms | 0 |
| Original, MSAA 4X | 0.60 ms | 0 (resolve in the scene pass) |
| Original, FXAA | 0.99 ms | 1 |
| Enhanced STANDARD, every effect off (direct) | 1.30 ms | 0 |
| STANDARD, NEUTRAL only | 1.43 ms | 1 |
| STANDARD, NEUTRAL and bloom | 1.94 ms | 12 (6 down, 5 up, composite) |
| STANDARD, NEUTRAL, bloom and occlusion LOW | 2.47 ms | 16 (+ depth, GTAO, two blurs) |
| STANDARD, defaults (occlusion HIGH) | 2.86 ms | 16 |
| STANDARD, defaults with MSAA 4X | 3.00 ms | 16 |
| STANDARD, defaults with FXAA | 3.34 ms | 17 |
| HIGH, normal maps, FILMIC, bloom, occlusion HIGH, MSAA 4X | 3.60 ms | 16 |

MSAA is cheap on the tile-based Apple GPU; desktop GPUs will pay more for the multisampled
RGBA16F targets. The auxiliary targets cost bandwidth whenever bloom or occlusion is on. A first
full-size GTAO cost about 4.4 ms (HIGH); half size, the half-size blur and the gathered upsample
brought it below 1 ms.

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
  - ambient occlusion, at LOW and HIGH, never brightens, darkens more than 2% of the pixels by a
    mean below 8 levels, and runs with bloom, FILMIC, MSAA and FXAA together;
  - the plain settings restore the direct frame and release every target.
- `gpu.surface` measures the enhanced shading with post-processing off.
- `world_shaders.formats` (auxiliary and post shaders, resources, MSL `[[color(1)]]`,
  `[[color(2)]]` and `[[vertex_id]]`), `menu` (the four rows, cycling, disabled rows) and
  `menu_preferences` (versions 7 and 8, migration, ranges).
- Not verified: Vulkan (Linux, Windows), MSAA support on other GPUs and the Quest.

## Remaining (M6)

SSR, render scaling and FSR1 for both renderers, post-processing in XR, and bloom for the DAT's
fully bright faces if the user wants it.
