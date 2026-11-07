# GI lot G12: reflection runtime

2026-11-07, lot G12 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#reflection-runtime-lot-g12). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal. Baked data: the four [G11](gi_g11_reflection_bake.md)
worlds (01_SubwayA, 02_CityHub, 03_Museum, 06_Airship2), in a scratch directory; there is no full
rebake, as the user asked.

## What landed

- **`engine/gi_reflection_runtime.{h,cpp}`**: `GiReflectionMode` (OFF/LOW/HIGH), the GPU data of a
  set (`gi_reflection_gpu`: RGBA16F layers of the valid probes, records, the cell grid), the
  placeholder, the panel header and `gi_reflection_environment`, the CPU mirror of the shader.
- **Shaders:** the `-DGI` code of `pbr.frag.hlsl` (`probeEnvironment`, box coverage, two-probe
  blending, box projection, Reflections only, the page alpha's `FACE_SHADOW` and
  `DIRECT_LUMINANCE`) and its 24 artifacts; `world_shaders` gains the 12 GI kinds.
- **Renderers:** `WorldPipelines` selects the GI variants (`prepare(..., gi, ...)`); `SurfaceScene`
  binds the probe buffers and the reflection array; `GpuRegistry::stage_texture` uploads one layer
  and level of a created texture (16-byte aligned staging offsets).
- **Runtime:** `GiWorldLight::compose` returns each texel's direct share (the page alpha); REFL is
  decoded with PLAC; `ScenePresenter::prepare_reflections` builds the data once per file, mode and
  set, and restages only the header for the panel.
- **Settings and tools:** Options > Display REFLECTION PROBES (row 32, preferences version 14), the
  F8 Runtime panel's Reflection probes group, `--gi-reflections off|low|high` for `--bench` and the
  smoke runs, and a `--bench` summary line (mode, set, layers, cells, memory, build time).
- **Docs:** the GI contracts (Reflection runtime), the enhanced renderer, post-processing, menu and
  Metal shader contracts, the plan, the roadmap, AGENTS.md and the provenance notes.

## Results

| Check | Result |
| --- | --- |
| Headless and private suite (`dev`, retail data), G11 and G12 tests included | 1,657 / 1,658: `gi_bake.retail` fails only on the working tree's uncommitted edit of `01_subwaya.json` (light-probe spacing 128 → 232, not part of this lot); with the committed placements it passes |
| GPU suite (`dev`, real display, Apple M5 Metal) | 149 / 150: `gpu.display_options` failed as in G9 (the window system did not apply the requested fullscreen state; the window path is untouched) |
| `gpu.gi` on Metal | pass: Reflections only matches the CPU mirror within one unit on 477,607 of 518,400 pixels (CityHub) and 507,090 (SubwayA) |
| `gpu.gi` on Vulkan (MoltenVK from the Vulkan SDK 1.4.350, Apple M5, `SDL_GPU_DRIVER=vulkan`) | pass, with the same pixel counts |
| Shader regeneration | all 96 artifacts regenerate byte for byte; the 72 earlier ones are unchanged |
| Inspector-free Release build (`BLOOD2_GI_TOOLS` off) | builds; headless suite 1,198 / 1,198 (no private data configured) |

## Cost

`blood2 --bench --renderer high --gi-dir … --gi-reflections off|low|high`, 1920×1080, 600 measured
frames after 30 warm-up frames, `dev` build, default post-processing (NEUTRAL, bloom, AO HIGH, SSR
HIGH, so the GI SSR variants run). Each row ran twice; the table gives the second run's mean GPU
completion wait (the serialized bench waits for each frame), in ms.

| World | GI on: probes OFF | LOW | HIGH | HIGH − OFF | GI off: probes OFF | HIGH (Original set) | HIGH − OFF |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 01_SubwayA | 4.140 | 4.324 | 4.325 | +0.185 | 4.120 | 4.315 | +0.195 |
| 02_CityHub | 3.999 | 4.272 | 4.270 | +0.271 | 4.076 | 4.306 | +0.230 |
| 03_Museum | 3.850 | 4.077 | 4.112 | +0.262 | 3.865 | 4.084 | +0.219 |
| 06_Airship2 | 4.079 | 4.244 | 4.245 | +0.166 | 4.081 | 4.248 | +0.167 |

