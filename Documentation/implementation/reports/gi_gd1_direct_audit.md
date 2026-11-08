# GI lot GD1: direct-light audit

2026-11-07, lot GD1 of the [GI plan](../gi_plan.md) (D1 (c), decisions D16–D20). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#direct-light-audit-lot-gd1). Host: Apple M5 (4
performance and 6 efficiency cores), macOS. Diagnostic only: no GI file, setting or frame changes.

## What landed

- **`engine/gi_direct.{h,cpp}`** (built with `-ffp-contract=off`):
  - `gi_direct_lights` reads Light and DirLight objects in object order: colours × BrightScale,
    the omni and cone OuterColor, the cone axis from the rotation (quarter-turn `gi_sin_cos`, no
    library trigonometry), ClipLight and whether the light sits in solid root-BSP space.
  - `gi_direct_term` gives the D16 term.
  - `GiDirectOccluders` holds one collision index per BSP (SOLID polygons, minus SURF_INVISIBLE).
  - `gi_direct_visible` is the D19 hard test.
  - `gi_direct_texels` sums every LMAP texel in jobs of 16 blocks, each writing its own slots.
  - `gi_direct_audit` produces the fit splits.
- **`--gi-direct-audit <world | single | all>`**: JSON for each world and for the total.
  `--gi-jobs` applies. `--gi-preview-dir` writes `<stem>.direct.<view>.png`, three panels:
  Original, Re-baked direct, Difference.
- **`--gi-preview` Difference panel** (`GiPreviewThird::Difference`): grey albedo with the mean
  channel difference ×4, red where re-baked is brighter, blue where darker.
- **Tests**:
  - headless: `gi_direct.term`, `gi_direct.lights`, `gi_direct.shadow`;
  - private: `gi_direct.retail`;
  - config parse cases.
- **Docs**:
  - the GD1 contract section;
  - the static light correction;
  - plan, roadmap, AGENTS.md and provenance.

## The term (D16, Gate F)

For each texel, with f = 1 − d/R and d measured from the texel centre (`LightmapFrame::position`):

- **Light:** `LightColor·f + OuterColor·(1 − f)`.
- **DirLight inside its cone:** `(InnerColor·a + OuterColor·(1 − a))·f`, with
  `a = (cos − cos(FOV/2)) / (1 − cos(FOV/2))`. Outside the cone the light adds nothing.

Colours are multiplied by BrightScale, sign kept. The texel is `AmbientLight + Σ`, rounded half
up and clamped to 0–255 once.

There is no N·L. These are DEdit's lightmap rules (`LMSampleGen.cpp`, read only) with the linear
falloff the 1998 data fits.

**Visibility (D19).**
- **Start:** the G7 sample, raised another 0.5 along the normal, so 1 unit off the face.
- **End:** 1 unit short of the light.
- **Test:** a zero-box sweep.
- **Occluders:** each BSP shadows only its own texels, with its SOLID polygons except
  SURF_INVISIBLE ones.
- **Special cases:** an unclipped light is always visible. A light in solid space stays shadowed.
  Sky and translucent faces occlude.

## Results (36 single-player worlds)

10,107,436 texels and 13,473 lights. The run took 65 s wall time with 10 jobs (303 s CPU).

| Set | Texels | fit | fit_step | mean \|d\| | brighter | darker |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| All | 10,107,436 | **84.8 %** | 80.8 % | 6.36 | 6.3 % | 9.0 % |
| Interior | 6,386,715 | 93.6 % | 91.3 % | 4.11 | 1.9 % | 4.5 % |
| Edge (visible light set differs from a 4-neighbour) | 3,720,721 | 69.6 % | 62.7 % | 10.23 | 13.7 % | 16.7 % |
| Primary world texels | 9,607,325 | 85.6 % | | | | |
| World-model texels | 500,111 | 68.5 % | | | | |
| 0 lights reaching | 435,364 | 99.9 % | | | | |
| 1 light | 871,586 | 94.8 % | | | | |
| 2–3 lights | 3,634,618 | 88.4 % | | | | |
| 4+ lights | 5,165,868 | 79.2 % | | | | |

- **fit:** every channel within one RGB565 step plus 3 (11, 7, 11).
- **fit_step:** within one step (8, 4, 8).

The GOURAUDSHADE flag makes no difference (84.4 % against 84.9 %).

**Where the error lies.** Texels away from shadow edges fit at 93.6 %. Most of the remaining error
is at shadow edges (37 % of the texels), in texels lit by many overlapping lights, and on texels
of world models.

