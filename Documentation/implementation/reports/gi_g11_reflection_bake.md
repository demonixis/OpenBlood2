# GI lot G11: reflection bake

2026-11-07, lot G11 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#reflection-bake-lot-g11). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal. Baked data: four worlds (01_SubwayA, 02_CityHub,
03_Museum, 06_Airship2) at Final from the committed placements, in a scratch directory. As the user
asked, there is no full rebake: it waits for the end of M10.

## What landed

- **`engine/gi_reflections.{h,cpp}`**:
  - the octahedral map (+y pole, one-texel folded borders) and its level chain down to 4 × 4;
  - cube capture directions and bilinear cube samples;
  - Hammersley points, `gi_ggx_half_vector` and the GGX prefilter (CyberEngine, reimplemented);
  - RGB9E5 chains (`gi_reflection_chain`);
  - the `REFL` codec (`write_gi_reflections`, `read_gi_reflections`) and its placement check;
  - the Original and Composed light sets (`gi_reflection_light`);
  - `bake_gi_reflections`: one probe per job, results in probe order, cancel.
- **Bake** (`gi_bake.cpp`):
  - the reflection pass after the light probes, with REFL written last;
  - `GI_PROGRESS stage=reflections` and the reflection fields of `GI_RESULT`;
  - `gi_algorithm_version` 4;
  - `GiSky::set_scale`, so the capture sees the sky at scale 1.
- **Tools:**
  - the CLI and the F8 bake write REFL whenever the placement has reflection probes;
  - the F8 progress bar gives the light probes 2 % and the reflection probes 3 % (`gi_bake_process`);
  - `tools/gi/bake_all.py` records `reflection_valid` and `reflection_ms` per world.
- **Gate C build split:** `BLOOD2_GI_TOOLS` is a `cmake_dependent_option` on `BLOOD2_INSPECTOR` (and
  not iOS or Android). The debug and dev presets build F8 and the in-game bake; Release presets do
  not. The headless CLI (`--bake-gi`, `--gi-place`, `--gi-preview`, `--gi-direct-audit`) stays in
  every build.
- **Docs:** the GI contracts (Reflection bake), the plan (Gate C, Gate D, D4 and D12), AGENTS.md
  and the provenance notes.

## Results

| Check | Result |
| --- | --- |
| `gi_reflections.octahedral`, `.prefilter`, `.sections`, `.capture` | pass |
| `gi_bake.retail` (SubwayA, private) | REFL written last, one valid chain per PLAC probe at 64² (run with the committed placements; the working tree has a foreign edit of `01_subwaya.json`) |
| Inspector-free Release build | builds; `BLOOD2_GI_TOOLS` is off; the SubwayA Final bake is byte-identical to the dev build's (1,645,400 bytes, SHA-256 `21bff4f3c63f4c3bbd13…`, 6 valid reflection probes) |

The suite totals for G11 and G12 together are in the [G12 report](gi_g12_reflection_runtime.md#results).

## Bake measurements

`blood2 --bake-gi WORLD --gi-quality final` with 10 jobs, dev build (RelWithDebInfo). Capture and
prefilter times are summed over the jobs; the wall time is the reflection pass alone.

| World | Probes | Valid | Rays | Back hits | Capture (ms, jobs) | Prefilter (ms, jobs) | Wall (ms) | REFL bytes | Whole bake (ms) | RSS (MB) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 01_SubwayA | 6 | 6 | 589,824 | 52 | 111 | 47 | 33 | 261,936 | 2,390 | 63 |
| 02_CityHub | 40 | 40 | 3,932,160 | 664 | 899 | 387 | 142 | 1,746,104 | 6,009 | 144 |
| 03_Museum | 41 | 41 | 4,030,464 | 3,255 | 958 | 407 | 152 | 1,789,756 | 7,510 | 158 |
| 06_Airship2 | 31 | 31 | 3,047,424 | 936 | 762 | 269 | 114 | 1,353,236 | 4,870 | 117 |

The reflection pass adds 1 to 3 % to a world's bake. Every probe of the four worlds is valid.

## Memory and GPU layout (Gate D, decision D12)

One probe's chain in RGBA16F takes 43,648 bytes at 64² (5 levels, 5,456 texels) and 10,880 bytes at
32² (4 levels). A standalone Metal program (`MTLDevice heapTextureSizeAndAlign`) measured:

| Layout | 42 probes, 64², 5 levels | 16 probes, 32², 4 levels |
| --- | --- | --- |
| Octahedral 2D array | 2,080,768 bytes | 212,992 bytes |
| Cube array (42 or 16 cubes) | 12,484,608 bytes | 1,212,416 bytes |
| Cube array as SDL_GPU creates it | 74,907,648 bytes | 7,225,344 bytes |

SDL_GPU's Metal backend passes `layer_count_or_depth` (six times the cube count) as the cube array's
`arrayLength`, so a cube array allocates six times its size. D12 is decided as (a), the octahedral
2D array, which also needs no SDL patch. Gate D is closed.

## Open

- The full rebake of the 36 worlds waits for the end of M10, as the user asked.