- **Within the aim** (0.3 ms at 1080p HIGH): HIGH adds 0.17 to 0.27 ms. The differences between
  medians are within 0.05 ms of these, except CityHub with GI off (0.31 ms between medians).
- **LOW costs the same time as HIGH:** both walk the cell's candidates, and HIGH samples a second
  probe only where two overlap. LOW saves memory only.
- **The GI variant itself is free:** with probes OFF, GI on (GI variant with the placeholder) and GI off
  (the G8 enhanced shaders) differ by less than the run-to-run noise.
- **CPU:** the record time is unchanged (0.16 to 0.31 ms per frame in every column); building the data
  takes 0.3 ms (SubwayA) to 2.4 ms (CityHub) at HIGH, once per file, mode or set.

| World | Probes (layers) | HIGH texture bytes (64², 5 levels) | LOW texture bytes (32², 4 levels) | Records and cells (bytes) | Grid (cell 128 units) | Occupied cells | Shared lists |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 01_SubwayA | 6 | 261,888 | 65,280 | 3,932 | 3 × 3 × 93 | 837 | 8 |
| 02_CityHub | 40 | 1,745,920 | 435,200 | 180,712 | 40 × 19 × 58 | 12,309 | 123 |
| 03_Museum | 41 | 1,789,568 | 446,080 | 101,236 | 44 × 11 × 50 | 6,685 | 132 |
| 06_Airship2 | 31 | 1,353,088 | 337,280 | 65,632 | 21 × 22 × 34 | 2,178 | 46 |

No cell of the four worlds exceeds the 8-candidate limit.

## A/B for Gate E

Captures from `--bench --renderer high --gi --gi-reflections off|high --capture` at the start point
(1280×720, GI on, not committed: they contain retail art). Pixels compared by RGB:

| World | Pixels changed | Darker / brighter | Mean luminance OFF → HIGH |
| --- | --- | --- | --- |
| 01_SubwayA | 19.5 % | 179,358 / 1 | 21.96 → 21.22 |
| 02_CityHub | 94.1 % | 865,780 / 1,232 | 37.67 → 34.23 |
| 03_Museum | 78.0 % | 715,977 / 2,889 | 64.74 → 60.32 |
| 06_Airship2 | 32.9 % | 277,211 / 26,190 | 32.78 → 29.97 |

With probes the environment reflection becomes what the probe saw (albedo times light of the
surroundings) instead of the face's own light, which is brighter: the uniform specular sheen of the
enhanced walls and floors drops, by 3 to 9 % of the mean luminance, and the view weapon and models
reflect their surroundings. The few brighter pixels are where a probe saw brighter surroundings than
the face's own light.

To play the A/B: Options > Display > RENDERER ENHANCED, then REFLECTION PROBES OFF/LOW/HIGH (with
or without GLOBAL ILLUMINATION), and F8 > Runtime > Reflection probes (intensity, Reflections only).
The world needs a file baked since G11 in a GI search root (`--gi-dir` or `user_directory()/gi`).

## Corrections during the lot

- `WorldPipelines::prepare` compared the GI flag without storing it: every GI frame recreated the
  pipeline set, and the first frame after switching GI off kept the GI pipelines, which then lacked
  their reflection binding (SDL's "Missing fragment sampler binding!" assertion in `gpu.gi`). The
  flag is now stored, the renderers refuse to record when the pipelines' GI state and the bound
  scene disagree, and `gpu.gi` checks that GI frames keep their pipelines.
- The headless page tests compared page colours with an alpha filter; they now compare RGB and the
  alpha separately against the share formula.
- The radiance checks of `gpu.gi` switch the world's fog off: fog applies after the probes, as on
  every face, and CityHub's fog moved most pixels a few units away from the mirror's value.

## Open

- **Gate E:** the user plays the A/B and decides the default per platform. REFLECTION PROBES stays
  OFF by default until then.
- The full rebake of the 36 worlds waits for the end of M10. Files baked before G11 have no REFL
  section; the panel says so.
- Vulkan ran through MoltenVK only; a native Vulkan driver (Linux NVIDIA, Windows) is not tested.