The stored texel is darker slightly more often than it is brighter.

### Per world (worst first)

The edge share is the share of each world's texels that are edge texels.

| World | Texels | fit | fit_step | mean \|d\| | brighter | darker | interior | edge | edge share | world models |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 06_Airship2 | 264,809 | 72.4 | 65.8 | 9.35 | 11.3 | 16.3 | 85.7 | 58.5 | 49 | 54.6 |
| 23_Rooftop | 129,610 | 77.3 | 75.2 | 9.68 | 8.2 | 14.5 | 88.2 | 60.2 | 39 | 72.3 |
| 05_CDMB | 29,213 | 78.0 | 73.9 | 9.19 | 9.9 | 12.1 | 92.7 | 64.0 | 51 | 76.4 |
| 22_Office | 246,574 | 79.4 | 74.5 | 8.42 | 7.5 | 13.1 | 92.1 | 67.2 | 51 | 56.6 |
| 17_Entrance | 288,388 | 80.1 | 73.8 | 7.70 | 8.3 | 11.6 | 90.2 | 69.7 | 49 | 74.0 |
| 02_CityHubC | 392,363 | 80.2 | 76.0 | 7.31 | 7.0 | 12.8 | 87.5 | 65.8 | 34 | 54.0 |
| 02_CityHubB | 295,240 | 80.3 | 76.3 | 7.74 | 8.0 | 11.6 | 88.1 | 66.5 | 36 | 56.8 |
| 05_CDM | 343,541 | 80.8 | 73.9 | 7.49 | 8.4 | 10.8 | 92.0 | 69.4 | 50 | 42.0 |
| 04_SteamTunnels | 600,508 | 81.0 | 77.0 | 6.57 | 7.4 | 11.6 | 92.9 | 64.2 | 42 | 60.2 |
| 20_PowerStation | 249,257 | 81.6 | 76.1 | 8.71 | 7.8 | 10.7 | 93.1 | 67.2 | 44 | 81.1 |
| 01_SubwayA | 142,970 | 82.9 | 79.0 | 6.47 | 7.9 | 9.2 | 96.0 | 63.0 | 40 | 71.3 |
| 02_CityHub | 336,601 | 83.0 | 78.5 | 7.10 | 6.6 | 10.4 | 90.7 | 70.2 | 38 | 31.0 |
| 06_Airship1 | 129,457 | 83.4 | 79.0 | 6.58 | 7.3 | 9.3 | 91.7 | 65.6 | 32 | 51.2 |
| 03_Museum | 432,829 | 83.5 | 77.5 | 6.16 | 6.7 | 9.8 | 94.6 | 69.5 | 44 | 74.6 |
| 08_SubwayB | 152,167 | 83.6 | 79.8 | 6.24 | 7.5 | 8.9 | 96.3 | 63.3 | 38 | 71.2 |
| 11_Slaughter_House | 606,643 | 84.1 | 79.7 | 6.52 | 6.4 | 9.5 | 92.8 | 72.1 | 42 | 71.7 |
| 12_Train_StationB | 400,048 | 84.2 | 82.0 | 7.91 | 6.6 | 9.2 | 94.3 | 62.1 | 31 | 63.0 |
| 10_SewageTreatmentPlant | 164,243 | 84.3 | 80.3 | 5.50 | 6.6 | 9.2 | 94.2 | 69.3 | 40 | 78.2 |
| 09_Abandoned_Buildings | 479,509 | 84.8 | 81.2 | 7.42 | 6.3 | 8.9 | 93.5 | 62.8 | 28 | 68.3 |
| 14_SubwayC | 280,088 | 85.0 | 80.8 | 6.08 | 7.0 | 8.0 | 97.4 | 65.6 | 39 | 67.3 |
| 16_Bridge | 321,820 | 85.2 | 80.7 | 6.39 | 6.4 | 8.4 | 94.3 | 74.4 | 45 | 73.5 |
| 19_Lobby | 411,632 | 85.8 | 82.1 | 6.26 | 6.0 | 8.3 | 94.0 | 76.2 | 46 | 64.8 |
| 24_AncientCity | 502,597 | 86.2 | 83.7 | 6.74 | 4.4 | 9.4 | 92.0 | 68.7 | 25 | 69.3 |
| 21_Laboratory | 256,195 | 86.8 | 80.1 | 4.79 | 6.5 | 6.7 | 94.8 | 78.7 | 50 | 84.6 |
| 07_Cathedral | 394,921 | 87.1 | 83.4 | 5.25 | 5.6 | 7.3 | 94.7 | 73.7 | 36 | 62.5 |
| 12_Train_Station | 537,425 | 87.5 | 84.8 | 6.07 | 5.3 | 7.2 | 95.0 | 66.0 | 26 | 63.2 |
| 13_Downtown | 479,174 | 87.8 | 84.7 | 5.34 | 4.8 | 7.4 | 95.1 | 74.8 | 36 | 76.2 |
| 19_LobbyC | 99,337 | 89.1 | 85.2 | 5.04 | 4.5 | 6.4 | 95.3 | 82.2 | 47 | 79.5 |
| 19_LobbyB | 99,401 | 89.2 | 85.3 | 4.92 | 4.5 | 6.3 | 95.0 | 82.8 | 47 | 79.8 |
| 15_Crypt | 128,142 | 90.8 | 88.4 | 4.16 | 4.2 | 5.0 | 95.9 | 76.5 | 26 | 73.3 |
| 18_Temple | 272,326 | 91.5 | 87.7 | 4.24 | 4.2 | 4.3 | 97.0 | 82.0 | 36 | 87.7 |
| Drop | 12,772 | 93.1 | 90.5 | 3.57 | 3.3 | 3.6 | 99.0 | 83.8 | 39 | — |
| 24b_Arena | 80,785 | 93.1 | 91.4 | 4.49 | 4.0 | 2.9 | 98.1 | 73.2 | 20 | 91.3 |
| 25_AncientOne | 536,820 | 97.7 | 97.1 | 1.58 | 1.0 | 1.4 | 98.6 | 81.5 | 5 | 97.9 |
| Credits | 1,589 | 100.0 | 100.0 | 0.00 | 0.0 | 0.0 | 100.0 | — | 0 | — |
| End | 8,442 | 100.0 | 99.7 | 0.86 | 0.0 | 0.0 | 100.0 | 100.0 | 6 | 100.0 |

