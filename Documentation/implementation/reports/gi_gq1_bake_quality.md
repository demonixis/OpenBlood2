# GI lot GQ1: bake quality settings

2026-11-08, lot GQ1 of the [GI plan](../gi_plan.md) (decisions D21 and D22), requested with lot GE1
the same day ("Both, quality first"). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#bake-quality-settings-lot-gq1). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal, `dev` build, 10 jobs. As the user asked, the
tests use four worlds (01_SubwayA, 02_CityHub, 03_Museum, 06_Airship2); the full rebake waits for
the end of the GI work.

## What landed

- **Settings:** texel supersampling (`--gi-texel-samples 1|4|9|16`), denoise (`--gi-denoise 0-4`),
  soft direct shadows (`--gi-shadow-radius 1-64`, `--gi-shadow-rays 1-64`), bounces up to 4,
  reflection strata (`--gi-reflection-strata 2-8`) and GGX samples (`--gi-reflection-samples
  32-1024`), all with `--gi-quality custom`.
- **Ultra preset** (`--gi-quality ultra`): 3 bounces, 512 rays, 4 texel samples, denoise 1, soft
  shadows 8/16, reflection strata 4, 256 GGX samples.
- **Settings text (D21):** a default value adds nothing; each other value adds one line before
  `direct rebaked`, so existing files keep their bytes and hash and other settings go stale under
  `--gi-if-stale`.
