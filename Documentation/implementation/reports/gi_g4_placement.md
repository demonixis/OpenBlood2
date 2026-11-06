# GI lot G4: probe placement

2026-11-06, lot G4 of the [GI plan](../gi_plan.md) (Gate A). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#probe-placement-lot-g4). Host: Apple M5, macOS.

## What landed

- **`engine/gi_placement.{h,cpp}`**: the placement file (canonical writer, strict bounded
  reader, statuses, atomic save, placement hash), the placement scene (root BSP, clearance and
  world-model validity, room fitting, visibility), the light-probe lattice with relocation,
  volumes and the budget ladder, automatic and manual reflection probes, and the `.ob2gi` PLAC
  section codec for G5.
- **`engine/bounded_json.{h,cpp}`**: the bounded JSON parser, moved out of the material
  manifest reader and shared with the placement reader (the manifest tests are unchanged).
- **`--gi-place WORLD|single|all`** with `--gi-reset`, `--gi-dry-run`, `--json` and
  `--gi-placement-dir` / `gi_placement_dir=`. The default folder is the checkout's
  `data/gi/placement` (decision 8, now created with its readme), else the user directory.
- **F8**: the Light probes and Reflection probes panels (settings, volumes, automatic placement,
  manual probes and their boxes, Save/Revert/Reset, Dirty), placement markers, and the
  placement summary in the Status panel and the Pinned strip.
- **Tests:** `gi_placement.lattice`, `world_models`, `reflections`, `json`, `files`, `section`,
  the private `gi_placement.retail`, extended `gi_overlay.draw`, `gi_overlay.retail` and
  `config.gi`.

No placement file is committed yet: the defaults wait for the Gate A decisions below.

## Results

| Check | Result |
| --- | --- |
| Headless suite, dev build | 1,157 of 1,157 pass |
| Headless suite, Release preset (`BLOOD2_INSPECTOR` off, GI tools on) | 1,157 of 1,157 pass; `--gi-place 01_SubwayA --gi-dry-run` runs |
| ASan/UBSan debug build | `gi_placement.*`, `surface.*`, `gi_overlay.*`, `config.*`: 23 of 23 pass; private `gi_overlay.retail` and `gi_placement.retail` (58 worlds, 7 min) clean |
| Private GI tests (dev build with data) | 25 of 25 pass, `gi_placement.retail` in 26 s |
| GPU frames | `gpu.surface`, `gpu.post`, `gpu.surface_effect`, `gpu.surface_effect_private`: 4 of 4 pass |
| `--gi-place all` (Release, written to a scratch folder) | 58 worlds, exit 0, 13.5 s wall; a second run reports `unchanged` |

## All-world audit

Default placement (spacing 128, budget 16,384, clearance 8, cap 42), Release build. "At 128" is
the probe count with an unlimited budget; bold marks a coarsened spacing or capped reflection
probes. Excluded and Unknown are zero everywhere (no volumes; every query decided). "Small" and
"Overlapping" count reflection room candidates (one per light probe, at most 4,096).

**Single-player (36 worlds, decision D7):** 175,510 light probes (84,299 relocated), 773
reflection probes, 9.1 s light and 1.0 s reflection time. Slowest: 25_AncientOne (2.8 s),
24_AncientCity (1.6 s).

