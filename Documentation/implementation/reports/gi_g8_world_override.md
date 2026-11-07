# GI lot G8: world override

2026-11-07, lot G8 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#world-override-lot-g8). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal. Baked data: the 36 single-player worlds at
Final from `tools/gi/bake_all.py` (algorithm 2), in the ignored `data/gi/baked`.

## What landed

- **`engine/gi_runtime.{h,cpp}`**: `load_gi_world_light` (content keys, `find_gi_file` over the G2
  roots, LMAP and `VTX ` decoded and checked, whole file or nothing), `GiWorldLight` (record
  lookup and the per-view composition), `GiComposition` (k, f, view), the runtime states and the
  status log line with the load cost.
- **`WorldMesh::build`** takes an optional `GiWorldLight`: listed blocks compose their page texels
  after RGB565 expansion (borders replicate the result), listed corner sets compose the vertex
  colours (vertex-lit, and lightmapped polygons at DETAIL LOW), and Lighting only draws the
  decoded world textures white. Without it the code path is unchanged.
- **`ScenePresenter`**: GI state, roots, composition and reload; the lookup runs when the world
  changes, GI turns on or F8 asks; the rebuild key adds the applied file, k, f and the view.
- **Options > Display** `GLOBAL ILLUMINATION [OFF/ON]` (setting 31, the 16th row, off by default),
  **preferences version 13**.
- **F8 Runtime panel**: the switch, the state, bake settings, file and composed counts, load and
  mesh cost, k/f sliders with Defaults, the six views and Look up again; the Status summary and
  the Pinned strip follow the runtime state.
- **Frontends**: the game (desktop and XR) follows the Display row; the network client loads GI
  from its own content and roots; the dedicated server never does. `--bench --gi` (and smoke
  runs) with `--gi-k`, `--gi-f` and `--gi-view`, logging the load cost.
- **Docs**: the GI, enhanced renderer and menu contracts, AGENTS.md (G8 paragraph, "no probes and
  no baking" superseded, preferences version 13), the plan and the roadmap.

## Results

| Check | Result |
| --- | --- |
| Headless and private suite, RelWithDebInfo (`dev`, retail data) | 1,634 / 1,634 |
| GPU suite (`dev`, real display, Apple M5 Metal), `gpu.gi` included | 150 / 150 |
| Inspector-free Release, headless suite | 1,180 / 1,180 |
| `gpu.gi`: CityHub and SubwayA, original and enhanced, high and low detail | GI off, no file, stale file and zero bake byte-identical (8 / 8); nonzero bake changes the frame; Original view and GI off restore it |
| `gi_runtime.retail` (SubwayA) | zero bake byte-identical meshes at high (3,672 blocks, 145,946 texels, 31 sets) and low detail (3,703 sets, 14,847 corners) |

## Cost

`--bench --bench-sweep` (360° yaw sweep), 1920×1080, 600 measured frames, `dev`
(RelWithDebInfo), no post-processing; three alternating runs per row, means of the three.

| World, renderer | Frame mean off → on (ms) | Frame p95 off → on (ms) | GPU wait off → on (ms) |
| --- | --- | --- | --- |
| 01_SubwayA, original | 1.097 → 1.082 | 1.368 → 1.291 | 0.629 → 0.626 |
| 01_SubwayA, enhanced standard | 4.366 → 4.349 | 4.779 → 4.731 | 3.759 → 3.766 |
| 02_CityHub, original | 2.529 → 2.490 | 3.735 → 3.089 | 0.807 → 0.795 |
| 02_CityHub, enhanced standard | 5.204 → 5.194 | 5.909 → 5.910 | 3.355 → 3.354 |

The frame cost does not change: GI only changes page texels and vertex colours, so the same
draws, textures and buffers are presented (differences are within run-to-run noise).

| Load (ms) | 01_SubwayA off → on | 02_CityHub off → on |
| --- | --- | --- |
| First update (original / enhanced) | 7.2 → 29.4 / 8.4 → 30.2 | 22.0 → 75.1 / 25.7 → 78.6 |
| World mesh build | 3.4 → 5.7 | 12.3 → 17.9 |
| Content keys (DAT + albedo digests) | 6.6 | 14.6 |
| File lookup (read, CRCs, SHA-256 trailer) | 13.0 | 32.3 |
| Section decode and checks | 0.1 | 0.4 |
| File | 3,672 blocks, 145,946 texels, 3,703 sets, 14,847 corners | 9,006 blocks, 344,666 texels, 9,829 sets, 39,152 corners |
| Composed at high detail | 3,672 blocks, 31 sets | 9,006 blocks, 823 sets |

The load cost is paid once per world (and on each F8 change of k, f or view, which rebuilds only
the world mesh: 6–18 ms). It is dominated by hashing: the DAT and albedo digests and the file's
SHA-256 trailer. Caching the content keys per world (they are also computed by F8 Status) is a
possible later optimisation; nothing in G8 needs it.

## A/B for Gate C

Captures from `--bench --gi --gi-view original|final|indirect --capture` at the start point
(1280×720, original renderer; not committed, they contain retail art):

- **01_SubwayA (tunnel):** with GI the rails, sleepers and the train's front sign become visible
  where the original lightmaps leave them black; the walls keep their look.
- **02_CityHub (station entrance):** almost unchanged at the start point, where the authored
  ambient fill already lights the scene about as much as the bounce does (f = 1 keeps the fill
  wherever it is stronger).

To play the A/B: Options > Display > GLOBAL ILLUMINATION ON, then F8 > Runtime to switch between
the Final and Original views or change k and f live. The `dev` and `release` builds package the 36
local bakes beside the binary (`BLOOD2_PACKAGE_GI`, the "packaged" root), so a plain launch finds
them; `gi_dir`/`--gi-dir` and the user directory come first.

## Interim effects (documented)

- Models sample the face light below them from the world mesh, so they see the composed light
  until the light probes (G9).
- The enhanced renderer's static-light specular occlusion and normal-map relief read the total
  GI-composed luminance until the reflection probes (G12).
- Model shadows remove 60 % of the composed light, GI included.

## Open

- **Gate C**: the user plays the A/B and decides whether GI becomes a player option (the row is
  off by default until then, decision D11).
- G9 (light probes) and G10 (F8 bake with hot reload) are next.