- **Tools:** the CLI and its help, `tools/gi/bake_all.py` (`--quality ultra`, the custom flags, the
  report's `settings`), the F8 Bake panel (preset radios, a Custom block, `gi_quality_arguments`) and
  `GI_RESULT` (settings and point/denoise counters).
- **Tests:** `gi_lightmap.quality` (new), additions to `gi_bake.settings`, `config.gi`,
  `gi_bake_process.parse` (an argument round trip through `parse_options`) and `gi_overlay`.

## Identity and determinism

- **Default bytes:** Final bakes of the four worlds after GQ1 are byte-identical to the G11 bakes.
- **Jobs:** Ultra on 01_SubwayA gives the same bytes with 1 and 10 jobs (51.6 s and 8.7 s); a custom
  mix (9 texel samples, denoise 2, shadows 12/5, strata 3, 100 GGX samples) is identical with 3 and
  10 jobs.
- **Each setting changes only its stage** (sections that differ from Final, 01_SubwayA):

| Setting | Changed sections |
| --- | --- |
| Texel samples, soft shadows | LMAP, DIR, VTX, PROB, REFL |
| Denoise, bounces | LMAP, VTX, PROB, REFL |
| Reflection strata, GGX samples | REFL |

- **Suites:** the headless suite passes, except `gi_bake.retail`, which fails only on an
  uncommitted local edit of `data/gi/placement/worlds/01_subwaya.json` made outside this lot. It
  passes with the committed placement files. The inspector-free Release build compiles the F8 tools
  out and passes its 1,200 tests. The GPU suite passes 141 of 150, `gpu.gi`, `gpu.surface` and
  `gpu.post` included. It ran with the screen locked and the display off: the eight
  `gpu.network_*_host` tests time out because frames are no longer paced (2,400 host frames gave 48
  server ticks), and `gpu.display_options` cannot enter fullscreen. These tests need a lit display
  and do not touch the GI code.

## `gi_lightmap.quality`

- Texel point offsets, and inner texels whose points all stay inside their polygon.
- Furnace (a closed lightmapped box, albedo 0.6, two bounces): with 9 points per texel, denoise 0
  and 2 both keep the analytic value within 2·10⁻⁶, the same for 1 and 4 jobs, and a flat field
  denoises nothing. The rays are exactly 7 × 9 × 216 × 2 + 63 × 24 (63 rays split as 7 per point).
- Noisy floor: the RMS error falls from 0.0140 (denoise 0) to 0.0092 (radius 1) and 0.0079
  (radius 2). The test asks for radius 1 to be below 0.75 × radius 0, and radius 2 below radius 1.
- Soft shadows: a 40-unit square halfway to a light disk of radius 64 leaves a visible share of
  0.844 with 64 rays, against an analytic 1 − 1600/(π 64²) = 0.876 (the test allows 0.04). Within
  the margin the test is the hard one. GD1's shelf gets more partially lit texels than with hard
  shadows, for 1 and 4 texel samples, the same for 1 and 3 jobs.

## The denoise guide: ρ⁴ to ρ²

The first design weighted neighbours by (1 − d²/h²)² ρ⁴, where ρ is the ratio of the two texels'
harmonic-mean hit distances. On the noisy floor this gained only 17–33 % RMS: hit distances from 64
rays are themselves noisy, and ρ⁴ refused most neighbours on open floors. A contact scene (a floor
meeting a wall, 256 rays, radius 1) measured the RMS near the wall for each guide:

| Guide | Near-wall RMS |
| --- | --- |
| None | 0.00416 |
| ρ | 0.00384 |
| ρ² | 0.00371 |
| ρ⁴ | 0.00374 |

ρ² keeps wall contact as well as ρ⁴ and filters the floor about twice as much (34 % at radius 1 and
64 rays), so the lot ships ρ².

## Four worlds

Total bake time in ms (10 jobs). Lot GE1's emissive columns are in the
[GE1 report](gi_ge1_emissive.md).

| World | Final | Ultra | Ultra / Final | Ultra RSS |
| --- | --- | --- | --- | --- |
| 01_SubwayA | 2,487 | 8,497 | 3.4× | 69 MB |
| 02_CityHub | 6,264 | 20,887 | 3.3× | 161 MB |
| 03_Museum | 8,019 | 26,596 | 3.3× | 181 MB |
| 06_Airship2 | 5,247 | 16,652 | 3.2× | 131 MB |

| Ultra, per world | 01_SubwayA | 02_CityHub | 03_Museum | 06_Airship2 |
| --- | --- | --- | --- | --- |
| Texel points | 583,784 | 1,378,664 | 1,752,688 | 1,078,876 |
| Dropped: solid / back faces | 11,554 / 13,740 | 32,277 / 8,065 | 21,237 / 36,725 | 19,534 / 39,504 |
| Invalid texels (Final → Ultra) | 3,802 → 3,169 | 2,203 → 1,544 | 10,482 → 9,164 | 11,709 → 10,673 |
| Dilated texels (Final → Ultra) | 3,246 → 2,470 | 1,896 → 1,089 | 6,388 → 4,984 | 7,369 → 6,781 |
| Direct pass (Final → Ultra) | 17 → 770 ms | 44 → 2,409 ms | 57 → 2,885 ms | 36 → 1,573 ms |
| Denoised texels | 141,719 | 336,163 | 426,555 | 258,970 |

Supersampling rescues texels at wall edges: 14–43 % fewer need dilation. Soft shadows dominate the
added direct cost (16 rays per light and point); the denoise filter costs at most 13 ms.

## In game

Captures at the start point (1280 × 720, enhanced renderer, LIGHTING HIGH, GI on, REFLECTION PROBES
HIGH), against the Final bake:

| World | Pixels changed by Ultra | Mean luminance (Final → Ultra) |
| --- | --- | --- |
| 01_SubwayA | 39.7 % | 21.22 → 21.28 |
| 02_CityHub | 39.0 % | 34.23 → 33.71 |
| 03_Museum | 62.9 % | 60.32 → 60.71 |
| 06_Airship2 | 89.1 % | 29.97 → 31.52 |

The changes are mostly softer shadow edges and less noise in the indirect light; overall brightness
moves by less than 2 %, except 06_Airship2 (+5 %).

## Open

- **Gate H:** the default preset (Final or Ultra), from the A/B in the GE1 report.
- The full rebake of the 36 worlds, at the end of the GI work.
