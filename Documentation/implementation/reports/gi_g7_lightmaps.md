# GI lot G7: lightmap core

2026-11-07, lot G7 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#gi-lightmap-core-lot-g7). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, 10 bake threads.

## What landed

- **`engine/gi_lightmap.{h,cpp}`**: the receivers (blocks per lightmapped polygon, corner sets per
  vertex-lit and DETAIL LOW polygon, samples, links and neighbours), the gathering passes, the
  dilation, the RGB9E5 LMAP/`VTX ` sections with strict readers and world checks, and
  `gi_compose`, the D1 composition G8 will use.
- **`engine/gi_sky.{h,cpp}`**: decision D9. `gi_sky_setup` replays the DemoSkyWorldModel and
  SkyPointer registrations, the sky definition and SkyFog; `GiSky` paints a 6×64² cube from the
  mapped sky eye, with the solid-angle average for uncovered directions and the sky scale.
- **`engine/gi_preview.{h,cpp}`** and `--gi-preview WORLD`: Original | GI | Indirect PNG views of a
  baked file from the start points, with `--gi-k`/`--gi-f`, for Gate B.
- **Bake:** `bake_gi_world` now runs the sky, receivers and passes after the survey and writes
  INFO, PLAC, LMAP and `VTX `. Settings version 2 adds the sky scale (`--gi-sky`, custom only;
  the presets use 1.00); `gi_algorithm_version` is 2, so G5 files are stale.
- **`WorldMesh::build`** can report each emitted triangle's polygon, corners and light kind
  (`WorldTriangleSource`, `WorldFaceLight`); the mesh is unchanged.
- **G5 corrections:** SkyPointer targets (doors and brushes drawn in the sky pass) are Sky ranges,
  no longer Main; face records keep their `WorldMesh` triangle.
- **Tests:** 14 headless `gi_lightmap` cases and the private `gi_lightmap.retail`; `gi_bake` and
  `config.gi` extended for the sky scale and `--gi-preview`.

## Results

| Check | Result |
| --- | --- |
| Headless suite, RelWithDebInfo (`dev`) | 1,179 / 1,179 |
| Headless suite, inspector-free Release | 1,179 / 1,179 |
| ASan + UBSan (`asan`, Debug): every `gi_*` and `config` test | 51 / 51, no report |
| Private GI tests (`dev`, retail data), `gi_lightmap.retail` included | 6 / 6 |
| GPU (`dev`, real display): `gpu.surface`, `gpu.post`, `gpu.surface_effect`, `gpu.surface_effect_private` | 4 / 4 |
| Preview files (SubwayA, CityHub): ASan Debug, Release, `dev` with 1 and 10 jobs | byte-identical |
| Every single-player world at Preview (36) | 36 baked in 37.0 s |
| `bake_all.py`, 36 single-player worlds at Final, 10 threads | 36 PASS in 196.0 s, 54.1 MB |
| `--gi-preview` (3 worlds × 3 k/f pairs, Cathedral at 1 and 3 bounces) | 11 runs, all ok |

## Analytic accuracy

