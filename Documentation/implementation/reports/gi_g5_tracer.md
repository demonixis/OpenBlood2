# GI lot G5: bake scene, deterministic tracer and `--bake-gi`

2026-11-06, lot G5 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#bake-scene-and-tracer-lot-g5). Host: Apple M5
(4 performance and 6 efficiency cores), macOS.

## What landed

- **`engine/gi_bake_scene.{h,cpp}`**: the bake scene built from the presentation `WorldMesh`
  at the authored pose (Main, Sky and Skip ranges by role, placement exclusions), per-face
  albedo from the mip closest to the 20-unit grid, WorldShade's baked light, a binned SAH BVH
  and a two-sided closest-hit tracer with cutouts and up to eight translucent layers.
- **`engine/gi_jobs.{h,cpp}`**: the job pool on SDL threads with a cancel flag, splitmix64
  streams seeded per ray.
- **`engine/gi_bake.{h,cpp}`**: the presets and INFO text, the probe survey and
  `bake_gi_world` (placement, `--gi-if-stale`, scene, survey, INFO + PLAC file, cancellation).
- **`--bake-gi WORLD`** with `--gi-quality`, `--gi-bounces`, `--gi-rays`, `--gi-jobs`,
  `--gi-out` and `--gi-if-stale`; `GI_PROGRESS`/`GI_RESULT` lines; exit 0, 2, 4, 5 and 12;
  SIGINT/SIGTERM cancel.
- **Placement format 2** (decision D8): `excluded_models`; version 1 files still read. The 36
  committed placement files were rewritten by `--gi-place single` (two lines each: the version
  and an empty list).
- **Tests:** `gi_bake.bvh`, `alpha`, `albedo`, `jobs`, `settings`, `determinism`, the private
  `gi_bake.retail`, and extended `gi_placement.json` and `config.gi`.

The file still holds INFO and PLAC only; the survey is the traced output this lot checks. G7
adds the lighting sections.

## Results

| Check | Result |
| --- | --- |
| Headless suite, RelWithDebInfo (`dev`) | 1,163 / 1,163 |
| Headless suite, inspector-free Release | 1,163 / 1,163 |
| GI tests with private data (`dev`, 32 tests) | pass |
| `gpu.surface`, `gpu.surface_effect(_private)`, `gpu.post` | pass |
| ASan + UBSan (`asan`): every `gi_bake` case, `gi_placement`, `config.gi` | pass, no report |
| ThreadSanitizer, standalone job-pool program (normal and cancelled runs) | no report |
| Thread-count independence | same bytes and digest for 1, 4 and 10 jobs (SubwayA, CityHub, SteamTunnels) |
| Build independence | same SubwayA digest in Debug+ASan, RelWithDebInfo and Release |
| WorldShade match (SubwayA, 20,000 segments) | 19,599 compared, worst fraction 4.2e-7, worst light 4.5e-5; 127 joint misses; 274 skipped (translucent or layered) |
| SIGINT during a CLI bake | exit 12, `GI_RESULT status=cancelled`, nothing written |
| `--gi-if-stale` second run | `status=current`, 32 ms (CityHub) |
| Throughput, 1 thread (SteamTunnels, 34,613 faces) | 4.58 M rays/s (target ≥ 1 M per core) |
| Throughput, 4 / 10 threads | 16.7 / 30.4 M rays/s |

Two findings changed the code during the lot:

- **Optimization-level noise.** The first digests differed between Debug+ASan and
  RelWithDebInfo with identical counts. Fused multiply-add contraction and Apple's merging of
  `sinf`/`cosf` into `__sincosf_stret` at `-O2` changed the last bits. The tracer and bake now
  compile with `-ffp-contract=off` and directions use rejection sampling (no library
  trigonometry); the three builds agree. The test's brute-force reference uses the same flag.
- **WorldShade agreement.** Primary-world hits are bit-identical in arithmetic; world-model hits
  differ only by the pivot rounding (local versus world space), far below the tolerances.

## Single-player survey at Final

`--bake-gi <world>` for each committed placement, Final (256 rays per probe), 10 threads. Rays go
to infinity from every light probe. Front, back, sky and miss are shares of the rays; layers count
translucent layers passed; inside counts probes with more than half their rays on back faces.

