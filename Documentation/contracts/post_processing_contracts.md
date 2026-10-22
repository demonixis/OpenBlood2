# Post-processing (M6)

Options > Display adds five replacement rows after NORMAL MAPS:

| Row (setting) | Values | Applies to |
|---|---|---|
| `TONEMAP` (19) | OFF / NEUTRAL / FILMIC | Enhanced renderer (disabled with ORIGINAL) |
| `BLOOM` (18) | ON / OFF | Enhanced renderer (disabled with ORIGINAL) |
| `AMBIENT OCCLUSION` (20) | OFF / LOW / HIGH | Enhanced renderer (disabled with ORIGINAL) |
| `REFLECTIONS` (21) | OFF / LOW / HIGH | Enhanced renderer (disabled with ORIGINAL) |
| `ANTI-ALIASING` (17) | OFF / FXAA / MSAA 2X / MSAA 4X | Both renderers |

The defaults are anti-aliasing off, bloom on, the neutral tonemap and HIGH ambient occlusion and
reflections. The original renderer therefore keeps its byte-identical frame by default; the
enhanced renderer gets an HDR scene, reflections, ambient occlusion, bloom and the neutral
tonemap. Everything here is replacement rendering: the 1998 game has no post-processing. The
increments covered the HDR target, bloom, tonemapping and anti-aliasing of roadmap M6, then GTAO,
then screen-space reflections.
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
| Enhanced with bloom, a tonemap, occlusion or reflections | RGBA16F scene, plus the auxiliary targets with bloom, occlusion or reflections | Occlusion, reflections, bloom chain, composite into the output (or into an intermediate for FXAA), FXAA |

