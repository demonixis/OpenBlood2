# GI lot GD2: re-baked direct bake

2026-11-07, lot GD2 of the [GI plan](../gi_plan.md) (D1 (c), decision D17), after the user accepted
Gate F (the GD1 term). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#re-baked-direct-bake-lot-gd2). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal. `original` stays the default direct source until
Gate G.

## What landed

- **Setting:** `GiBakeSettings::direct` (`original` | `rebaked`), `--gi-direct`, `bake_all.py
  --direct`, the F8 Bake panel's Direct light choice. Only re-baked bakes add the `direct rebaked`
  settings line, so original files keep their bytes and their settings hash.
- **Direct pass** (`gi_rebake_direct`): after the receivers, before the bounce passes, on the
  bake's jobs, cancel flag and progress (`GI_PROGRESS stage=direct`). D′ = max(0, min(T, 255) −
  A_eff)/255 per LMAP texel; texels in solid space or that cannot be placed keep D. D′ goes through
  RGB9E5 before the passes, so E′ is the bounce of exactly what the file stores.
- **`DIR ` section** after LMAP: format 1, count, RGB9E5 D′ in LMAP order; strict reader, count
  checked against LMAP by the runtime.
- **Composition:** `gi_compose_direct` on texels of a re-baked file in Final, Lighting and Direct;
  Original, Indirect, Validity, corners and DETAIL LOW are unchanged. `--gi-preview`, the F8
  Runtime panel (`Direct light: re-baked (DIR section)`) and the status line follow the file.
- **Deterministic shadows:** the GD1 test used `CollisionWorld::sweep`, game code built without
  `-ffp-contract=off`. GD2 replaces it with `gi_segment_blocked`, a two-sided any-hit trace over a
  `GiBvh` of each BSP's occluding fan triangles in `gi_direct.cpp` (built with
  `-ffp-contract=off`), so the bytes depend on neither jobs nor build.
- **Tests:** `gi_direct.bake` (headless); additions to `gi_bake.settings`,
  `gi_bake_process.parse`, `gi_file.read_refusals` (seven tags), `gi_overlay` (the panel's
  argument), `config.gi` and `gi_bake_all_test.py`; `gi_bake.retail_direct` (private); `gpu.gi`
  (private, GPU) with a re-baked file.

## The touch rule

The first BVH version made `gi_direct.retail` fail: SubwayA's interior fit fell to 94.8 %
(the test asks 95 %). A scratch comparison against the sweep found 2,565 of 2,567 disagreements
were new hits at t < 10⁻⁴: samples lying exactly on faces with flags 0x1081 (distance 0), whose
segment the sweep treats as touching, not crossing. Ignoring hits within 0.01 units of the
sample (`gi_direct_limits::touch`) reproduces it: 25 of SubwayA's 459,762 tests, 14 of Office's and
50 of Airship2's still differ, and the audit total is unchanged at 84.8 % (interior 93.6 %, shadow
edges 69.6 %). The deterministic tracer also runs the 36-world audit in 3.2 s instead of 65 s.

## Identities and determinism

- **Composition identity** (`gi_direct.bake`): D′ = (O − A_eff)/255 composes O exactly at f = 1 and
  E = 0 for every O, 13 ambients (NaN and infinities included), four k and three zero E: 39,936
  cases, also through RGB9E5. Elsewhere `gi_compose_direct` stays within one step of `gi_compose`:
  158,553 of 159,744 combinations equal, the rest one step off at exact .5 boundaries (float D).
- **Passes:** with D′ = exact D, `bake_gi_lightmaps` and `gi_light_state` give the original E and
  light state bit for bit.
- **Original files keep their bytes:** Preview bakes of 01_SubwayA and 22_Office after GD2 equal the
  pre-GD2 baseline (SHA-256 `9fdd8217…a959`, `bddd6953…16d8`), in the dev and Release builds.
- **Jobs and builds:** re-baked 22_Office Preview bakes are identical for 1, 2 and 7 jobs and in
  Release (`5e7a63b0…0424`); `gi_bake.retail_direct` checks 01_SubwayA for 1, 2 and 7 jobs.
- **Frames** (`gpu.gi`, Apple M5, Metal): for 02_CityHub and 01_SubwayA, original and enhanced
  renderer, high and low detail, a DIR of O − A_eff with E = 0 gives the reference frame byte for
  byte; the re-baked D′ changes the lightmapped frame, keeps DETAIL LOW, and the Original view
  restores it.
- **Cancel** during the direct pass exits 12 and keeps the previous file.

## The 36 worlds at Final

`tools/gi/bake_all.py --quality final --direct rebaked` (Release, committed placement files, one
world at a time, 10 threads): 36 of 36 pass in **207.4 s**, against 194.0 s for the original Final
bake earlier the same day. The difference is mostly noise: back-to-back bakes of 12_Train_Station
(+13.6 %, then +1.6 %) and 10_SewageTreatmentPlant (+4.3 %, then +1.3 %) vary more between runs
than between sources, and the bounce passes trace the same rays.

| Over the 36 worlds | Value |
| --- | --- |
| Light and DirLight objects | 13,473 |
| LMAP texels | 10,307,495 |
| Occluder triangles | 1,126,174 |
| Visibility tests | 40,615,023 |
| Direct pass, summed | 1.22 s (at most 87 ms, 11_Slaughter_House) |
| Kept texels (solid sample or unplaced centre) | 200,059 (1.9 %) |
| Saturated texels (T above 255 in a channel) | 150,235 (1.5 %) |
| File sizes | 63.6 MB → 104.8 MB (the DIR section adds 4 bytes per texel) |

The baked files stay local (decision D3).

## A/B captures

`--bench --frames 30 --capture` at each world's start, 1280×720, original renderer, k = 2, f = 1:
GI off (the 1998 lightmaps), GI Final from the original Final file, the re-baked file at k = 0
(D′ with the ambient floor) and GI Final from the re-baked file. The captures are retail content
and stay local.

| World | Re-baked k = 0 against GI off | Final re-baked against Final original |
| --- | --- | --- |
| 01_SubwayA | 67 % of pixels change, 88 % of those brighter, mean absolute difference 1.0 | 86 %, 93 % brighter, 1.3 |
| 02_CityHub | 77 %, 85 % brighter, 0.7 | 81 %, 89 % brighter, 0.7 |
| 06_Airship2 | 92 %, 53 % brighter, 4.1 | 96 %, 54 % brighter, 4.1 |
| 23_Rooftop | 36 %, 62 % brighter, 0.8 | 38 %, 58 % brighter, 0.8 |

SubwayA and CityHub change little and slightly brighten; 06_Airship2, GD1's worst fit (72.4 %),
changes most: about half the changed pixels darken and the frame is 1.6 levels darker overall.

## Gate G

The user plays the A/B and picks the default direct source: keep `original` (D1 (a), the 1998
lightmaps under the bounce) or make `rebaked` the default for `bake_all.py`, the F8 panel and the
packaged files. Either way the Original view stays the stored light and `original` files stay
current. GD3 (a 10-unit direct grid) waits for Gate G and a separate approval.
