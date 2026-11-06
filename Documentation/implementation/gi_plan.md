# Global illumination plan (M10: baked GI, light probes and reflection probes)

Drafted 2026-11-06 at `55c640b6`. Nothing is implemented yet. The user answers
the [decisions](#decisions) before lot G1 starts.

## The request

- A new GI system that **overrides the original lightmaps** when it is active.
- It follows CyberEngine (`../CyberEngine`, `docs/probes-baking.md`): light
  probes and reflection probes.
- An ImGui overlay on **F8** provides:
  - automatic probe placement;
  - baking;
  - enabling and disabling GI;
  - CyberEngine's feature switches (box projection, blending, priority and so on).
- **One script bakes everything.**
- A **project folder** holds the bake data, and builds pick it up from there.
- Each lot states the Claude Code **reasoning effort** to use.

Constraints from earlier choices (the reverted PBR attempt):

- Never bake at launch, at world load or during a build.
- No noticeable per-frame cost.
- No long builds.
- With GI off, the original renderer stays byte-identical.
- Commercial data stays out of the repository unless the user decides otherwise (D3).

Size scale: S < 400 changed lines, M 400–1,200, L 1,200–2,500, XL > 2,500.

## Starting point

| Area | Current state | Consequence |
| --- | --- | --- |
| World lighting | DAT RGB565 lightmaps are decoded into 1024² RGBA8 pages bound at t1. The shader computes `albedo × 2 × min(lightmap × (1 − shadow) + dynamic, 1)`. Pages are rebuilt when the `presenter.lightmaps` key changes (DETAIL LOW). | GI can replace the pages in the same layout with no shader change and no per-frame cost. |
| Original preprocessor | Bakes direct light and shadows plus a flat AmbientLight fill, with no bounce. Stored texels are 1.0–1.4× the N·L term. | Keep the lightmap as the shadowed direct term; replace only the flat fill with baked bounce light. |
| Vertex-lit data | Credits, six GlobalDirLight multiplayer maps and Nightmares `soc_kickthehead2` are almost entirely vertex-lit. DETAIL LOW uses vertex light everywhere. | Bake per-corner values as well as texels. |
| World models | 2,729 world models carry their own lightmaps. Movers and breakables: 2,464 CDestructableBrush, 680 Door, 242 RotatingDoor, 136 RotatingBrush. | Bake at the authored pose like the original; GI moves with the model. |
| Model lighting | `ModelLighting` runs on the CPU per instance: the 4 strongest static lights with N·L ×2, plus AmbientLight + ModelAdd + the remainder + the object colour, cached. | Light probes replace the flat AmbientLight share on the CPU, with no shader change. |
| AmbientLight key | Read case-sensitively. 8 worlds spell it `ambientlight`, so 24_ancientcity and 24b_arena lose 15 10 5. | GI reads it case-insensitively. Fixing the original renderer is the separate optional lot GA. |
| Enhanced `pbr.frag` | All 4 fragment uniform slots are used (1376/1600/48/32 B). 12 samplers and 6 storage buffers are free. The environment specular is `diffuse × split`, and composite subtracts `reflectionLight` where SSR hits. | Reflection probes are the only shader change: `-DGI` variants with storage buffers and one texture array. SSR keeps replacing the environment term. |
| Shader artifacts | 72 artifacts (64 renderer/post-processing + 8 line), compiled by DXC and shadercross to SPIR-V and MSL 1.2. Each new `-D` switch adds 24. The generator rejects compute shaders, storage textures and read-write resources. | Prefiltering happens on the CPU at bake time; the GPU only samples. |
| GPU portability | Cube arrays need `imageCubeArray` on Vulkan. MSL 1.2 support is uncertain. SDL's Metal backend is suspected of a 6× cube-array over-allocation. The 256-layer guarantee allows 42 cubes. GpuRegistry uploads only 2D RGBA8 ≤ 1024². | Default to an octahedral RGBA16F 2D array; use cube arrays only after a measured gate. Extend GpuRegistry for float arrays with mips. |
| Probe validity | Root-BSP descent (`WorldVisibility::find_leaf`) classifies points exactly (−1 empty, −2 solid). World models are not carved into the root BSP. A 128-unit lattice has 266,720 empty cells over the 36 single-player worlds (median 3,449; 23_rooftop 75,492). | Validity needs the BSP, a collision clearance check and a world-model test, plus a per-world budget. |
| Tools | The inspector exists only with `BLOOD2_INSPECTOR`; ImGui is linked in every build. F8 is free but rebindable; F1/F2/F3/F10 are reserved. CLI exit codes are 0/2/3/4/5/6. `tools/icons/generate_icons.py` uses only the standard library. | F8 needs a controls v3 migration. The bake CLI follows the existing exit codes, and the script follows `generate_icons.py`. |
| Data folders | No runtime data folder exists. `SDL_GetBasePath` returns the executable directory, `Contents/Resources` on macOS, the bundle root on iOS and the APK assets on Android. | A new `gi/` folder ships beside the executable, in the bundle resources or in the APK assets. |
| Threads | The only engine thread is `demo_install.cpp`. SDL3 provides `SDL_CreateProcessWithProperties`, `SDL_KillProcess` and `SDL_WaitProcess`. | The F8 bake runs the bake CLI as a child process. Worker threads exist only inside the bake process. |
| Preferences | Version 12. Display has 15 rows; the next setting ID is 31. | GI rows 31 and 32, preferences version 13. |
| Contracts | The [enhanced renderer contract](../contracts/enhanced_renderer_contracts.md) says it has "no probes and no baking". The roadmap says the legacy set "takes no PBR inputs". | Lots G8 and G12 supersede these statements explicitly. |

## CyberEngine mapping

| CyberEngine | OpenBlood2 |
| --- | --- |
| SH L2 irradiance probes (9 RGB coefficients, band factors π, 2π/3, π/4) | Same math. Lattice per world, sampled per model instance on the CPU (G9). |
| Indirect diffuse replaces ambient | GI pages replace the flat ambient fill in the lightmaps (G8). Probes replace AmbientLight on models (G9). |
| CPU BVH bake, 1–2 bounces | CPU SAH BVH bake with 1–3 bounces, from the original direct light (G5, G7). |
| `.cybergi` / `.cybercube` v2 sidecars, SHA-256 fingerprints, atomic writes | One `.ob2gi` per world keyed by the DAT SHA-256, with per-section CRCs, a SHA-256 trailer and atomic writes (G2). |
| States Missing/Ready/Dirty/Baking/Failed | Same states, plus Stale(reason) and Disabled (G2, G3). |
| `BakeExecutor` (≤ 2 jthreads with stop_token) | A bake child process with worker threads and a stop flag; exit 12 means cancelled (G5, G10). |
| Reflection probes: RGBA16F cubes, CPU GGX prefilter, 2 probes + sky, priority, coverage, box projection | Same rules. Octahedral RGBA16F array by default (G11, G12). |
| Editor labels: Local reflections, Box projection, Blend distance, Priority, Resolution 16–256 (default 64), Bake/Load/Cancel, Without/With/Indirect only, Show baked SH samples | Same labels in the F8 overlay (G3, G4, G8, G9, G12). |
| `CyberGiBaker` CLI | `blood2 --bake-gi` plus `tools/gi/bake_all.py` (G5, G6). |

CyberEngine has no LICENSE file, so it serves as a reference by default (D13).
Its donor commit is `00fc3b28663883155ef277afcaf3e389d5dad07f`.

## Design decisions

1. **One offline bake produces three things per world:**
   - **GI lightmaps:** indirect irradiance for every DAT lightmap texel and polygon corner. This is the default world path.
   - **A light-probe lattice:** SH L2 irradiance. It lights models, and it can also light the world in an optional mode.
   - **Reflection probes:** box-projected radiance, prefiltered on the CPU, used by the enhanced renderer.
2. **What "override" means (D1).** When GI is effective, no surface samples the original pages.
   - `WorldMesh::build` substitutes GI pages composed in the exact DAT layout: same blocks, UVs, packer, borders, page count and VRAM.
   - Symbols: O is the original texel; A is the world ambient fill; A_eff = min(O, A); D = O − A_eff is the direct light with its shadows; E is the baked indirect irradiance in display units; k is the intensity (default 1); f is the ambient-floor weight (default 1).
   - Lightmapped faces: `rgb = clamp(D + max(k·E/2, f·A_eff), 0, 1)`.
   - Vertex-lit faces: `Dv + max(k·Ev, f·A_eff)`, with no /2 because the vertex path has no ×2.
   - Guarantees:
     - E = 0 with f = 1 reproduces O exactly.
     - With f = 1, GI never makes a surface darker than the original.
     - The ambient fill is never bounced twice.
   - Page alpha holds the direct fraction. The enhanced GI variant uses it for specular visibility and to apply model shadows to the direct part only.
   - Never modified: SURF_BRIGHT faces, sky faces, sprites, FX and PolyGrids.
3. **Light probes are the model GI**, sampled per receiver as in CyberEngine.
   - Validity-weighted trilinear sampling at the instance bounds centre, on the CPU.
   - `ModelLighting` ambient becomes `max(k_p·SH_ambient, f·AmbientLight)`. A dominant-direction pseudo-light from the L1 band competes in the existing top-4.
   - Cached per instance, and recomputed when the instance changes cell or moves more than 16 units.
   - The same lattice offers the world source **Light probes**: pages composed from the lattice evaluated per texel with the face normal. F8 can compare it with the default **Texel gather**.
4. **Reflection probes are the only shader work**, in `-DGI` variants of the enhanced `pbr.frag`.
   - Data: probe records and a cell grid in storage buffers; an octahedral RGBA16F 2D array with mips, where roughness = mip/(levels − 1).
   - Selection: at most 2 probes blended per pixel, plus a fallback to the sky or the current approximation. Priority, coverage and blend distance follow CyberEngine; box projection is computed per pixel.
   - The environment term replaces `diffuse` in `diffuse × split`, and `reflectionLight` carries it, so SSR still replaces it with no double count.
   - XR has no post-processing and uses the environment term inline.
5. **Bake engine.** CPU only, headless and deterministic.
   - Each ray's seed hashes the world digest, the element, the pass and the ray, so any thread count gives identical bytes.
   - A SAH BVH is built over the authored pose. Worker threads exist only in the bake process.
   - Radiance at a hit:
     - lightmapped face: `albedo·2·min(D + E/2, 1)`;
     - vertex-lit face: `albedo·(Dv + Ev)`;
     - SURF_BRIGHT face: albedo;
     - sky: traced through the sky world models (D9);
     - back face or void: zero, counted and invalidating.
6. **Never bake at launch, at world load or during a build.**
   - Loading validates the file and composes pages, with a target of 50 ms or less.
   - Builds copy files and nothing else.
   - A missing or stale file means original lighting plus an F8 status. A file is never partially applied.
7. **Content-keyed `.ob2gi` v1 file.**
   - Header: magic, version, world DAT SHA-256, edition, albedo key, settings hash, placement hash and algorithm version.
   - Sections, each with a CRC: INFO, LMAP (RGB9E5 plus a validity bitset), VTX, PROB (27 halves plus validity), REFL (mips).
   - A SHA-256 trailer, bounded limits and deterministic bytes (no timestamps or host names).
8. **Project folder `data/gi/`:**
   - `settings/presets.json`: committed bake presets and per-tier runtime defaults.
   - `placement/<world>.json`: committed, editable from F8, read with a strict bounded JSON reader.
   - `baked/<world>.<sha12>.ob2gi`: the payloads, handled per D3.
   - `bake_report.json`: written by the script.
   - Runtime search order (the first valid, current file wins):
     1. `--gi-dir` or `gi_dir=` in `blood2.local.ini`;
     2. `user_directory()/gi`, where F8 writes;
     3. `/sdcard/openblood2/gi` on Android and Quest;
     4. `SDL_GetBasePath()/gi`, the packaged copy.
9. **The F8 overlay uses CyberEngine's editor vocabulary.**
   - Panels: Status, Runtime, Light probes, Reflection probes and Bake.
   - States: Closed; Open, which pauses the game like the inspector; Pinned, which keeps markers visible during play.
   - Switches:
     - GI master, GI lightmaps, light probes (models), reflection probes;
     - world source: Texel gather or Light probes;
     - Box projection, globally and per probe; Blend 2 or Nearest; blend distance; priority;
     - intensities: k, f, probe, reflection, sky.
   - Views:
     - Final, Original, Direct only, Indirect only, Lighting only, Texel validity, Probe influence, Reflections only;
     - Show baked SH samples.
   - Gizmos: orange influence boxes, blue projection boxes, capture points, and probe spheres coloured by their SH, with invalid probes in magenta.
   - Editing: add at camera, move to camera, resize, delete, include and exclude boxes.
   - Bake: Preview or Final, progress with ETA, Cancel, and "Suspend scene rendering during bake".
10. **The F8 bake is a child process (D6).**
    - The overlay starts `blood2 --bake-gi …` with `SDL_CreateProcessWithProperties`, reads its progress lines, and cancels it with `SDL_KillProcess`.
    - It reloads the result at a frame boundary, but only if the world digest still matches.
    - The output equals the CLI's bytes by construction, and the game process gains no threads.
11. **Platform tiers.**
    - GI lightmaps and model probes are data-level, so they cost no GPU time and work in both renderers. This is a large gain for Quest, whose default renderer is ORIGINAL.
    - Reflection probes need ENHANCED. They are off by default on mobile and Quest; the opt-in uses 32², at most 16 probes, nearest probe only.
    - F8 and the baker exist only on desktop.

## Lots

Effort scale (the Claude Code reasoning effort to set before starting the lot):

- **low**: transcription or mechanical edits, with tests that catch mistakes.
- **medium**: established repository patterns, where failures are visible.
- **high**: a new subsystem built with known techniques, or an untrusted-input parser.
- **xhigh**: heuristics on real 1998 data, a wide regression surface, or concurrency.
- **max**: the lot fixes units or a GPU interface that later lots inherit, and its errors look plausible.

| # | Lot | Size | Effort | Depends on | Done when |
| --- | --- | --- | --- | --- | --- |
| G1 | Prerequisites: shared lightmap texel frame, point contents, world-model roles, ambient audit | M | **high** | decisions | Page digests are identical on every private world and `gpu.surface` is green. The contents query reproduces the seed survey. The user has reviewed the ambient audit table. |
| G2 | `.ob2gi` format, content keys, loader, `data/gi` search order, `--list-worlds`, `--gi-verify` | M | **high** | G1 | Limit, fuzz and sanitizer tests are green and every refusal is named. Writes are atomic. `gi_contracts.md` documents what refuses a file and what only marks it stale. |
| G3 | F8 overlay shell, F8 reservation (controls v3), Status panel, gizmo drawing | M | **medium** | decisions | F8 works in the normal, `--view-world` and demo flows. It is ignored during text entry and in XR, and the migration logs any dropped binding. Status is truthful, the inspector-free Release build is checked and no frame changes. |
| G4 | Automatic placement of light and reflection probes, placement JSON, F8 editing, `--gi-place` (Gate A) | L | **xhigh** | G1, G2, G3 | The all-world placement audit is in a report: counts, rejections by reason, time. SubwayA and CityHub have been reviewed in F8. The user fixes spacing, budget and reflection caps from the table. |
| G5 | Bake scene, deterministic CPU tracer, job pool, `--bake-gi` CLI | L | **high** | G2, G4 | Output bytes are identical for any thread count, and hits match `WorldShade`. Cancelling exits 12, writes nothing and passes the sanitizers. Throughput is reported (aim ≥ 1 M rays/s per core on the M5). |
| G6 | `tools/gi/bake_all.py`, `data/gi` folder, build injection for every platform | M | **medium** | G5 | One command bakes every single-player world and a second run skips them all. Desktop builds and the macOS bundle carry `gi/`, and the APK and iOS bundle contain it (checked by building only). An empty folder leaves build time unchanged, and a CI guard enforces D3. |
| G7 | GI lightmap bake core: indirect light per texel and corner, bounces, sky, dilation, leak control | L | **max** | G5 | Analytic tests (furnace, box, two-plane, leak) pass and the E = 0 identity is exact. Every single-player world bakes at Preview, and the Final time of the largest world is measured. The user approves the k, f, bounce and sky defaults from dumps of 2–3 worlds. |
| G8 | Runtime world override, GLOBAL ILLUMINATION row (preferences v13), F8 Runtime panel and views (Gate C) | L | **xhigh** | G7 | Frames with GI off, missing or stale stay byte-identical in both renderers, and the zero bake is exact. SubwayA and CityHub render with GI on. `--bench --gi` shows no frame cost beyond noise and load cost is measured. The user plays an A/B comparison and accepts the look. |
| G9 | Light probes: SH bake, model lighting, Light probes world source | L | **xhigh** | G8 | Exact identity where no probe applies, and probe counts stay within budget on every world. Models pick up coloured bounce light in a user A/B. Sampling costs under 0.2 ms with 100 moving objects. Both world sources can be compared in F8. |
| G10 | In-game F8 bake as a child process, with hot reload | M | **high** | G8 | An F8 bake of SubwayA completes with frame times logged. Cancel, NEW_LEVEL and quit during a bake are clean. The output bytes equal the CLI's, and the swap happens at a frame boundary. |
| G11 | Reflection probe capture and CPU GGX prefilter (Gate D) | L | **high** | G9 | Reflections bake from the CLI, the script and F8, and the ported CyberEngine prefilter tests pass. Sizes, times and per-tier memory are recorded, and D12 is decided after measuring the Metal allocation. |
| G12 | Reflection probe runtime: the single shader lot, REFLECTION PROBES row, F8 switches | XL | **max** | G11 | The 72 existing artifacts are unchanged and the 24 GI artifacts are regenerated together. GPU tests pass on Metal (and Vulkan when available), and readbacks match a CPU mirror. The cost is measured (aim ≤ 0.3 ms at 1080p HIGH on the M5), and the user accepts the A/B. |
| G13 | Platform tiers, full bake evidence, AGENTS.md, acceptance | M | **medium** | G6, G10, G12 | Every preset builds: macOS, iOS with `CODE_SIGNING_ALLOWED=NO`, Android and the Quest APK. Cost, size and coverage tables are recorded. The user accepts M10 or lists follow-ups. |
| GA | Optional: case-insensitive AmbientLight in the original renderer | S | **low** | user approval | Only the lowercase-key worlds change, with evidence. |

**Effort summary:** max ×2 (G7, G12), xhigh ×3 (G4, G8, G9), high ×5
(G1, G2, G5, G10, G11), medium ×3 (G3, G6, G13), low ×1 (GA).

**Order:** G1 → G2 → G4 → G5 → G6 → G7 → G8 → G9 → G10 → G11 → G12 → G13.

- G3 can run alongside G2.
- G6 lands right after G5. At that point the files hold only placement, so the F8 → CLI → folder → build → runtime loop is proven before the physics lots.
- G10 can run alongside G9.

**Total:** about 18,000–25,000 changed lines, excluding baked data and
generated artifacts.

**Gates:**

| Gate | After | What is decided |
| --- | --- | --- |
| A | G4 | Placement quality and budgets. |
| B | G7 | Bake time, determinism and look of the raw bake. |
| C | G8 | Look in game, and whether GI becomes a player option. |
| D | G11 | Reflection memory and GPU layout. |
| E | G12 | Reflection cost, and the default per platform. |

### Lot details

**G1, prerequisites (high).**
- Extract one shared `LightmapFrame`, used by both `WorldMesh` and the baker. It maps texels to world positions by solving `[P;Q;N]·x = [o_p + 20·sx, o_q + 20·sy, d]` and keeps the stored-size origin choice.
- Add a point-contents query (empty, solid or void), plus the collision clearance check and the world-model test.
- Classify world models: static, sky, container, mover or destructible. When BSP names repeat, the first model in file order wins.
- Audit the ambient floor per world: A read case-insensitively, the measured minimum texel and corner, and whether lightmaps carry A at ×1 or ×0.5.
- Reuse the DAT SHA-256 that `save_fingerprint.cpp` already computes.
- No rendered frame changes.

**G2, file and keys (high).**
- The container from decision 7, with limits: 256 MiB, 4 M texels, 262,144 probes, 42 reflection probes, resolution ≤ 256 and a power of two.
- The albedo key hashes each referenced texture's path, size and providing archive.
- Statuses: Missing, Ready, Stale(reason), Dirty, Baking, Failed(reason), Disabled.
- `--list-worlds --json` uses a fixed order: single-player worlds in index order, then `worlds/multi/`, then `worlds_ao/`.
- `--gi-verify` checks a file.
- The runtime search order is decision 8.

**G3, F8 shell (medium).**
- Compiled under `BLOOD2_GI_TOOLS`: desktop presets including Release, never iOS, Android, Quest or the dedicated server.
- Shares UiRuntime's ImGui context with the HUD and the F10 inspector.
- F8 is handled in `MenuInput` and `DemoFlow` before the inspector gate, following the F10 pattern. It is added to `reserved_key`, and controls v3 drops and logs any existing F8 binding.
- Closed, Open and Pinned states. Gizmos use the existing line renderer, so no new shader artifact is needed.

**G4, placement (xhigh).**
- Light probes:
  - a 128-unit lattice, adaptive within a per-world budget;
  - validity from contents, clearance and world models;
  - relocation inside the cell, and include/exclude boxes.
- Reflection probes:
  - seeds from large empty BSP leaves and rooms (floor-to-ceiling extent);
  - an influence box and a projection box each;
  - merged by overlap, at most 42 per world.
- `--gi-place` writes `data/gi/placement/<world>.json`. F8 shows markers and the edit list, and editing sets the Dirty state.

**G5, tracer (high).**
- BakeScene:
  - the faces `WorldMesh` renders, at the authored pose;
  - CPU DTX albedo from the mip closest to the 20-unit grid;
  - alpha and chroma cutouts, with translucent pass-through up to 8 layers.
- A SAH BVH with one traversal stack per thread, a job pool on SDL threads, and per-ray seeds.
- CLI options: `--gi-quality`, `--gi-jobs`, `--gi-out`, `--gi-placement`, `--gi-if-stale`.
- Output: a `GI_RESULT status=baked|current|failed` line and `GI_PROGRESS` lines.
- Exit codes: 0, 2 (arguments/data), 4, 5 (world load), 12 (cancelled).

**G6, script and packaging (medium).**
- `tools/gi/bake_all.py` uses only the standard library and runs under `python3 -I`.
- Options: `--blood2`, `--data-dir`, `--edition retail|nightmares|demo`, `--set single|multi|all` or `--worlds`, `--quality preview|final|custom`, `--jobs`/`--threads`, `--force`, `--prune`, `--list`, `--dry-run`.
- It prints one `GI_BAKE <world> PASS|SKIP|FAIL|CANCELLED` line per world and a total line, and writes `bake_report.json`. A failed world never stops the run, and Ctrl-C is forwarded to the bake.
- The binary owns all format and staleness logic; the script never parses `.ob2gi`.
- CMake:
  - `BLOOD2_GI_DATA_DIR`, default `data/gi/baked`;
  - `BLOOD2_PACKAGE_GI`: a POST_BUILD `copy_directory_if_different`, `MACOSX_PACKAGE_LOCATION` `Resources/gi`, the iOS bundle, Gradle assets through `-Pblood2.*`, and `PackageCi` only as D3 allows.

**G7, GI lightmap core (max).**
- Samples:
  - texel centres clamped inside the polygon and offset 0.5 units along the normal;
  - corners pulled toward the centroid;
  - invalid when starting in solid geometry or when too many rays hit back faces.
- Gathering:
  - stratified cosine rays, with B passes separated by barriers;
  - sky radiance per D9, emitters per D10.
- Dilation fills invalid samples inside each polygon block first, then across shared edges.
- The contract fixes the units: pages are ×2 Saturate, vertex light is not.
- Writes the LMAP and VTX sections.

**G8, world override (xhigh).**
- A compose hook in `WorldMesh::build` covers lightmapped pages, vertex corners and DETAIL LOW corners.
- The rebuild key gains the GI state, k, f and the view.
- Options > Display row 31: GLOBAL ILLUMINATION [OFF/ON], with preferences v13.
- The F8 Runtime panel offers intensities and views, all produced by recomposing pages.
- Network clients load GI from their own content; the dedicated server never does.
- Supersedes "no probes and no baking" in the enhanced renderer contract and AGENTS.md.
- Known interim effect: until G12, enhanced grid specular reads the total GI luminance. This is documented.

**G9, light probes (xhigh).**
- Capture: Fibonacci-sphere rays with a seeded rotation per probe, against the GI-composed world and the sky.
- Projection: SH L2 with the cosine convolution and CyberEngine's constants. Probes whose rays mostly hit back faces are invalid.
- Model lighting: the `ModelLighting` replacement and the dominant-direction pseudo-light from decision 3.
- F8: Show baked SH samples, the nearest probes and their weights at the camera, an SH readout, and the Light probes world source.

**G10, in-game bake (high).**
- Saves the edited placement, then starts the child bake.
- Progress and ETA come from `GI_PROGRESS`. Cancel kills the child, which exits 12.
- Output goes to `user_directory()/gi`, with a "Copy to project data/gi" button in desktop builds.
- Optional suspension of scene rendering during the bake.
- Hot reload follows decision 10.

**G11, reflection bake (high).**
- Six faces at 64² (16–256) with 2×2 supersampling, captured from the GI-composed world and the sky.
- CPU GGX prefilter: roughness = level/(levels − 1), with 64/32 samples.
- Octahedral encoding with borders, RGBA16F (R11G11B10 optional), and optional miniz compression of the REFL section.
- Measures the Metal allocation needed for D12.

**G12, reflection runtime (max).**
- `-DGI` variants of `pbr.frag`, for enhanced pipelines only.
- GPU data:
  - storage buffers for probe records and the cell grid;
  - a texture array and its sampler;
  - GpuRegistry support for RGBA16F arrays with mips.
- Per pixel:
  - selection of 2 probes plus the fallback, by priority, coverage and blend distance;
  - box projection by slab intersection (CyberEngine `reflection_environment.hlsli`).
- Kept intact: SSR replacement, the inline environment term in XR, and the full-size Metal pushes.
- Specular visibility comes from the page alpha, and model shadows apply to the direct part only.
- Options > Display row 32: REFLECTION PROBES [OFF/LOW/HIGH].
- F8 switches and the Reflections only view.

**G13, acceptance (medium).**
- Builds every preset, and runs the desktop-forced mobile tier test.
- A full single-player bake with time, size, coverage and cost tables.
- The AGENTS.md paragraph and the contracts.
- No simulator, emulator or device run without the user's consent.

## Verification policy

- **Every lot:**
  - the headless suite is green;
  - `gpu.surface` and `gpu.post` are green, proving ORIGINAL and ENHANCED with GI off byte-identical;
  - sanitizers run on parsers, threads and lifetime code;
  - the inspector-free Release build is checked whenever the app or UI changes.
- **Reporting:** private-data results (retail worlds) are reported separately from headless results. GPU tests run on the M5 under Metal, with the device name logged, and on NVIDIA Vulkan when available.
- **Determinism:** identical bytes across runs and thread counts, checked for every bake section.
- **Mobile and Quest:** build only. Simulators, emulators and devices require the user's consent every time.
- **Bookkeeping per lot:**
  - update `Documentation/contracts/gi_contracts.md` (created in G2);
  - write a report in `Documentation/implementation/reports/`;
  - add provenance notes for anything taken from CyberEngine;
  - one commit on main, with no AI attribution.

## Risks

| Risk | Mitigation |
| --- | --- |
| Wrong direct term where the ambient fill is not uniform or the key is lowercase | G1 audits every world; E = 0 with f = 1 is exact; F8 shows Direct only. |
| Light leaks through thin BSP walls, door gaps and world-model seams | Validity from contents and the back-face ratio; dilation; probe relocation; leak tests; F8 Texel validity view. |
| Bake time is unknown (estimate: 3–15 min for the campaign at Final) | Measured in G5 and G7; SAH BVH; all cores; Preview preset; the script skips current bakes. |
| Commercial-derived payloads committed or published by accident | D3; a `.gitignore` rule plus a CI check; `BLOOD2_PACKAGE_GI` off in public CI. |
| Byte-identity regressions | A null override runs today's code; GI lives in separate `-DGI` variants; existing artifacts are compared byte for byte. |
| Shader portability: cube arrays in MSL, Metal over-allocation, uniform budget | Octahedral 2D array by default; storage buffers instead of new uniforms; Gate D measures before committing. |
| Unit mistakes make GI too bright or too dark | Units are fixed in the G7 contract with analytic tests; the vertex/lightmap/model scales are tested separately. |
| Static bake goes stale on doors, trains and destructibles | The same behaviour as the original lightmaps, documented; dynamic lights still apply on top. |
| Reserving F8 removes a player binding | The controls v3 migration logs what it drops; same pattern as F10. |
| In-game bake contention and lifetime | A child process, cancelled by killing it, with reload only if the digest still matches. |
| The user pictures probes lighting every surface | D1 is explicit, and the Light probes world source lets the user compare both in game. |

## Decisions

| # | Question | Options | Recommendation |
| --- | --- | --- | --- |
| D1 | What does "GI overrides the lightmaps" mean on world surfaces? | (a) GI pages in the DAT layout: original direct light and shadows plus baked bounce, zero per-frame cost; (b) per-pixel probe sampling in the shaders, as in CyberEngine (per-pixel cost including XR, leaks through thin walls); (c) also re-bake the direct light from Light/DirLight objects. | **(a)**, with the Light probes world source for comparison; (c) as a later option. |
| D2 | Which renderer gets GI? | (a) GI pages and model probes in both renderers, reflections in ENHANCED only; (b) everything in ENHANCED only. | **(a)**: free on Quest. GI off keeps ORIGINAL byte-identical. |
| D3 | May baked payloads (derived from retail lightmaps and textures, about 100–150 MB for the campaign at Final) be committed or shipped? | (a) placement and settings committed, `baked/` gitignored and injected only into local builds; (b) commit the payloads; (c) Git LFS or a private repository. | **(a)**. The folder and its injection exist either way. |
| D4 | Which builds contain F8 and the baker? | (a) every desktop preset including Release, behind `BLOOD2_GI_TOOLS`; (b) debug/dev only, with the inspector. | **(a)** |
| D5 | How is F8 claimed? | (a) reserved like F10, with controls v3 dropping existing F8 bindings; (b) a rebindable "GI tools" action, default F8. | **(a)** |
| D6 | How does F8 bake? | (a) a child process running the same binary; (b) worker threads inside the game. | **(a)**: CLI-identical output, no threads in the game. |
| D7 | Which worlds are covered? | (a) the 36 single-player worlds accepted, multiplayer, Nightmares and demo best-effort through `--set`/`--edition`; (b) all 58 worlds plus Nightmares accepted. | **(a)** |
| D8 | How do moving and destructible world models enter the bake? | (a) everything at the authored pose, as the original preprocessor did, with per-object exclusion in the placement JSON; (b) movers excluded as occluders by class. | **(a)** |
| D9 | Does the sky emit light? | (a) trace the sky world models from the mapped sky eye, falling back to the average sky colour, with a sky scale (1.0) in F8; (b) a constant colour; (c) no sky light. | **(a)** |
| D10 | Which surfaces emit in the bake? | (a) lit surfaces re-emit their light, SURF_BRIGHT faces emit their albedo, manifest emission is not baked; (b) also bake manifest emissive classes. | **(a)** |
| D11 | Display rows and defaults? | (a) GLOBAL ILLUMINATION [OFF/ON] and REFLECTION PROBES [OFF/LOW/HIGH], off until acceptance, with defaults decided at G13; (b) on by default once data exists. | **(a)** |
| D12 | How are reflections stored on the GPU? | (a) octahedral RGBA16F 2D array; (b) cube array, with the SDL Metal patch if the over-allocation is confirmed. | **(a)**, revisited at Gate D. |
| D13 | How is CyberEngine code reused? | (a) as a reference, reimplemented, with provenance notes; (b) import selected files (SH, prefilter), which needs a license statement since the repository has no LICENSE. | **(a)**; the author may approve (b). |
| D14 | Where does the work sit in the roadmap? | (a) a new M10, "Baked GI and probes", taken from the "Rendering extras" row under Later; (b) left under Later. | **(a)** |
| D15 | Fix the case-sensitive AmbientLight read? | (a) GI reads it case-insensitively and the original renderer is unchanged; (b) also fix the original renderer (lot GA). | **(a)**, with GA as a separate approval. |

## Defaults taken (the user may override)

| Setting | Default |
| --- | --- |
| Light-probe spacing | 128 units (about 2.8 m) |
| Light-probe budget | 16,384 probes per world |
| Reflection probes | 64² with 2×2 supersampling; at most 42 per world on desktop, 16 on mobile |
| Bake presets | Preview: 1 bounce, 64 rays. Final: 2 bounces, 256 rays. Custom: up to 3 bounces. |
| Runtime intensities | k = 1, f = 1, probe 1, reflection 1, sky 1 |
| Names | Folder `data/gi`, extension `.ob2gi`, script `tools/gi/bake_all.py`, contract `gi_contracts.md` |
| Effort levels | As listed in the lot table |