| World | At 128 | Spacing | Probes | Relocated | Solid | Clearance | World model | Reflections | Open | Small | Overlapping | Capped | Light ms | Refl. ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 01_subwaya | 1,031 | 128 | 1,031 | 743 | 436 | 11 | 10 | 6 | 0 | 424 | 601 | 0 | 19 | 17 |
| 02_cityhub | 6,185 | 128 | 6,185 | 2,965 | 37,882 | 13 | 0 | 40 | 1 | 477 | 2,576 | 0 | 205 | 37 |
| 02_cityhubb | 4,273 | 128 | 4,273 | 2,531 | 26,725 | 10 | 0 | 32 | 1 | 494 | 1,611 | 0 | 143 | 24 |
| 02_cityhubc | 6,951 | 128 | 6,951 | 3,886 | 40,355 | 22 | 0 | 34 | 1 | 565 | 2,877 | 0 | 209 | 41 |
| 03_museum | 4,637 | 128 | 4,637 | 3,081 | 20,004 | 42 | 1 | 41 | 0 | 357 | 1,921 | 0 | 92 | 37 |
| 04_steamtunnels | 5,077 | 128 | 5,077 | 3,400 | 81,628 | 189 | 6 | 42 | 0 | 944 | 1,535 | **18** | 323 | 35 |
| 05_cdm | 3,336 | 128 | 3,336 | 2,478 | 36,855 | 65 | 0 | 42 | 0 | 735 | 2,554 | **5** | 105 | 44 |
| 05_cdmb | 17,922 | **160** | 8,847 | 1,882 | 25,241 | 24 | 0 | 4 | 0 | 190 | 2,755 | 0 | 93 | 16 |
| 06_airship1 | 52,135 | **224** | 11,151 | 2,100 | 1,856 | 17 | 0 | 2 | 1 | 644 | 3,071 | 0 | 144 | 40 |
| 06_airship2 | 2,161 | 128 | 2,161 | 1,679 | 15,964 | 91 | 0 | 31 | 0 | 773 | 1,357 | 0 | 73 | 25 |
| 07_cathedral | 8,760 | 128 | 8,760 | 4,070 | 25,410 | 18 | 0 | 25 | 3 | 259 | 2,636 | 0 | 140 | 40 |
| 08_subwayb | 1,037 | 128 | 1,037 | 749 | 436 | 5 | 10 | 6 | 0 | 430 | 601 | 0 | 11 | 18 |
| 09_abandoned_buildings | 5,524 | 128 | 5,524 | 3,227 | 51,291 | 24 | 1 | 42 | 0 | 537 | 2,162 | **21** | 173 | 32 |
| 10_sewagetreatmentplant | 1,576 | 128 | 1,576 | 936 | 14,594 | 50 | 4 | 20 | 0 | 312 | 1,244 | 0 | 53 | 15 |
| 11_slaughter_house | 9,513 | 128 | 9,513 | 5,351 | 20,022 | 169 | 0 | 27 | 0 | 290 | 2,854 | 0 | 153 | 54 |
| 12_train_station | 8,228 | 128 | 8,228 | 3,634 | 107,167 | 9 | 0 | 42 | 0 | 211 | 2,486 | **4** | 271 | 38 |
| 12_train_stationb | 5,171 | 128 | 5,171 | 2,643 | 66,028 | 14 | 0 | 41 | 1 | 232 | 2,313 | 0 | 190 | 29 |
| 13_downtown | 9,497 | 128 | 9,497 | 6,444 | 145,020 | 53 | 0 | 42 | 0 | 310 | 2,804 | **10** | 722 | 30 |
| 14_subwayc | 1,172 | 128 | 1,172 | 808 | 940 | 16 | 0 | 11 | 0 | 685 | 476 | 0 | 16 | 36 |
| 15_crypt | 1,043 | 128 | 1,043 | 676 | 4,412 | 3 | 2 | 10 | 1 | 224 | 809 | 0 | 22 | 12 |
| 16_bridge | 10,578 | 128 | 10,578 | 3,421 | 54,008 | 52 | 0 | 11 | 1 | 174 | 3,341 | 0 | 382 | 33 |
| 17_entrance | 3,030 | 128 | 3,030 | 1,792 | 20,154 | 45 | 3 | 25 | 0 | 311 | 2,694 | 0 | 78 | 31 |
| 18_temple | 4,580 | 128 | 4,580 | 1,944 | 46,243 | 137 | 0 | 12 | 1 | 348 | 1,930 | 0 | 199 | 21 |
| 19_lobby | 4,716 | 128 | 4,716 | 2,552 | 29,947 | 40 | 3 | 38 | 2 | 185 | 2,135 | 0 | 124 | 28 |
| 19_lobbyb | 1,091 | 128 | 1,091 | 691 | 7,865 | 20 | 0 | 11 | 0 | 111 | 969 | 0 | 29 | 10 |
| 19_lobbyc | 1,085 | 128 | 1,085 | 685 | 7,463 | 20 | 0 | 11 | 0 | 105 | 969 | 0 | 28 | 10 |
| 20_powerstation | 2,737 | 128 | 2,737 | 1,834 | 27,786 | 67 | 0 | 20 | 1 | 688 | 2,029 | 0 | 82 | 27 |
| 21_laboratory | 2,018 | 128 | 2,018 | 1,041 | 25,298 | 12 | 0 | 27 | 0 | 506 | 1,485 | 0 | 117 | 21 |
| 22_office | 3,238 | 128 | 3,238 | 1,812 | 15,889 | 13 | 0 | 17 | 0 | 107 | 3,114 | 0 | 69 | 28 |
| 23_rooftop | 88,796 | **256** | 11,356 | 3,770 | 4,385 | 9 | 0 | 1 | 0 | 4 | 3,781 | 0 | 267 | 24 |
| 24_ancientcity | 12,182 | 128 | 12,182 | 7,226 | 68,724 | 94 | 0 | 42 | 0 | 280 | 3,710 | **29** | 1,621 | 62 |
| 24b_arena | 1,591 | 128 | 1,591 | 792 | 7,349 | 237 | 0 | 8 | 0 | 88 | 1,495 | 0 | 76 | 17 |
| 25_ancientone | 54,901 | **224** | 11,643 | 3,172 | 6,714 | 11 | 0 | 7 | 1 | 939 | 2,935 | 0 | 2,845 | 41 |
| credits | 63 | 128 | 63 | 48 | 21 | 0 | 0 | 1 | 0 | 0 | 62 | 0 | 0 | 0 |
| drop | 189 | 128 | 189 | 91 | 27 | 0 | 0 | 1 | 0 | 0 | 188 | 0 | 1 | 1 |
| end | 243 | 128 | 243 | 145 | 81 | 0 | 0 | 1 | 0 | 0 | 242 | 0 | 1 | 1 |