| World | Probes | Faces | Transl. | Rays | Mrays/s | Front % | Back % | Sky % | Miss % | Layers | Inside | Survey ms | Total ms | Bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 01_subwaya | 1,031 | 7,441 | 62 | 263,936 | 31.2 | 99.2 | 0.2 | 0.0 | 0.6 | 3,359 | 0 | 8 | 51 | 21,437 |
| 02_cityhub | 6,185 | 19,964 | 488 | 1,583,360 | 32.8 | 63.5 | 0.2 | 36.3 | 0.0 | 6,417 | 3 | 48 | 332 | 127,101 |
| 02_cityhubb | 4,273 | 18,264 | 476 | 1,093,888 | 33.8 | 69.9 | 0.2 | 29.9 | 0.0 | 2,531 | 2 | 32 | 224 | 88,254 |
| 02_cityhubc | 6,951 | 21,795 | 472 | 1,779,456 | 31.8 | 65.7 | 0.1 | 34.2 | 0.0 | 3,934 | 1 | 56 | 318 | 141,966 |
| 03_museum | 4,637 | 22,880 | 1216 | 1,187,072 | 31.0 | 85.1 | 0.1 | 14.8 | 0.0 | 42,483 | 2 | 38 | 205 | 96,216 |
| 04_steamtunnels | 5,077 | 34,613 | 1876 | 1,299,712 | 30.4 | 99.4 | 0.5 | 0.1 | 0.0 | 7,534 | 0 | 43 | 442 | 105,098 |
| 05_cdm | 3,336 | 23,292 | 728 | 854,016 | 33.2 | 89.0 | 0.1 | 11.0 | 0.0 | 21,830 | 0 | 26 | 203 | 70,269 |
| 05_cdmb | 8,847 | 2,078 | 250 | 2,264,832 | 42.4 | 35.4 | 0.3 | 64.3 | 0.0 | 52,989 | 0 | 53 | 160 | 177,602 |
| 06_airship1 | 11,151 | 14,670 | 601 | 2,854,656 | 38.0 | 14.9 | 0.5 | 68.7 | 15.9 | 12,937 | 0 | 75 | 274 | 223,534 |
| 06_airship2 | 2,161 | 21,908 | 1136 | 553,216 | 31.1 | 73.2 | 0.5 | 26.3 | 0.0 | 28,079 | 0 | 18 | 147 | 45,938 |
| 07_cathedral | 8,760 | 19,969 | 60 | 2,242,560 | 32.1 | 78.3 | 0.0 | 21.7 | 0.0 | 0 | 0 | 70 | 271 | 177,463 |
| 08_subwayb | 1,037 | 8,413 | 62 | 265,472 | 33.8 | 99.1 | 0.2 | 0.0 | 0.7 | 3,611 | 0 | 8 | 43 | 21,557 |
| 09_abandoned_buildings | 5,524 | 26,924 | 946 | 1,414,144 | 32.2 | 75.3 | 0.2 | 24.5 | 0.0 | 11,997 | 6 | 44 | 306 | 114,045 |
| 10_sewagetreatmentplant | 1,576 | 8,543 | 429 | 403,456 | 29.7 | 96.8 | 0.9 | 2.2 | 0.1 | 11,269 | 0 | 14 | 91 | 33,414 |
| 11_slaughter_house | 9,513 | 24,154 | 408 | 2,435,328 | 32.0 | 80.0 | 0.4 | 19.6 | 0.0 | 49,728 | 5 | 76 | 313 | 192,681 |
| 12_train_station | 8,228 | 20,771 | 339 | 2,106,368 | 34.2 | 77.4 | 0.2 | 22.5 | 0.0 | 41,127 | 3 | 62 | 402 | 168,119 |
| 12_train_stationb | 5,171 | 17,590 | 186 | 1,323,776 | 33.5 | 83.0 | 0.4 | 16.6 | 0.0 | 41,188 | 10 | 39 | 271 | 106,904 |
| 13_downtown | 9,497 | 19,342 | 274 | 2,431,232 | 36.1 | 89.2 | 0.2 | 10.5 | 0.1 | 15,440 | 9 | 67 | 834 | 193,494 |
| 14_subwayc | 1,172 | 17,675 | 130 | 300,032 | 30.8 | 98.8 | 0.2 | 0.0 | 1.0 | 7,081 | 0 | 10 | 72 | 24,637 |
| 15_crypt | 1,043 | 7,043 | 548 | 267,008 | 30.1 | 99.0 | 0.1 | 0.9 | 0.0 | 6,507 | 1 | 9 | 48 | 21,979 |
| 16_bridge | 10,578 | 15,184 | 402 | 2,707,968 | 32.9 | 73.1 | 0.1 | 26.8 | 0.0 | 16,099 | 0 | 82 | 514 | 212,756 |
| 17_entrance | 3,030 | 17,818 | 712 | 775,680 | 34.0 | 85.4 | 0.2 | 14.4 | 0.0 | 16,997 | 1 | 23 | 159 | 62,862 |
| 18_temple | 4,580 | 10,920 | 54 | 1,172,480 | 34.8 | 84.0 | 0.2 | 15.8 | 0.0 | 2,537 | 2 | 34 | 272 | 92,872 |
| 19_lobby | 4,716 | 20,077 | 710 | 1,207,296 | 31.5 | 98.4 | 0.0 | 1.6 | 0.0 | 40,996 | 1 | 38 | 224 | 97,567 |
| 19_lobbyb | 1,091 | 4,871 | 306 | 279,296 | 32.6 | 98.0 | 0.0 | 2.0 | 0.0 | 16,920 | 0 | 9 | 54 | 23,016 |
| 19_lobbyc | 1,085 | 4,837 | 290 | 277,760 | 31.6 | 97.9 | 0.0 | 2.1 | 0.0 | 16,023 | 0 | 9 | 52 | 22,896 |
| 20_powerstation | 2,737 | 12,542 | 220 | 700,672 | 34.0 | 99.2 | 0.2 | 0.0 | 0.6 | 22,737 | 0 | 21 | 139 | 56,626 |
| 21_laboratory | 2,018 | 14,247 | 1062 | 516,608 | 32.0 | 99.5 | 0.1 | 0.3 | 0.0 | 43,215 | 0 | 16 | 182 | 42,776 |
| 22_office | 3,238 | 14,636 | 968 | 828,928 | 35.7 | 89.5 | 0.8 | 9.6 | 0.0 | 36,793 | 20 | 23 | 141 | 66,412 |
| 23_rooftop | 11,356 | 5,594 | 832 | 2,907,136 | 44.7 | 31.4 | 0.1 | 68.6 | 0.0 | 6,757 | 0 | 65 | 345 | 227,557 |
| 24_ancientcity | 12,182 | 17,806 | 128 | 3,118,592 | 31.3 | 71.0 | 0.0 | 28.8 | 0.2 | 121 | 0 | 99 | 1694 | 247,197 |
| 24b_arena | 1,591 | 2,039 | 122 | 407,296 | 26.9 | 75.7 | 0.3 | 23.9 | 0.1 | 0 | 1 | 15 | 101 | 32,788 |
| 25_ancientone | 11,643 | 8,144 | 122 | 2,980,608 | 34.8 | 49.2 | 0.0 | 33.0 | 17.8 | 16 | 0 | 86 | 2843 | 233,756 |
| credits | 63 | 104 | 24 | 16,128 | 15.8 | 32.4 | 0.0 | 67.6 | 0.0 | 0 | 0 | 1 | 3 | 1,694 |
| drop | 189 | 390 | 0 | 48,384 | 29.8 | 100.0 | 0.0 | 0.0 | 0.0 | 0 | 0 | 2 | 4 | 4,211 |
| end | 243 | 326 | 160 | 62,208 | 30.9 | 36.9 | 0.0 | 63.1 | 0.0 | 17,521 | 0 | 2 | 6 | 5,290 |
| **Total** | 175,510 | 506,864 | | 44,930,560 | 34.0 | | | | | | | 1,321 | 11,740 | 3,581,984 |