- **Auxiliary targets.** With bloom, ambient occlusion or reflections every scene pipeline gets two
  more colour targets: the glow (RGBA16F, alpha holds the occlusion mask, see [Bloom](#bloom) and
  [Ambient occlusion](#ambient-occlusion)) and an R32F copy of the window depth. Reflections add two
  RGBA16F targets (see [Screen-space reflections](#screen-space-reflections)), five in all;
  SDL_GPU accepts eight.
- **MSAA.** Every multisampled colour attachment resolves with `SDL_GPU_STOREOP_RESOLVE` at the
  end of the scene pass. SDL_GPU cannot resolve or sample a multisampled depth buffer, which is why
  occlusion reads the depth copy instead. The sample count is the highest one up to the request
  that the scene format, `D32_FLOAT` and the auxiliary and reflection formats in use all support;
  `PostStats` keeps both. The shadow-tile pass stays single-sampled.
- **Targets.** Textures live in the presentation `GpuRegistry`. They are recreated when the
  output size or format or the plan changes, and all are released when the plan returns to the
  direct pass. The post pipelines (bloom down/up, composite, FXAA, half-size depth, GTAO and its
  blur, reflection trace and blur) are created together for each output format, and the scene
  `WorldPipelines` are recreated with the auxiliary and reflection targets and sample count, so
  nothing is compiled lazily.
- **XR.** `record_eyes` keeps the single-sampled pass into the eye layers: post-processing is
  desktop only in this increment, and the settings are ignored in a headset session.

## HDR scene

With the enhanced renderer and bloom, a tonemap, ambient occlusion or reflections, every scene pipeline (sky, sprites,
PolyGrids, models and world faces) draws into an RGBA16F target. Values stay in the renderer's
display space but are no longer clamped, so specular highlights and emission above white
survive until the composite. The composite decodes with the sRGB transfer function (extended
above 1), applies ambient occlusion and adds bloom in linear light, tonemaps and encodes for the
UNORM output; reflections are applied before the decode, in display space, where the shaders
added the term they replace.

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
  top-left pixel of each 2×2 block, so no depths are mixed. It also builds the
  [depth pyramid](#depth-pyramid-hi-z) shared with reflections. GTAO runs on it:
  - per texel, a set of screen-space slices rotated by interleaved gradient noise;
  - on each side of a slice, the highest horizon within 48 world units (linear falloff over the
    outer 60%), its step distances squared and jittered;
  - the cosine-weighted visibility integrated analytically around the normal projected into the
    slice, the normal rebuilt from the nearer depth neighbours;
  - taps read the pyramid level `floor(log2(distance)) − 2` (XeGTAO's depth mips), so far taps
    stay in cache;
  - the result raised to the power 1.5.
  LOW uses 2 slices × 4 steps per side, HIGH 4 × 6. The screen radius is bounded to 256 half-size
  pixels (96 before the pyramid, which cut part of the occlusion of very near walls). The minimum
  depth slightly overstates distant thin occluders; no halo was visible on the captures.
- **Blur and upsample.** A separable 9-tap blur (Gaussian weights cut by a 4% relative depth
  difference) runs across and down at half size. The composite then upsamples bilaterally: the four
  nearest texels in bilinear weights, divided by their relative depth difference to the pixel.
- **Application.** In linear light the composite keeps the glow and darkens the rest of the pixel:
  `c = g + (c − g) · lerp(1, ao, mask)`. Emission, the view models (the reserved near depth slice)
  and the background never darken, and the view models occlude nothing.
- **Limits.** Screen-space only: occluders off screen or behind the first surface are unknown,
  thin foreground objects darken the wall behind them within the radius, and with MSAA the resolved
  depth copy averages depths along silhouettes.

## Depth pyramid (Hi-Z)

Occlusion and reflections share one minimum-depth pyramid (R32F, at most 7 levels). Level 0 is
the half-size linear depth. Each following level keeps the nearest depth of its 2×2 children, plus
the extra row or column when the previous size is odd, so every texel covers its whole footprint.

- **Mip sizes.** They are the texture's own (halved, rounded down), so cell `k` of level `i` covers
  level-0 texels `[k·2^i, (k+1)·2^i)`.
- **No self-sampling.** A pass never samples the texture it renders into: each level is rendered to
  two targets at once, its mip of the pyramid and a separate texture that the next level reads. On
  Vulkan, a sampled view covers every mip of the texture, and SDL_GPU has disabled its check for
  this case (SDL issue 13871).
- **Cost.** About 0.1 ms at 1080p on Apple M5: occlusion HIGH went from 2.86 to 2.97 ms with the
  pyramid and the wider screen radius.

## Screen-space reflections

The enhanced shaders light every surface with a uniform environment: the local baked light
(`diffuse`) times the split-sum specular weight. Screen-space reflections replace that light by the
light actually reflected, where the screen can supply it. Nothing is added twice.

- **Inputs** (targets 3 and 4, opaque non-sky pipelines only, like the depth copy):
  - the environment specular weight `split · compensation · cavity` (RGB), with roughness in alpha;
  - the environment light it multiplied, `diffuse` times the scene light scale.
  The original shaders and the fully bright, unlit, flat and `FLAG_NOLIGHT` paths write a zero
  weight. Translucent draws do not write them, and the occlusion mask keeps their share of the
  pixel unreflected.
- **Trace (half size, Hi-Z).** On the depth pyramid:
  - the view ray is reflected about the normal rebuilt from the nearer depth neighbours;
  - it is clipped to 1024 world units, the near plane and the scene viewport;
  - it is traversed cell by cell (Uludag 2014), with depth interpolated perspective-correctly along
    the segment (McGuire and Mara 2014). A cell is skipped and the next one tested a level coarser
    when the farthest point of the ray segment crossing it stays in front of its nearest depth;
    otherwise the trace descends one level;
  - at level 0, a segment that reaches the surface within 12 units plus 2% of its depth is a hit;
    a segment entirely behind a thicker surface continues texel by texel;
  - LOW allows 24 iterations, HIGH 48, enough to cross the screen;
  - the scene colour (display space, HDR) is read bilinearly at the hit.
  The first version marched 16/32 linear steps with bisection: a long ray (floor to back wall)
  advanced 10 to 15 texels per step and could miss thin objects.
  Only pixels with a weight and roughness below 0.5 trace. The confidence fades with roughness
  (from 0.3), at the screen edges, over the last 40% of the ray, and for rays turning back to the
  camera. The output is the light premultiplied by the confidence.
- **Blur.** A separable 7-tap blur runs across and down, spread up to 6 half-size texels with
  roughness and cut by a 4% relative depth difference.
- **Application.** The composite upsamples it like the occlusion: `c += mask · W · (L − a · E)`,
  where `W` is the weight, `E` the environment light, and `L` and `a` the premultiplied reflection
  and its confidence. View models are neither reflecting nor reflected.
- **Content.** Only the manifest's glossy classes reflect visibly: tile and marble (0.35), wet,
  shiny metal, glass, screens and plastic. Most retail floors and walls resolve to rough classes,
  so the effect is local: the office's marble pillars and ceiling tiles, the 09_Abandoned_Buildings
  panels. Dielectric reflectance is about 4% at normal incidence, so walls change little. Rough
  surfaces darken slightly where the reflected scene is darker than the baked light that stood in
  for it. With every class made glossy (a test manifest), the CityHub floor mirrors the NPC and
  the gate bars.
- **Limits.** Screen-space only: anything off screen, behind the first surface or in the view
  model slice is not reflected, and the ray uses the geometric normal (no normal-map detail).
  Continuing texel by texel behind a thick surface spends iterations, so a ray passing behind a
  large occluder may end before its true hit.

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
  version 8 adds `ambient_occlusion` (0-2), version 9 `reflections` (0-2). Older versions load
  the defaults (0, 1, 1, 2, 2); writes produce version 9.
- The bench and direct paths take `--aa off|fxaa|msaa2|msaa4`, `--no-bloom`,
  `--tonemap off|neutral|filmic`, `--ao off|low|high` and `--ssr off|low|high`, with the same
  defaults.
- The inspector's world section and the bench log report the last frame's plan: direct or HDR
  scene, MSAA samples (and the request), FXAA, bloom levels, tonemap, ambient occlusion,
  reflections, passes and targets.
- `ScenePresenter` resolves the enhanced materials again whenever `surface_manifest` is reparsed
  (`SurfaceManifest::revision`), not only when a setting changes.

## Shaders

Twenty-three shaders are added:

- `world_aux_frag`, `sprite_aux_frag` and the four `pbr_*_aux_frag` variants (`-DAUX_TARGETS`,
  outputs 1 and 2), and their `*_aux_ssr_frag` versions (`-DREFLECTION_TARGETS` too, outputs 3
  and 4);
- `post.vert` (a buffer-free triangle), `bloom_down.frag`, `bloom_up.frag`, `composite.frag`,
  `fxaa.frag`, `ao_depth.frag` (two outputs: the half-size depth and pyramid level 0),
  `hiz_down.frag` (two outputs: a level and its pyramid mip), `gtao.frag`, `ao_blur.frag`,
  `ssr_trace.frag` and `ssr_blur.frag`.

All 68 artifacts are regenerated together; the 22 earlier ones are byte-identical after
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
| STANDARD, NEUTRAL, bloom and occlusion HIGH | 2.97 ms | 22 (+ depth, 6 pyramid levels, GTAO, two blurs) |
| STANDARD, defaults (+ reflections HIGH) | 3.40 ms | 25 (+ trace, two blurs) |
| STANDARD, defaults with MSAA 4X | 3.53 ms | 25 |
| STANDARD, defaults with FXAA | 3.77 ms | 26 |
| HIGH, normal maps, FILMIC, every effect, MSAA 4X | 4.10 ms | 25 |

MSAA is cheap on the tile-based Apple GPU; desktop GPUs will pay more for the multisampled
RGBA16F targets. The auxiliary targets cost bandwidth whenever bloom, occlusion or reflections are
on. A first full-size GTAO cost about 4.4 ms (HIGH); half size, the half-size blur and the
gathered upsample brought it to about 1 ms. Hi-Z reflections cost 0.41 ms on CityHub and
about 0.6 ms in the office. Most pixels are rough and stop before tracing, so the two extra scene
targets dominate. With every material glossy (the office, nearly every pixel tracing) they cost
0.71 ms (LOW) and 0.85 ms (HIGH).

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
  - ambient occlusion, at LOW and HIGH, never brightens and darkens more than 2% of the pixels by a
    mean below 8 levels;
  - reflections change the built-in frame by a mean below 2 levels, and more than 1% of the pixels
    once every material is glossy;
  - occlusion and reflections build the full 7-level pyramid, with the matching passes and targets;
  - every effect together, with MSAA or FXAA, runs with the expected targets and passes;
  - the plain settings restore the direct frame and release every target.
- `gpu.surface` measures the enhanced shading with post-processing off.
- `world_shaders.formats` (auxiliary, reflection and post shaders, resources, MSL `[[color(1)]]`,
  `[[color(2)]]`, `[[color(4)]]` and `[[vertex_id]]`), `menu` (the five rows, cycling, disabled
  rows) and `menu_preferences` (versions 7-9, migration, ranges).
- Not verified: Vulkan (Linux, Windows), MSAA support on other GPUs and the Quest.

## Remaining (M6)

Render scaling and FSR1 for both renderers, post-processing in XR, glossier material classes
where the retail textures call for them (polished floors), and bloom for the DAT's fully bright
faces if the user wants it.
