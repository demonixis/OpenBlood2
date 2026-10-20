# M2 — Reduce CPU skinning and model preparation work

Date: 2026-10-17. Baseline: `2c82c1fc2c744bced9847dc66b9c4b7b51f1bd93`.
Implementation: `444035f86168a586c91c4b0ff5c6a77448a8b457`. **M2 remains incomplete.**

After the collision and container-query optimizations, the remaining CityHub
model stage still expanded roughly 104,000 vertices per frame on the CPU.
Separate instrumented Debug timers measured 83,520 skinning calls across 360
CityHub ticks: about 1.22 seconds inside vertex sampling versus 4.91 seconds
inside model preparation. These are nested instrumented totals, not additive
frame times. Skeleton composition accounted for only about 0.19 seconds.
Reusing fully prepared geometry therefore addresses more work than moving only
the bone calculations.

Immutable shared ABC assets now reuse their last exactly matching local-space
triangle stream. Static or synchronized instances avoid repeated skinning,
packed deformation, scale/UV validation and face expansion. Changing poses
reuse their existing cache storage. A separate borrowed-model path always
rebuilds, allowing mutable synthetic fixtures and an uncached comparison.

The sampler itself resolves the key interval once and uses bounded local node
scratch instead of allocating a temporary pose vector each call. Quaternion
interpolation, hierarchy composition, repeated packed-vertex write order and
normalized ABC coordinates are unchanged. No original game file, animation
clock, model-key event, collision cadence or AI rule was changed.

The cache is limited to 256 weakly owned assets and one million retained vertex
slots (32 MB plus metadata). It does not keep previous-level models or objects
alive. Expired owners are pruned; ownership identity also rejects address reuse
within a frame. The existing world-revision clear resets CPU/GPU presentation
at NEW_LEVEL. Cache exhaustion uses ordinary uncached preparation. Hidden,
missing, masked and invalid entries retain their original diagnostics and gates.

The inspector now shows sampled/reused/uncached preparation, reused vertices,
retained vertex bytes and per-entry reuse. These are real counters, not FPS.
World transforms, skin materials, transparency and close-view selection are
fresh per instance. Expanded vertices still upload through SDL_GPU each frame;
**skinning remains CPU-side**. Vulkan/SPIR-V and Metal/MSL shaders are unchanged.

See [contracts](../../contracts/model_preparation_contracts.md),
[source ledger](../sources/m2_model_preparation_sources.json) and
[retained evidence](evidence/m2-model-preparation.json).

## Validation

The new synthetic tests compare cached and uncached vertices exactly across
sample times, scaling, node masks, current transforms and alpha. They check
invalid poses/transforms, hidden and required-skin failures, weak ownership,
reused addresses, clear, asset/vertex budgets and fail-closed frame overflow.
The real CityHub test compares valid/missing skin changes and checks asset
expiration while the inspector still exists. Existing removal/reload, node-hide,
player restoration, attachment, packed deformation and model-key tests remain.

A separate control compiles the baseline model sampler, view implementation and
scene assembly against the same current unchanged game/resources libraries.
For 360 fixed ticks in SubwayA followed by a real NEW_LEVEL to CityHub and 360
more ticks, the control and new implementation produce identical digests of
expanded model vertices, colors, UVs, ranges, material references, draws,
visibility/errors and checked object transforms/lifetimes/container state.
This is a finite diagnostic equivalence check, not proof of every game state.

**52 headless tests pass in each of Debug, inspector-free Release and Clang
ASan/UBSan**, including ten private-data cases. **Eight GPU scenarios pass in
each graphical configuration.** Full commands and logs are retained in the
evidence manifest. GPU checks select
NVIDIA using `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json` and
confirm NVIDIA GeForce RTX 3060 Laptop GPU in the logs. Pixel assertions cover
cached as well as freshly prepared node-hide geometry, world occlusion, close
weapons, alpha, resize and unchanged texture revisions. Private checks render
SubwayA, CityHub models, destructible models and repeated destination transfers.
The destination capture was inspected; game assets and images remain outside Git.
Native Metal was not retested during this optimization.

## Isolated measurements

Median of three runs, in milliseconds:

| Build / world | Model mean, before → after | Model p95, before → after | Model maximum, before → after |
| --- | ---: | ---: | ---: |
| Debug / 01_subwaya | 5.032 → 4.609 | 5.304 → 4.869 | 7.762 → 6.567 |
| Debug / 02_cityhub | 13.631 → 8.274 | 15.037 → 8.811 | 21.218 → 11.579 |
| Release / 01_subwaya | 0.469 → 0.507 | 0.521 → 0.589 | 0.692 → 0.787 |
| Release / 02_cityhub | 1.351 → 0.985 | 1.530 → 1.108 | 2.080 → 1.336 |

On the local Intel Core i5-10500H / NVIDIA RTX 3060 Laptop machine, the
same retained frame diagnostic runs three before/after pairs per build,
without concurrent builds or tests. It measures 360 fixed 1/60-second ticks in
each world, discarding the first sixty for steady statistics. It includes real
simulation, offline PCM mixing and world/model/sprite preparation. It excludes
GPU upload/draw, ImGui, real audio-device scheduling and synchronous world
loading. Debug diagnostic wrappers/scene units use -O2 with actual -O0 engine/game
libraries; Release uses the project's -O3 libraries. Both Debug timing versions
use the same retained session timing instrumentation. Baseline timing executables
were preserved before rebuilding; the reproducible control recipe is also kept. The initial
instrumentation patch/recipe targets that baseline commit, not the changed
ModelView layout. Diagnostic sources use the explicit temporary filenames in
the retained scripts; they are not production dependencies.

CityHub model preparation falls about **39% in Debug and 27% in Release**.
At the final measured CityHub frame, 206 of 232 visible preparations reuse
geometry, including 47,994 of 104,202 expanded vertices; 52 cached assets retain
822,816 vertex bytes. SubwayA reuses only 12 of 28 preparations (4,512 vertices).
Its Release mean rises by 0.039 ms (about 8%): cache bookkeeping/copying and the
sampler's extra safe bounds checks are not a universal win for animated scenes.
This tradeoff is retained for the measured destination-level improvement.

Simulation medians are effectively unchanged: CityHub Debug 14.791 → 14.781 ms,
Release 3.196 → 3.216 ms. Existing simulation spikes remain. Model-stage peak
improvements are finite measurements, not an end-to-end FPS or stutter guarantee.
All 10,923 inventoried reference-file hashes and six reference repository states
remain unchanged. No commercial asset was added to either repository.

## Remaining work

One cached variant per asset can miss for differently scaled or unsynchronized
instances. CPU face copying/GPU upload, simulation work and synchronous resource
loading still contribute to frame cost. This change does not certify that all
playtest stutter is solved, nor require a new GPU shader/skinning architecture.
Idle Cultists, encounter durability/possible firing failure, missing CityHub
water presentation and required M2 services remain follow-ups. M2's observable
exit criteria are unchanged.
