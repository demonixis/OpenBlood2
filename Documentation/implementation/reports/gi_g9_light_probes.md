# GI lot G9: light probes

2026-11-07, lot G9 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#light-probes-lot-g9). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal. Baked data: the 36 single-player worlds rebaked
at Final from `tools/gi/bake_all.py` (algorithm 3), in the ignored `data/gi/baked`.

## What landed

- **`engine/gi_probes.{h,cpp}`**: binary16 conversion, the SH L2 basis, Fibonacci directions with a
  per-probe random rotation, the probe capture (`gi_capture_probes`, jobs of 16 probes, cancel),
  the PROB section codec, `GiProbeField` (validity-weighted trilinear over the PLAC lattice),
  `gi_probe_model_light` (the ambient/pseudo-light split) and the probe world source.
- **Bake** (`gi_bake.cpp`): after the last lightmap pass, `gi_light_state` (the light those passes
  read, without the ambient fill) and the probe pass; PROB is written after `VTX `;
  `GI_PROGRESS stage=probes`, the probe fields of `GI_RESULT`; `gi_algorithm_version` 3.
- **Runtime** (`gi_runtime.cpp`): PROB decoded and checked against PLAC at lookup (whole file or
  nothing); `GiComposition::source` Texel gather / Light probes; the probe source prepared once per
  applied file.
- **`ModelLighting`**: the per-instance sample (cached per object slot and generation within the
  cell and 16 units), the probe ambient replacing AmbientLight and the pseudo-light ranked in the
  top four (or its mean c/4 for flat models and outside the top four); counters in
  `ModelLightingStats`.
- **`ScenePresenter`**: `GiProbeSettings` (switch, intensity p), the probe gate (file, switch,
  0 < p ≤ 4, not Original or Direct), the link to F8.
- **F8**: Runtime probe counts, switch, p slider, model counters and World source; Light probes
  panel with Show baked SH samples, the camera cell, its weights, the blended SH and the split.
- **Command line**: `--gi-source texels|probes` and `--gi-probes P` with `--gi`; `--bench` adds a
  "light probes (GI lot G9)" series and a summary line.
- **Docs**: the GI and enhanced renderer contracts, AGENTS.md (G1–G9, the G9 sentence, the
  enhanced renderer line), the plan, the roadmap and the provenance notes.

## Results

| Check | Result |
| --- | --- |
| Headless and private suite, RelWithDebInfo (`dev`, retail data), `gi_probes.*` included | 1,642 / 1,642 |
| GPU suite (`dev`, real display, Apple M5 Metal), `gpu.gi` included | 149 / 150: `gpu.display_options` failed because the window system did not apply the requested fullscreen state (the window path, untouched by G9) |
| Inspector-free Release, headless suite (built from the working tree with G10) | 1,188 / 1,188 |
| `gi_bake.retail` (SubwayA) | four sections, algorithm 3, 1,031 probes, all valid |
| `gi_runtime.retail` (SubwayA) | zero probes over the real lattice: identical meshes with the probe source, six lookups |
| `world_lighting.retail` | zero probes leave every model light identical; uniform probes raise the ambients and are reused |
| `gpu.gi` | zero probes byte-identical (models and probe source); nonzero probes change the frame, the switch, p = 0 and the Original view restore it |

## Rebake

The 36 worlds at Final take 194 s (10 jobs). Probes per world: 63 to 12,182; 60 invalid of
175,510; the probe pass totals 1.6 s across the 36 worlds. LMAP and
`VTX ` bytes are unchanged by the refactor (`gi_light_state` shares the lightmap passes' state);
PROB is identical for 1, 2 and 7 jobs.

## Cost

`--bench --bench-sweep --gi`, 1920×1080, 600 measured frames, `dev`, original renderer; two
alternating runs per row (means of the second run, the first warms caches).

| World | Probe series (ms) | Models CPU (ms) off → on | Frame mean off → on (ms) | GPU wait off → on (ms) |
| --- | --- | --- | --- | --- |
| 01_SubwayA (28 models) | 0.001 | 0.112 → 0.113 | 1.056 → 1.055 | 0.611 → 0.607 |
| 02_CityHub (232 models, 205 probe-lit) | 0.011 | 0.137 → 0.147 | 2.433 → 2.435 | 0.785 → 0.784 |

A sample and split cost about 0.4 µs (`gi_probes.cost`); with the cache, a moving model resamples
only when it leaves its cell or moves more than 16 units. CityHub's 27 models without a valid probe
stand where the placement put no light probe (outside reachable space); they keep their original
light. The probe world source takes about 20 ms on SubwayA, once per applied file.

## A/B for Gate C

Captures from `--bench --gi --gi-probes 0|1 --capture` at the start point (1280×720, original
renderer; not committed, they contain retail art):

- **01_SubwayA (tunnel, train ahead):** 2,915 pixels change (mean 14.85 → 15.00 of 255): the
  models in view (the figure behind the train's front window) take the bounced tunnel light from
  the probes instead of the face below them.
- **02_CityHub (station entrance):** 32,777 pixels change (mean 30.80 → 30.93): the civilian and
  the view weapon get a faint directional term from the lit wall; the authored AmbientLight is
  stronger than the probe ambient there, so f = 1 keeps it and the change stays small.

To play the A/B: Options > Display > GLOBAL ILLUMINATION ON, then F8 > Runtime > Light probes
(models) on and off, the p slider and the World source.

## Corrections during the lot

- The probe world source's corners now take the polygon's plane normal, like the bake's
  receivers, so the Light probes source and the texel gather read the same orientation.
- Two test fixtures wrote PLAC after LMAP; `save_gi_file` correctly refused the order (INFO, PLAC,
  LMAP, `VTX `, PROB, REFL).

## Open

- **Gate C**: the user plays the A/B and decides whether GI becomes a player option.
- The lattice stays single-level (contract, Lattice) until an A/B shows missing detail.
- G10 (F8 bake with hot reload) follows in its own commit.