Measured worst errors of the headless cases (the tests' limits are in parentheses):

| Case | Lightmap receiver | Vertex receiver |
| --- | --- | --- |
| Furnace, bounces 1–3, 64 rays | exact to 2e-6 relative (2e-6) | exact to 2e-6 relative (2e-6) |
| Box: floor under a bright ceiling and black walls, 4,096 rays | 7.7e-4 (2e-3) | 5.0e-4 (2e-3) |
| Two planes: emitter above a receiver, uniform sky, 4,096 rays | 6.3e-4 (3e-3) | 3.4e-4 (3e-3) |
| Leak: dark room beside a lit room | E = 0 exactly | — |
| Identity: E = 0, f = 1 over 232,960 (O, A, k) cases | exact | exact |

## Every single-player world at Preview

`--bake-gi <world> --gi-quality preview` (1 bounce, 64 rays) for the 36 committed placements, one
world at a time. Solid, back-invalid and filled count elements; unfilled counts receivers left at
0 after the neighbour fill. Total ms includes loading, placement, mesh, survey and sky.

| World | Texels | Corners (LOW) | Solid | Back-invalid | Filled | Unfilled | Sky cover | Lightmap ms | Total ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 01_subwaya | 145,946 | 14,847 (14,723) | 3,522 | 3,757 | 3,221 | 237 | 0.000 | 326 | 443 |
| 02_cityhub | 344,666 | 39,152 (35,927) | 10,051 | 2,370 | 2,059 | 831 | 1.000 | 747 | 1,128 |
| 02_cityhubb | 307,753 | 35,750 (33,666) | 15,314 | 1,598 | 1,788 | 1,110 | 1.000 | 669 | 939 |
| 02_cityhubc | 399,093 | 42,682 (40,235) | 8,462 | 2,540 | 2,147 | 723 | 1.000 | 916 | 1,290 |
| 03_museum | 438,172 | 44,956 (41,677) | 7,737 | 10,475 | 6,452 | 990 | 1.000 | 1,002 | 1,272 |
| 04_steamtunnels | 604,562 | 69,305 (68,385) | 5,330 | 10,358 | 10,773 | 436 | 0.000 | 1,458 | 2,005 |
| 05_cdm | 348,639 | 46,595 (42,770) | 6,840 | 7,348 | 3,281 | 1,307 | 1.000 | 806 | 1,069 |
| 05_cdmb | 30,483 | 3,709 (2,339) | 1,526 | 6,113 | 4,404 | 180 | 0.991 | 62 | 198 |
| 06_airship1 | 132,215 | 28,195 (13,696) | 3,746 | 2,934 | 4,136 | 181 | 0.991 | 478 | 743 |
| 06_airship2 | 269,719 | 43,303 (42,295) | 6,618 | 11,615 | 7,827 | 969 | 0.991 | 659 | 859 |
| 07_cathedral | 397,313 | 39,200 (38,925) | 2,954 | 2,673 | 1,640 | 266 | 0.550 | 955 | 1,263 |
| 08_subwayb | 154,868 | 16,793 (16,669) | 3,203 | 4,269 | 3,616 | 224 | 0.000 | 328 | 394 |
| 09_abandoned_buildings | 490,134 | 53,009 (52,761) | 12,474 | 8,537 | 7,333 | 951 | 0.550 | 1,062 | 1,452 |
| 10_sewagetreatmentplant | 164,896 | 16,721 (15,530) | 1,024 | 7,118 | 5,796 | 161 | 0.550 | 367 | 486 |
| 11_slaughter_house | 621,627 | 47,992 (47,540) | 17,167 | 20,072 | 21,069 | 756 | 0.913 | 1,341 | 1,719 |
| 12_train_station | 541,796 | 40,683 (40,472) | 5,048 | 6,248 | 3,417 | 415 | 1.000 | 1,146 | 1,630 |
| 12_train_stationb | 409,688 | 34,661 (34,573) | 10,758 | 6,783 | 7,331 | 562 | 1.000 | 848 | 1,184 |
| 13_downtown | 486,647 | 38,596 (37,266) | 8,740 | 6,082 | 10,131 | 268 | 0.002 | 1,042 | 1,989 |
| 14_subwayc | 284,587 | 35,295 (35,043) | 5,706 | 6,835 | 6,141 | 605 | 0.000 | 661 | 779 |
| 15_crypt | 132,057 | 13,485 (12,749) | 4,885 | 1,530 | 3,841 | 274 | 1.000 | 291 | 364 |
| 16_bridge | 324,935 | 29,759 (25,827) | 3,629 | 12,944 | 10,894 | 325 | 1.000 | 792 | 1,338 |
| 17_entrance | 299,484 | 35,851 (32,011) | 12,616 | 9,259 | 13,394 | 594 | 1.000 | 658 | 868 |
| 18_temple | 274,921 | 21,580 (20,232) | 3,015 | 3,246 | 4,056 | 165 | 1.000 | 555 | 872 |
| 19_lobby | 424,799 | 39,886 (33,301) | 14,781 | 5,188 | 6,117 | 767 | 1.000 | 893 | 1,169 |
| 19_lobbyb | 103,417 | 9,503 (7,987) | 4,574 | 776 | 1,516 | 223 | 1.000 | 219 | 292 |
| 19_lobbyc | 103,008 | 9,435 (7,931) | 4,165 | 630 | 1,341 | 199 | 1.000 | 200 | 273 |
| 20_powerstation | 257,062 | 25,114 (20,458) | 8,333 | 12,928 | 16,514 | 367 | 0.000 | 640 | 814 |
| 21_laboratory | 274,229 | 28,251 (26,216) | 20,401 | 3,706 | 9,032 | 957 | 1.000 | 608 | 857 |
| 22_office | 255,222 | 28,168 (25,008) | 10,214 | 5,011 | 4,322 | 943 | 1.000 | 629 | 814 |
| 23_rooftop | 132,110 | 10,674 (8,506) | 2,848 | 2,892 | 2,848 | 164 | 1.000 | 305 | 687 |
| 24_ancientcity | 505,893 | 34,366 (32,266) | 3,840 | 4,030 | 2,536 | 331 | 1.000 | 1,314 | 3,325 |
| 24b_arena | 81,805 | 3,652 (3,648) | 1,136 | 3,897 | 2,405 | 158 | 1.000 | 206 | 328 |
| 25_ancientone | 538,753 | 15,637 (15,637) | 2,071 | 1,472 | 1,672 | 48 | 1.000 | 821 | 4,108 |
| credits | 1,589 | 95 (95) | 0 | 0 | 0 | 0 | 0.918 | 2 | 8 |
| drop | 12,772 | 780 (780) | 0 | 0 | 0 | 0 | 0.000 | 18 | 24 |
| end | 12,635 | 404 (404) | 4,325 | 0 | 8 | 64 | 0.392 | 12 | 23 |
| **Total** | 10,307,495 | 998,084 (927,548) | 237,053 | 195,234 | 193,058 | 16,751 | | 23,036 | 37,006 |

The whole set bakes in 37.0 s, 23.0 s of it in the lightmap passes; files total 54.1 MB. Ancient
City and Ancient One spend most of their time loading, as in G5.

- **Validity.** 2.1 % of the 11.3 M elements are solid and 1.7 % see more than 10 % back faces.
  98.2 % of traced elements see no back face at all; only 1.8 % see 10 % or more (the tenths are
  0.9824, 0.0063, 0.0024, 0.0010, 0.0011, 0.0015, 0.0007, 0.0009, 0.0008, 0.0030). 0.92 % of all
  rays hit a back face.
- **Edge inset.** Moving texel centres that lie within 1 unit of an edge inward, not only those
  outside the polygon, cut back-invalid samples on 07_Cathedral from 9,608 to 2,673, on
  04_SteamTunnels from 19,217 to 10,358 and on 01_SubwayA from 4,629 to 3,757: a centre on an
  edge sits on the plane of the wall it meets and sees that wall's back.
- **DETAIL LOW.** 93 % of the corners belong to lightmapped polygons (DETAIL LOW sets); true
  vertex-lit polygons are rare in retail data.
- **Sky.** 30 worlds draw a sky. 13_Downtown covers 0.2 % of the cube: its two DemoSkyWorldModel
  boxes bind one model name (the first match wins) and the last definition is kept, so the bake
  sees the near-empty sky the runtime draws; misses there take the average of that sliver. Worlds
  without a sky (the subways, 04_SteamTunnels) give misses 0, which settles G5's miss note.

## Final times

`--gi-quality final` (2 bounces, 256 rays):

| World | Elements | Total s | Passes ms | Bytes |
| --- | ---: | ---: | --- | ---: |
| 24_ancientcity | 540,259 | 12.1 | 4,819 + 5,240 | 2,609,567 |
| 04_steamtunnels (most elements) | 673,867 | 11.6 | 5,036 + 5,995 | 3,160,362 |
| 11_slaughter_house (most texels) | 669,619 | 11.5 | 5,716 + 5,290 | 3,145,805 |
| 25_ancientone | 554,390 | 9.6 | 3,021 + 3,166 | 2,583,713 |
| 07_cathedral | 436,513 | 7.6 | 3,460 + 3,734 | 2,134,217 |
| 02_cityhub | 383,818 | 6.4 | 2,754 + 3,198 | 1,861,104 |
| 01_subwaya | 160,793 | 2.6 | 1,158 + 1,304 | 743,782 |

The whole single-player campaign bakes at Final in 196 s (3.3 min) through `bake_all.py`, one
world at a time with 10 threads (the plan estimated 3–15 min); 04_SteamTunnels is the slowest at
13.2 s there. The passes trace 21–54 M rays per second on 10 threads (median 31 M).

## Gate B material

`--gi-preview` dumps of the Final bakes of 01_SubwayA, 02_CityHub and 07_Cathedral (k and f are
composition parameters, applied by G8 without a rebake; bounces and the sky scale are bake
settings). Mean E is over valid texels; "changed" is the share of 8-bit channels the composition
changes and "mean" their mean change.

| World | AmbientLight | Mean texel E | k 1, f 1 | k 2, f 1 | k 1, f 0 |
| --- | --- | --- | --- | --- | --- |
| 01_subwaya | 0 0 0 | 0.148 0.123 0.104 | 65 %, +16.0 | 72 %, +29.2 | as f 1 |
| 02_cityhub | 17 23 23 | 0.111 0.129 0.087 | 20 %, +2.5 | 53 %, +11.3 | 98 %, −6.2 |
| 07_cathedral | 20 20 20 | 0.048 0.054 0.068 | 3 %, +0.2 | 24 %, +2.2 | 99 %, −11.8 |

Bounces on 07_Cathedral (256 rays, sky 1.00): mean texel E 0.041 0.048 0.059 with 1 bounce
(4.2 s), +15 % with 2 (7.6 s), +2 % more with 3 (11.0 s).

- **f = 1** keeps the D1 guarantees (identity at E = 0, never darker). f = 0 drops the authored
  ambient floor wherever the bounce is weaker, which darkens CityHub and Cathedral.
- **k = 1** keeps the units physical. Where AmbientLight is about 20 or more, the floor already
  exceeds most of the bounce, so GI shows little; SubwayA (no ambient) shows the full effect.
- **2 bounces** capture nearly all the energy; a third costs 45 % more time for 2 %.
- **Sky 1.00** draws the sky as the renderer does.

## Gate B decision

On 2026-11-07 the user accepted Gate B with the intensity raised to **k = 2**; f = 1, the Final
preset (2 bounces, 256 rays) and sky scale 1.00 stay. `gi_default_k` and `gi_default_f` hold the
defaults (`--gi-preview` uses them when `--gi-k`/`--gi-f` are absent); G8 composes with them. k
is not a bake setting, so the files above stay current.

## Decisions and adaptations

- **Vertex radiance** is `albedo × min(D + E, 1)` rather than the plan's `albedo·(Dv + Ev)`: the
  composed corner is clamped to 255 like every 8-bit colour, so that is what the face shows.
- **Ray seeds.** Each (element, pass) draws one lattice shift from the world digest; rays are the
  stratified lattice points (the plan's per-ray hash would lose the stratification).
- **The sky scale is a bake setting** (`--gi-sky`, in the settings key): sky light and bounce light
  are mixed in E, so D9's runtime "sky" intensity cannot scale the sky alone. F8 exposes it with
  the G10 Bake panel.
- **Sky fallback.** D9's "average sky colour" is the solid-angle average of the covered part of the
  cube, used for the uncovered directions; misses take the sky.
- **Translucent layers** composite front to back with their texel alpha; a back face behind layers
  keeps the layers' light. Albedo is the raw stored RGB, as the renderer multiplies it.
- **Validity** uses a 10 % back-face limit, decided on the element's first trace; DETAIL LOW sets,
  which feed no lookup, are traced once in the last pass.
- **Edge inset** (1 unit) extends the plan's "clamped inside the polygon" (see the Preview
  results).
- `--gi-preview` is a diagnostic addition for Gate B; the game never uses it.

## Open points

- **Coplanar twins.** Equal-distance hits go to the lower face index, so a ray between two opaque
  faces in one plane facing opposite ways can hit the back of the other. Retail data has 2 drawn
  pairs, both in 09_Abandoned_Buildings; the leak test covers the synthetic case.
- **13_Downtown's sky** is the runtime's own (duplicate sky box); the bake follows it.
- Model shadows, dynamic lights and manifest emission are not baked (D10).
- G8 composes the sections into the world; until then nothing renders GI.
