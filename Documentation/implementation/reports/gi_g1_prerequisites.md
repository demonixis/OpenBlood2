# GI lot G1: prerequisites

2026-11-06, lot G1 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#world-queries-lot-g1). Retail data:
`/Users/yann/Games/Blood2` (58 `worlds/*.dat`). GPU: Apple M5, Metal.

## What landed

- **`engine/gi_world.{h,cpp}`:**
  - `LightmapFrame` and `lightmap_frame`;
  - `point_contents`, `point_clear`, `world_model_bounds` and `world_model_containing`;
  - `world_model_roles`;
  - `world_info_ambient_any_case` and `audit_ambient`;
  - `world_dat_digest`.
- **`WorldMesh::build`** takes its lightmap origin and vertex lightmap coordinates from
  `LightmapFrame`. Nothing else changes.
- **`tests/gi_world_tests.cpp`:** the headless cases `frame`, `contents`, `roles` and `ambient`;
  the private `retail` case; and a `digests` mode.

## Byte identity

`gi_world_tests digests` was recorded before the refactor and again after it. It hashes
every vertex and every lightmap page of `WorldMesh::build`, with and without lightmaps, for the
58 worlds (116 digests). The two runs are **identical**.

These GPU tests pass unchanged on the M5, with the dev build temporarily configured with GPU
tests and the private data:

- `gpu.surface`, `gpu.post`, `gpu.lightmaps`;
- `gpu.world`, `gpu.world_depth`, `gpu.world_alpha`, `gpu.world_alpha_private`, `gpu.world_debris`, `gpu.world_sprite`.

`world_view.subway`, `world_lighting.retail` and `gi_world.retail` also pass.

## Point contents and the earlier survey

The plan's survey counted empty cells on a 128-unit lattice: cell centres at
`lo + 64 + 128 k`, whole cells only, over the root point bounds.

| Measure | Survey | `point_contents` |
| --- | --- | --- |
| Empty cells, 36 single-player worlds | 266,720 | 266,814 |
| Worlds with identical counts (all 58) | — | 57 |
| 24_AncientCity | 8,700 | 8,794 |
| 23_Rooftop / 06_Airship1 / 25_AncientOne | 75,492 / 39,181 / 37,241 | identical |
| GameStartPoints in empty space | 295 / 297 | 295 / 297 |

The one difference, 94 cells in 24_AncientCity, comes from points lying on split planes.
The survey script evaluated them with a different precision, while `point_contents` uses the
engine's double-precision descent with the original −0.0001 bias.

The two start points outside empty space are:

- 02_CityHub at (−3264, 1344, −64);
- 09_Abandoned_Buildings at (−512, 576, 1888).

All 297 start points are clear for an 8-unit cube. 43 lie inside some world model's
conservative bounds, often a large volume brush, which shows why probe validation must not use
the bounds test alone.

## World-model roles (58 worlds)

| Role | Models |
| --- | --- |
| Primary | 58 |
| Sky | 50 |
| Container | 416 |
| Mover | 1,058 (680 Door + 242 RotatingDoor + 136 RotatingBrush) |
| Destructible | 2,497 |
| Unbound | 39 |

## Ambient audit (single-player worlds)

| Worlds | AmbientLight | Darkest texel | Darkest vertex |
| --- | --- | --- | --- |
| 02_CityHub, 02_CityHubB, 02_CityHubC, 03_Museum | 17 23 23 | 16 20 16 | 17 23 23 |
| 04_SteamTunnels | 10 20 20 | 8 20 16 | 10 20 20 |
| 05_CDM, 09, 12, 12b, 14_SubwayC, 15_Crypt | 10 10 10 | 8 8 8 | 10 10 10 |
| 07_Cathedral | 20 20 20 | 16 20 16 | 20 20 20 |
| 11_Slaughter_House | 10 10 15 | 8 8 8 | 10 10 15 |
| 24_AncientCity, 24b_Arena (lowercase key) | 15 10 5 | 8 8 0 | 15 10 5 |
| drop | 15 15 15 | 8 12 8 | 28 28 28 |
| 01, 08, 10, 13, 20, 22 | 0 0 0 | 0 0 0 | 0 0 0 |
| 17, 18, 19, 19b, 19c, 21 (lowercase key) | 0 0 0 | 0 0 0 | 0 0 0 |
| 05_CDMB, 06_Airship1/2, 16, 23, 25, credits, end | none | 0 0 0 | 0 0 0 |

Across all 58 worlds (22.6 M texels and 4.3 M vertices), **no sample is below the ambient value**:

- texels by more than their RGB565 step;
- vertices by more than 0.5.

The fill is therefore a floor at ×1, and the lightmap value is AmbientLight truncated to
RGB565. The multiplayer maps behave the same way; for example, bb_condemned has 50 67 134
against a darkest texel of 49 65 132.

**Consequence for G7:** `D = O − min(O, A)` with A at ×1 for both texels and vertices. No
×0.5 variant is needed.

The lowercase-key worlds carry their floor in the baked data, so the original preprocessor read
the key case-insensitively. This supports the optional lot GA. GA still needs the user's
approval, because it changes original-renderer model lighting in 24_AncientCity and 24b_Arena.

## Not done in G1

- Reflection-probe seeds and the probe lattice: G4.
- The probe validity order is fixed but not yet applied (empty contents, clearance, outside world
  models).
- The DAT digest is not yet stored anywhere: G2 keys the `.ob2gi` file with it.
- No rendered frame, save or simulation behaviour changes.