**Multiplayer (22 worlds, best effort):** 90,620 light probes (31,632 relocated), 215 reflection
probes, 2.3 s light and 0.5 s reflection time.

| World | At 128 | Spacing | Probes | Relocated | Solid | Clearance | World model | Reflections | Open | Small | Overlapping | Capped | Light ms | Refl. ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| bb_area15 | 543 | 128 | 543 | 426 | 1,120 | 46 | 1 | 9 | 0 | 235 | 299 | 0 | 14 | 5 |
| bb_bloodarena | 4,468 | 128 | 4,468 | 2,966 | 7,517 | 15 | 0 | 17 | 0 | 396 | 1,821 | 0 | 126 | 31 |
| bb_bloodkeep | 540 | 128 | 540 | 347 | 1,446 | 9 | 0 | 13 | 0 | 310 | 217 | 0 | 8 | 7 |
| bb_bodies | 1,557 | 128 | 1,557 | 1,015 | 3,150 | 5 | 0 | 9 | 0 | 130 | 1,418 | 0 | 28 | 12 |
| bb_bridge | 2,280 | 128 | 2,280 | 626 | 3,477 | 3 | 0 | 3 | 0 | 91 | 2,186 | 0 | 34 | 21 |
| bb_brownstar | 975 | 128 | 975 | 657 | 1,913 | 28 | 0 | 9 | 0 | 113 | 853 | 0 | 21 | 12 |
| bb_cabana | 3,196 | 128 | 3,196 | 1,947 | 2,861 | 33 | 0 | 4 | 3 | 231 | 2,961 | 0 | 563 | 44 |
| bb_chosenarena | 3,500 | 128 | 3,500 | 2,117 | 8,645 | 176 | 0 | 42 | 0 | 375 | 3,068 | **15** | 82 | 53 |
| bb_condemned | 9,230 | 128 | 9,230 | 3,893 | 10,294 | 60 | 0 | 1 | 0 | 152 | 2,924 | 0 | 406 | 23 |
| bb_crypt | 1,082 | 128 | 1,082 | 724 | 5,151 | 3 | 4 | 11 | 0 | 247 | 824 | 0 | 36 | 12 |
| bb_deathcanyon | 14,493 | 128 | 14,493 | 2,944 | 5,991 | 51 | 0 | 2 | 1 | 21 | 3,601 | 0 | 183 | 29 |
| bb_drain | 1,323 | 128 | 1,323 | 973 | 2,077 | 0 | 0 | 3 | 0 | 228 | 1,092 | 0 | 25 | 11 |
| bb_dustbowl | 4,488 | 128 | 4,488 | 824 | 3,122 | 78 | 0 | 1 | 0 | 25 | 2,218 | 0 | 50 | 29 |
| bb_fields | 22,068 | **160** | 11,385 | 1,139 | 6,015 | 0 | 0 | 1 | 1 | 0 | 3,794 | 0 | 17 | 4 |
| bb_jgrave | 1,563 | 128 | 1,563 | 938 | 2,451 | 11 | 0 | 13 | 0 | 142 | 1,408 | 0 | 14 | 15 |
| bb_midgard | 1,326 | 128 | 1,326 | 672 | 2,180 | 14 | 8 | 7 | 0 | 40 | 1,279 | 0 | 34 | 11 |
| bb_power | 607 | 128 | 607 | 471 | 816 | 5 | 0 | 11 | 0 | 101 | 495 | 0 | 7 | 6 |
| bb_rooftop | 27,907 | **160** | 13,036 | 691 | 4,512 | 2 | 0 | 2 | 1 | 3 | 3,254 | 0 | 74 | 7 |
| ctf_cabana | 5,327 | 128 | 5,327 | 3,710 | 8,105 | 57 | 0 | 3 | 2 | 143 | 2,518 | 0 | 156 | 38 |
| ctf_deadcities | 3,790 | 128 | 3,790 | 2,629 | 10,663 | 127 | 0 | 13 | 0 | 390 | 3,387 | 0 | 371 | 47 |
| ctf_dmx | 1,661 | 128 | 1,661 | 773 | 13,679 | 50 | 0 | 21 | 4 | 211 | 1,429 | 0 | 62 | 16 |
| ctf_jam | 4,250 | 128 | 4,250 | 1,150 | 9,610 | 0 | 0 | 20 | 0 | 148 | 1,957 | 0 | 38 | 29 |