The plan's starting point measured 74.7 % over a sample. The full audit raises it to 84.8 %.
Airship2 rises from 54 % to 72.4 % and Office from 55 % to 79.4 %.

## Sweeps

Each variant was measured over all 36 worlds and kept only when it raised the fit there. The
earlier sweeps ran with the older term (texel corner, no OuterColor), so compare figures only
within a row.

| Question | Variants (all-world fit) | Kept |
| --- | --- | --- |
| Occluders (old term) | Primary world only 76.3; each BSP its own texels 78.0; world-model texels by primary + own 77.2; primary texels by every BSP 71.8; every BSP for everything 70.7 | Each BSP its own texels |
| Sample for the term and the ray | Ray from the moved G7 sample 83.5; projected on the face 76.0; moved + 0.1 82.8; moved + 2 83.6; term at the moved sample 78.9; term at the corner 78.0 | Term at the texel centre, ray from the moved sample |
| Ray start height and inset | 1 unit off the face 83.8; 4 units 83.0; 8 units 81.5; texel insets 83.3, 82.9, 82.5 | 1 unit off the face (the G7 0.5 plus a 0.5 lift), no inset, end margin 1 |
| Cone shape (22_Office) | Inner colour only 70.6; mix(cone)·distance 78.8; mix(distance·cone) 68.5; mix(distance)·cone 71.0; distance squared far worse; outer colour outside the cone 46–61 | `(inner·a + outer·(1 − a))·f`, nothing outside the cone |
| Omni OuterColor (DEdit rule) | 20_PowerStation 78.9 → 81.6, 22_Office +0.1, no world lower | Kept |
| Rounding and clamp | Round half up 84.5; floor 84.6; clamp per light 84.5 | Round half up, clamp once (DEdit's order; the 0.1 point difference is within noise) |
| Unclipped lights | Treated as clipped 84.4 | Never shadowed (ClipLight false) |
| Lights in solid space | Unblocked 80.8 | Shadowed |
| Invisible faces | Excluded from occluders 84.68 (05_CDMB 77.2 → 78.0) | Excluded |
| Sky faces | Excluded 84.51: CityHubB +1.2, CityHubC +2.1, AncientCity +1.8, but Train Station −3.0, Arena −1.3, Abandoned −1.8 | Occlude (world-dependent, net loss) |
| Translucent faces | Excluded 83.96 | Occlude |
| Metric | One RGB565 step only 80.6 | Both reported; Gate F reads `fit` |

## Named causes

- **22_Office and 20_PowerStation: OuterColor was ignored.**
  - Office has 160 lights with a cone and 3 omni lights with a nonzero OuterColor. Adding the cone
    blend took it from 70.6 % to about 79.4 %.
  - PowerStation has 222 cone and 14 omni OuterColor lights. The omni rule took it to 81.6 %.
  - Still open: PowerStation's `grill.dtx` faces (flags 0x81) are its main dark blockers.
    Grilles probably let light through in 1998.
- **05_CDMB: an invisible solid face.**
  - A SOLID `black.dtx` face with flags 0x145 shadowed 1.5 % of the texels.
  - Fixed by the invisible-face rule.
- **06_Airship2: open.**
  - Lowest interior fit (85.7 %) and 49 % edge texels.
  - Long-radius hull lights: L341/L342, R 800, colour (127, 26, 0).
  - Letting other models' faces occlude, or every bake-scene face, recovers only 0.6 % / 0.3 %.
  - The remaining error looks like soft or offset edges from the 1998 sampler, not a missing
    occluder.
- **23_Rooftop: open.**
  - Outdoor R 600 lights.
  - The top dark blockers are brushcrete faces and masked fence world models (flags 0x1089).
  - Masked faces are probably transparent where their texture is.
- **17_Entrance, CityHubB/C: mostly shadow edges.**
  - Interior fit is 87–90 %.
  - The sky-face choice moves CityHubB/C by up to 2 points, but excluding sky faces loses more
    elsewhere.
- **World-model texels: 68.5 %, open.**
  - Doors and breakables are lit at their authored pose (D8).
  - In 1998 they were probably lit with different occluders or a different sample.

## Static light evidence correction

The [static light contract](../../contracts/static_light_contracts.md) cited lightmap texels as
"1.0–1.4× the N·L term". GD1 shows that the lightmap term has no N·L, so that note is marked
superseded.

Model light still applies N·L ×2. Whether to change that is a separate decision for the user.
Model light also reads no OuterColor.

## Preview panels

`--gi-direct-audit <world> --gi-preview-dir <dir>` renders three 480×360 panels for each view:
Original, Re-baked direct, Difference. Vertex-lit faces keep the original (D18).

Panels were rendered for three views each of 01_SubwayA, 22_Office and 06_Airship2. They are not
committed, since they show retail content.

- The re-baked panel is hard to tell from the original away from shadow edges.
- The difference panel shows smooth low-amplitude red/blue gradients (the RGB565 step and the
  sampler).
- It shows a few strong blue bands where the re-baked shadow edge falls one texel off, for
  example Office's door frame.
- On light fixtures (red) the stored light is darker than the term predicts close to the source.

## Results of the checks

| Check | Result |
| --- | --- |
| `gi_direct.term`, `gi_direct.lights`, `gi_direct.shadow` (headless) | pass |
| `gi_direct.retail` (private, retail data) | pass: SubwayA 371 lights / 261 cones / 6 unclipped, fit ≥ 82 %, interior ≥ 95 %, identical texels with 1 and 8 jobs; Office 530 / 299 / 2 / 160 cone OuterColor, fit ≥ 78 %, interior ≥ 90 % |
| `config.gi` (GD1 parse cases) | pass |
| Headless and private suite, RelWithDebInfo (`dev`) | 1,645 / 1,648. Three failures: `config.gi`, expected text since fixed, passes; `locomotion.cityhub_roam`, timeout under `-j 10`, passes alone in 19.8 s; `gi_bake.retail`, caused by an uncommitted edit to `data/gi/placement/worlds/01_subwaya.json` (light-probe spacing 128 → 232) that is not part of this lot |
| Inspector-free Release, headless suite | 1,193 / 1,193 |
| ASan (`gi_direct.term/lights/shadow`, `config.gi`) | 4 / 4 |
| GI lightmap, preview and GD1 tests after the final term changes (`dev`) | 20 / 20, `gi_direct.retail` included |

## Decisions within the lot

- The edge/interior split uses the visible-light-set hash of the 4 neighbours, so a misfit can be
  attributed to a shadow boundary without a second trace.
- The audit excludes texels whose sample lies in solid space, as G7 does.
- `CollisionWorld` is not built with `-ffp-contract=off`. GD1's numbers do not depend on it
  (identical across job counts), but GD2 must make the occlusion test build-independent before
  bytes are written.

## Open

- **Gate F:** the user accepts or adjusts the term.
- **GD2 prerequisite:** build-independent occlusion.
- **Model light ×2:** a separate user decision.
- **Possible occluder refinements:** see-through grilles (PowerStation), masked fences (Rooftop),
  per-world sky faces, world-model texels and Airship2's edges. Each refinement must raise the
  all-world fit.