The whole campaign surveys 44.9 M rays in 1.3 s of tracing; the 11.7 s total is mostly world
loading, placement resolution and mesh building (Ancient City and Ancient One load slowest).
Every file is under 250 KB (PLAC only).

Notes for G7:

- **Misses.** 06_Airship1 (15.9 %) and 25_AncientOne (17.8 %) send many rays into the void with
  no sky face, and the subway worlds lose about 0.6 % through gaps. D9's fallback (the average sky
  colour) or a miss policy must be fixed in G7.
- **Inside probes.** 22_Office has 20 probes and 12_Train_StationB 10 with mostly back-face hits:
  candidates for the G7 sample-validity rule rather than placement changes.
- **Layers.** Museum, Slaughter House, Laboratory and Lobby pass 40–50 k layers per 1–2.5 M rays;
  only four rays (21_Laboratory) reached the eight-layer limit.

## Decisions and adaptations

- The plan's `--gi-placement` is the existing `--gi-placement-dir` (lot G4).
- `GI_RESULT` adds `status=cancelled` (exit 12) to the plan's `baked|current|failed`.
- F8 editing of `excluded_models` comes with the G10 Bake panel; G5 reads the list.
- Albedo is the raw stored RGB; the colour space and the alpha/albedo transmission formula for
  layers are G7 decisions.
- Unbound world models (including WorldModelDebris fragments past the exact name) and containers
  are not in the bake scene.