### Observations

- **Budget.** Six worlds exceed 16,384 probes at 128 and coarsen: 05_CDMB and the two
  multiplayer rooftops/fields to 160, 06_Airship1 and 25_AncientOne to 224, 23_Rooftop to 256.
  They are large open volumes (sky, canyons) where most probes float in air.
- **Relocation.** About half of the probes are relocated (48 % single-player): nodes that land
  in or against walls move up to 3s/8 into the room. Relocation from a solid node has no
  visibility test, so a wall thinner than s/4 can be crossed (accepted risk, documented).
- **Rejections.** Clearance (1,602 single-player) and world-model (40) rejections are rare;
  almost every rejected node is solid. World-model rejections are door and crate interiors.
- **Reflection probes.** Seven worlds reach the cap of 42: 04_SteamTunnels (18 more rooms
  capped), 05_CDM (5), 09_Abandoned_Buildings (21), 12_Train_Station (4), 13_Downtown (10),
  24_AncientCity (29) and bb_chosenarena (15). Corridors under 192 units wide or 96 high get no
  automatic probe ("Small"); open rooms (reaching the world bounds) get probes without box
  projection.
- **Cost.** The light lattice dominates (classification and relocation); placement of every
  world takes 13.5 s, so it never needs to run at load or build time.

## Gate A: decisions for the user

1. **Spacing and budget.** Keep 128 / 16,384 (six open worlds coarsen to 160–256), raise the
   default budget (32,768 keeps CDMB and the multiplayer worlds at 128; Airship1 and AncientOne
   would still coarsen), or set per-world budgets in their files (already possible).
   Recommendation: keep 16,384 by default and raise it per world where G9's model lighting
   shows a need.
2. **Two-level lattice for open worlds.** Fine 128 near geometry and coarse in open air, instead
   of coarsening the whole world. Recommendation: decide in G9, once probe lighting can be
   compared; the PLAC section would need a new version.
3. **Reflection cap.** Keep 42 per world (seven worlds capped), or lower it for memory
   (about 0.26 MB per 64² RGBA16F probe with mips, 11 MB at 42). Recommendation: keep 42.
4. **Minimum room.** 192×96 skips corridors; lowering it to 128 wide adds probes in corridors
   at the cost of reaching the cap in more worlds. Recommendation: keep 192×96 and add manual
   probes in F8 where corridors need them.
5. **Commit.** After review, run `--gi-place single` and commit the 36 single-player files in
   `data/gi/placement` (decision D3 (a)).

## Not done in G4

- **SubwayA and CityHub have not been reviewed in F8 by the user** (a Gate A criterion). The
  panels and markers are covered by headless ImGui frames and `gi_overlay.retail`, not by a
  hand-driven session in a game window.
- Per-object bake exclusion in the placement file (decision D8) arrives with the bake scene
  (G5).
- iOS, Android and Quest builds were not rebuilt (GI tools are off there; the engine change is
  one source file).
