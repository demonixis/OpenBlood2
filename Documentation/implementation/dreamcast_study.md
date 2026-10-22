# Dreamcast port study

Exploratory study, 2026-10-22, at `924e7761`. No code was written. It is not a
roadmap milestone: reassess once the campaign (M3) and its contracts are complete.
It answers two questions asked by the user after a Dreamcast scene developer
offered help with the KallistiOS/SDL3 infrastructure:

1. Is a Dreamcast version realistic from this code base?
2. Should it live in a dedicated fork, or behind a rendering abstraction with a
   Dreamcast renderer next to SDL_GPU (which could later admit OpenGL)?

Web facts were checked on 2026-10-22; [sources](#sources) are listed at the end.
Claims marked *unconfirmed* come from press or fan material that could not be
verified.

## Conclusion

- **Feasible, but as a project of its own.** The renderer is not the hardest part:
  memory is. The current build needs 47 MB (SubwayA) to 76 MB (CityHub) of RAM for
  the simulation alone, before any GPU, presentation mesh or audio buffer. The
  Dreamcast has 16 MB in total, code included. The original game recommended
  64 MB: the same fourfold gap Secret Level faced when it squeezed Unreal
  Tournament onto the Dreamcast, at the cost of maps.
- **The toolchain is no longer an obstacle.** KallistiOS ships GCC 15.2 with full
  C++20, libstdc++ with threads, `std::filesystem` and exceptions. GPF's SDL3
  fork covers input, audio, threads and timers, but not SDL_GPU.
- **No fork.** Keep one repository with a `dreamcast` preset that builds the game
  only, like iOS, Android and Quest. The gameplay code (`src/game`, ~69k lines) is
  most of the value and changes every week; a fork would have to replay every fix.
- **A high-level scene backend, not a low-level RHI.** The PowerVR2 has no shaders
  and no hardware transform: an RHI wide enough to cover it would be a
  fixed-function lowest common denominator. Split the presenter at the boundary
  the code already has (API-free snapshots, then SDL_GPU recording) and add a
  native PVR backend beside the SDL_GPU one. LithTech did the same in 1998 (Blood
  II ships `d3d.ren` and `soft.ren` renderer modules), and so does dca3, the
  closest modern precedent, with a Dreamcast backend for its renderer library.
- **OpenGL is a separate decision.** OpenGL is shader-based and belongs beside
  SDL_GPU, not beside the PVR. The Dreamcast neither requires it nor benefits from
  it, except GLdc as an optional first-light shortcut.

## The target

| Resource | Dreamcast | Consequence |
| --- | --- | --- |
| CPU | Hitachi SH-4, 200 MHz, 8 KB instruction / 16 KB data cache (8 KB can become scratchpad), two 32-byte store queues. Single-precision FPU with vector ops (`FIPR`, `FTRV`, `FSCA`, `FSRRA`). | Every world, model and particle vertex is transformed and clipped on the CPU. |
| Doubles | KallistiOS defaults to `-m4-single`: doubles are real but switch the FPU mode; double add/multiply has 7–9 cycles of latency and blocks the FPU for about 5 (single: 3–4, one per cycle). `-m4-single-only` silently makes doubles floats. | Code that uses `double` on purpose stays correct but slow. |
| Main RAM | 16 MB, shared by code, heap and I/O buffers | The binding constraint (see measurements). |
| Video RAM | 8 MB: framebuffers, tile accelerator buffers and textures. dca3 reports about 2–2.5 MB left for textures. | Textures VQ-compressed or 16-bit, never RGBA8. |
| GPU | PowerVR2 CLX2: tile-based deferred (32×32 tiles), fixed function, opaque / punch-through / translucent lists plus opaque and translucent modifier volumes, automatic per-pixel translucency sorting, table and vertex fog, free VQ decoding, bump mapping | No programmable shading; the original renderer maps well, the enhanced renderer does not. |
| Textures | ARGB1555, RGB565, ARGB4444, YUV422, bump, 4/8 bpp paletted (1024 palette entries in total); 8 to 1024 texels per side; twiddled or strided; VQ only pre-encoded | An offline converter is required. |
| Audio | AICA: ARM7 with 2 MB sound RAM, 64 channels of 8/16-bit PCM or 4-bit ADPCM, loops and envelopes | Sounds must be converted and staged; the AICA is itself a sampler. |
| Storage | GD-ROM (~1 GB) is not writable by homebrew; CD-R via MIL-CD (~700 MB); optical drive emulators (~900 MB images for dca3); SD over the serial port; IDE mods | Slow seeks: data packed per level. |
| Saves | VMU: 128 KB of flash, 200 usable blocks of 512 bytes (100 KB) | A 194 KB save does not fit uncompressed. |
| Input | Maple controller with one analog stick and analog triggers; Maple keyboard and mouse; rumble pack | New default layout; keyboard/mouse optional. |
| Display | 640×480, 50/60 Hz | Same resolution as the original menus. |
| Development | dcload-ip over the Broadband/LAN adapter (100 Mbit, host file I/O); Flycast 2.7 | Emulators do not model cache and memory timing: profile on hardware. |

## Toolchain and SDL3

| Component | State | Consequence |
| --- | --- | --- |
| Compiler | `utils/kos-chain` stable profile: GCC 15.2.0, binutils 2.45.1, newlib 4.6; GCC 13–16 testing profiles, 17-dev with SuperH improvements; a Clang environment exists. KallistiOS 2.2.2 (2026-04), active. | C++20 builds as is; the README claims full C++20 and preliminary C++23. |
| libstdc++ | `kos` thread model; examples cover `std::thread`/`jthread`, mutexes, atomics, coroutines, `std::filesystem` and exceptions; no `-fno-exceptions`/`-fno-rtti` by default; chrono time zones off. | No filesystem shim needed. |
| Atomics | `-matomic-model=soft-gusa`, emulated on the single core; 64-bit atomics are not lock-free. | The one atomic counter in `server_messages` is fine. |
| SDL3 | GPF's fork, branch `dreamcastSDL3` (not upstream; kos-ports still has SDL 1.2). Video through GLdc by default or software framebuffers; audio on the AICA (`snd_stream`, ADPCM); Maple controllers, rumble, keyboard and mouse; pthreads; timers; filesystem. **SDL_GPU is off.** | SDL3 can keep input, audio, timers and the frame loop; the video path is ours. |
| GLdc | Partial OpenGL 1.2 with paletted/VQ texture extensions and render-to-texture; advertises two texture units, but the automatic second pass is disabled in current master. | Usable for a first light; lightmaps still need an explicit second pass. |
| Math | sh4zam (Falco Girgis) wraps `FIPR`, `FTRV`, `FSCA`, `FSRRA`. | Candidate for the CPU transform and skinning. |

## What the code already gives

| Area | Current state | For a Dreamcast port |
| --- | --- | --- |
| Gameplay | `src/game` uses no SDL_GPU or ImGui; its only SDL calls are `SDL_GetPrefPath`/`SDL_free` in [save_slots.cpp](../../src/game/save_slots.cpp) and [menu_preferences.cpp](../../src/game/menu_preferences.cpp). | Compiles unchanged in principle; the work is memory and CPU, not API. |
| Presentation snapshots | `WorldMesh`/`WorldDraw` ([world_view.h](../../src/engine/world_view.h)), `ModelView`, `SpriteView`, `PolyGridView` and the 2D `HudSurface`/`HudCanvas` ([hud_surface.h](../../src/engine/hud_surface.h)) contain no SDL type. | The backend-neutral half of the boundary exists. |
| SDL_GPU | Confined to 38 files: engine renderers (`world_renderer`, `model_renderer`, `gpu_registry`, `world_shaders`, `capture`, `runtime`, `xr/`) and app presentation (`scene_presenter`, `hud`, `menu_renderer`, `frontend`, `bench`, `xr_*`). | These are the files a PVR backend replaces or bypasses. |
| Presenter | [ScenePresenter](../../src/app/scene_presenter.h) mixes neutral state (world mesh, draws, sky order, model/sprite/PolyGrid views, light grid) with SDL_GPU state (registry, pipelines, renderers, shadow atlas); `update()` is neutral, `record()` is SDL_GPU. | Split it into a snapshot and a backend. |
| Frame loop | Acquire, record, ImGui and submit are inline in `run_frontend` ([frontend.cpp](../../src/app/frontend.cpp)). | Needs a platform frame interface. |
| Menus/HUD | Original 2D surfaces composed on the CPU into RGBA canvases; HUD and modals are ImGui draw lists. | Draw as PVR sprites; a full-screen RGBA canvas costs 1.2 MB of RAM. |
| Skinning | GPU skinning with the CPU `sample_vertices` path kept for tests. | The CPU path is the Dreamcast path (with `FTRV`). |
| Dependencies | SDL3, ImGui, spdlog ([Dependencies.cmake](../../cmake/Dependencies.cmake)). `try`/`throw` in two files; threads only in logging, crash capture and the launcher (plus one atomic counter in `server_messages`). | Small surface; spdlog and the inspector can be compiled out. |
| Precision | `double` in 163 engine/game files (527 uses), including the collision solver (32 in [collision.cpp](../../src/engine/collision.cpp), whose contracts require exact SAT results) and the DLS sampler (35). Audio mixes in `SDL_AUDIO_F32`. | Correct under `-m4-single`, slow in hot paths (see CPU). |
| Visibility | The DAT's per-leaf visibility lists are bounded and skipped by [world.cpp](../../src/engine/world.cpp); the presenter only frustum-culls. | Real visibility must be added (see CPU). |
| Platform presets | iOS, Android and Quest already "build the game only, without launcher or tests" ([packaging](../packaging.md)). | A `dreamcast` preset follows the same pattern. |

### Measurements

macOS arm64, `dev` preset (RelWithDebInfo with inspector), retail data, measured
with `/usr/bin/time -l`. No GPU, presentation mesh or audio output is involved
in the world runs: they are the simulation side only.

| Measure | Value |
| --- | --- |
| Executable / `__text` | 9.1 MB / 4.86 MB |
| Process baseline (`--check-data`) | 2.5 MB peak footprint |
| 01_SubwayA `--start-game` | 47.5 MB peak footprint (56 MB RSS), 67 decoded images |
| 02_CityHub `--inspect-world` (objects only) | 45.9 MB peak footprint |
| 02_CityHub `--start-game` | 76.1 MB peak footprint (85 MB RSS), 276 decoded images |
| SubwayA world textures (earlier GPU run) | 53 textures, 6.16 MB as RGBA8 |
| World vertex (`WorldVertex`) | 52 bytes |
| Retail install | 635 MB: BLOOD2.REZ 255, SOUND.REZ 195, BLOOD2NM.REZ 104, BLOOD2P 23, BLOOD2P2 2.2, Music 12 (five sets of 2.1–3.1 MB with their DLS banks), Intro.smk 26 |
| One save (`current.ob2sav`) | 194 KB |

A 32-bit target halves pointers and `size_t`, but most of the footprint is owned
decoded data (images, collision structures, models, strings). The first lot of
any port is a heap profile per world, not a renderer.

## Precedents

| Port | What it shows |
| --- | --- |
| KISS Psycho Circus (2000, Tremor, Windows CE) | A LithTech game did ship on the Dreamcast (reportedly LithTech 1.5, *unconfirmed*). Multiplayer was cut; reviews report slowdowns in busy fights or reduced detail; Metacritic 59 against 71 on PC. |
| Unreal Tournament (Secret Level) | "A 64 MB PC game in 16 MB": Assault mode and larger maps were dropped, few maps keep eight players. |
| Quake III Arena (Raster) | Maps modified to fit memory (Carmack, *unconfirmed* snippet); simplified textures, about 30 fps. |
| Half-Life (Captivation/Gearbox) | Cancelled in 2001 over frame rate and load times; decompiled in 2026. |
| Soldier of Fortune | Dropped multiplayer, some effects and geometry; long loads. |
| Quake II homebrew (2002) | Lightmaps as 8-bit paletted textures, 256×256 textures without mips, reduced limits. |
| dca3 (GTA III / Vice City, 2024–25) | Closest analogue: a modern reimplementation (re3/librw, `-std=gnu++20`) with a Dreamcast renderer backend driving the PVR directly, opaque/punch-through/translucent lists, vertices transformed 128 at a time in locked cache and streamed out. Offline tools for VQ, triangle strips, texture shrinking and audio downsampling; ~2–2.5 MB for textures and under 2 MB for audio; heap fragmentation ends a session after about an hour. Started July 2024, alpha in December 2024. |
| Doom 64, Super Mario 64, Star Fox 64 (KallistiOS) | Recent ports show the current toolchain carries real 3D games. |

## Constraints, in order of risk

### 1. Memory

16 MB must hold code, the simulation, world geometry for the CPU transform,
models, I/O staging and audio staging (sound RAM is separate but small). A
plausible budget: 3–4 MB code (no inspector, launcher or spdlog; `-Os`, LTO,
`--gc-sections`), 1 MB system and stacks, the rest per level. CityHub's 76 MB
must come down about fivefold. Directions, all of which also help Android, iOS
and Quest:

- Offline-converted runtime formats read straight from disc: textures go to VRAM
  in PVR format and are never kept decoded in main RAM.
- Compact world vertices for the CPU transform (position, UV, packed colour);
  lightmaps stay in VRAM.
- Interned names and compact collision/BVH nodes; shared immutable model data
  (partly done for ABC assets); compressed animations, as dca3 did for Vice City.
- Sounds staged in sound RAM, long sounds streamed.
- Pools or arenas per level against fragmentation, which dca3 still hits.
- A desktop "budget" build with a counting allocator and the Dreamcast limits, so
  CI catches regressions without hardware.

If a world cannot reach the budget without changing original behavior, the port
needs level-specific cuts, as every commercial port of the era made. This project
does not make them silently: each would be a documented, inspectable omission.

### 2. CPU

- **Visibility.** With no hardware T&L, every submitted vertex costs SH-4 time.
  The presenter only frustum-culls, and world clustering/PVS was deferred on
  measured desktop evidence. A Dreamcast build needs real visibility: the DAT's
  per-leaf visibility lists, or a replacement.
- **Precision.** The collision solver and several clocks use `double` on purpose.
  `-m4-single` keeps them correct but slow; `-m4-single-only` would silently break
  their contracts. A float collision path would be a Dreamcast-specific
  replacement policy with its own tests, decided after profiling.
- **Audio and music.** The software DLS sampler, IMA sequencer and float mixer run
  on the main CPU today; on the Dreamcast they belong on the AICA.
- **Original requirements.** The original game asked for a Pentium 166 with a
  DirectX 6 accelerator; the SH-4 is in that class. The replacement collision
  solver, typed message checks and bounded copies cost more than the original
  code did, so the frame budget must be profiled on hardware early.

### 3. Rendering

The Dreamcast renderer is the **original renderer only**. Mapping of the current
features:

| Current feature | PowerVR2 mapping |
| --- | --- |
| Albedo × baked vertex light | Gouraud vertex colour, opaque list |
| Lightmaps (`albedo × 2 × lightmap`) | Explicit second pass in the translucent list (modulate 2×, depth equal), or lightmaps baked into paletted/VQ pages; or the original `LightMap 0` low detail, vertex light only |
| Authored translucent faces, sprites, particles, PolyGrids | Translucent list with hardware per-pixel sorting, which removes the open "global translucent ordering" gap |
| Chroma-keyed / alpha-tested textures | Punch-through list |
| `FLAG_SHADOW` model shadows | Modifier volumes ("cheap shadow" mode), or dropped |
| Sky | Drawn first with its own projection |
| Underwater fog (open gap on desktop) | Table fog |
| GPU skinning | CPU skinning, the existing `sample_vertices` path |
| DTX textures | Offline conversion to VQ or twiddled 565/1555/4444, with mips |
| Menus and HUD | Textured quads at the native 640×480 |
| Enhanced renderer, normal maps, per-pixel model light, XR | Not available |

VRAM budget: SubwayA's 6.16 MB of RGBA8 world textures become ~3.1 MB at 16 bits,
above dca3's 2–2.5 MB texture budget, or roughly 0.8–1 MB as VQ. VQ is the
default; 16-bit is for small or alpha textures.

### 4. Audio

SOUND.REZ is 195 MB of WAV against 2 MB of sound RAM. Convert sounds offline to
4-bit ADPCM (4:1 against 16-bit PCM) and downsample where inaudible, stage per
level and stream long cues. For music, the DLS banks alone (2–3 MB per set) do not
fit beside the effects. Two options:

- **AICA as the synthesizer.** The AICA's hardware voices (loops, envelopes,
  pitch) play the decoded DLS regions while the IMA sequencer stays on the SH-4.
  Most faithful, most work; banks still need trimming to fit.
- **Offline-rendered loops.** Render each music set to streamed ADPCM. Simple, but
  loses the original interactive transitions.

### 5. Data, discs and saves

Commercial data stays external. A desktop tool converts the user's own install
into a Dreamcast data image: per-level contiguous packs (slow optical seeks), VQ
textures, ADPCM sounds, and a replacement or omission for the Smacker videos.
Users burn it to CD-R (MIL-CD) or use an optical drive emulator or SD adapter.
Saves need compression and fewer slots to fit a VMU, or go to SD.

### 6. Out of scope on the Dreamcast

Launcher, inspector, XR, the enhanced renderer and LAN play (the Broadband
Adapter exists, but M9 is not designed for it).

## Fork or abstraction

### Dedicated fork

Total freedom for memory shortcuts and float collision, and no effect on the main
repository. But `src/game` is the bulk of the work and keeps changing: every fix
would be merged by hand, contracts and tests would diverge, and the fork would
fall behind within weeks. It suits a frozen snapshot, which the project is not.

### Low-level RHI (buffers, textures, pipelines, passes)

It fits two shader-based APIs (SDL_GPU and OpenGL), not the PowerVR2: no shaders,
no hardware transform, screen-space vertices submitted into typed display lists.
Covering it means either a fixed-function lowest common denominator, which would
remove the enhanced renderer and XR, or an abstraction that leaks everywhere.

### High-level scene backend (recommended)

The game produces a backend-neutral frame description; each backend renders it
its own way. This is Quake II's renderer interface, LithTech's own `.ren` modules
and dca3's Dreamcast librw backend. Here the cut already exists in practice:

- `SceneSnapshot`: today's `ScenePresenter::update()` state (world mesh and
  draws, sky order, model/sprite/PolyGrid views, lights, camera, 2D canvases).
- `SceneBackend`: today's SDL_GPU recording (registry, pipelines, renderers,
  shadow atlas, XR eyes) and, later, a PVR backend.
- A platform frame interface for acquire/submit, replacing the inline sequence in
  `run_frontend`.

Do the split as a pure refactor when Dreamcast work starts, validated by the
existing pixel tests (original frames byte-identical, `gpu.surface`). Before
then it only adds indirection.

### Recommended layout

- One repository, a `dreamcast` preset with a KallistiOS toolchain file, game
  only.
- Platform code in its own directory (PVR backend, VMU and AICA glue where SDL3
  does not cover it), the converter in `tools/dreamcast/`.
- A Dreamcast profile flag for budgets and replacement policies, documented like
  the other replacements in [provenance](provenance.md).
- GPF's SDL3 for input, audio, timers and the frame loop; the native PVR backend
  owns video. GLdc only if a quick first light is worth a temporary path.

## OpenGL

OpenGL would extend the game to machines without Vulkan or Metal (older GPUs and
drivers) and, through WebGL 2, to browsers. It is a shader-based backend: it would
sit beside SDL_GPU, either as a second scene backend or under a thin RHI shared
with SDL_GPU (the shaders are already HLSL compiled to SPIR-V, which SPIRV-Cross
can translate to GLSL). It does not help the Dreamcast: GLdc is OpenGL 1.2 fixed
function, so a desktop GL 3 backend and the Dreamcast would share nothing below
the scene boundary. Decide it on its own merits; the scene backend boundary keeps
the door open at no extra cost.

## Suggested order, if it starts

Sizes use the M9 scale (S ≈ 1 lot, M ≈ 2–3, L ≈ 4–5, XL ≈ 6–8).

| # | Lot | Size |
| --- | --- | --- |
| DC.1 | Heap profile per world on desktop; 16 MB budget table; budget build in CI | M |
| DC.2 | KallistiOS preset; engine and game headless on the SH-4 (Flycast, then hardware over dcload-ip): world entry and simulation without rendering | M |
| DC.3 | Memory reductions until CityHub fits the budget (shared with mobile and Quest) | XL |
| DC.4 | Converter: retail install → Dreamcast packs (VQ textures, ADPCM sounds, per-level packs) | L |
| DC.5 | Scene backend split on desktop, pixel tests unchanged | M |
| DC.6 | PVR backend: world with vertex light, lightmap pass, CPU-skinned models, sprites/particles/PolyGrids, sky, 2D menus/HUD | XL |
| DC.7 | Visibility for the CPU transform budget | L |
| DC.8 | AICA sound effects, then the music strategy | L |
| DC.9 | Controller layout, VMU saves, disc image, hardware playtest | M |

About 30–40 lots. DC.3 carries most of the risk; DC.1 and DC.2 are cheap and
answer whether the rest is worth it.

## Questions for the Dreamcast contact

- Can GPF's SDL3 run input, audio and timers while the application owns the PVR
  directly (no GLdc video)?
- An existing ImGui renderer for the PVR, and the recommended VQ/texture
  converter.
- Heap and fragmentation practice for a 16 MB C++ program (dca3's experience).
- Whether sh4zam or hand-written `FTRV` loops are the usual route for CPU
  transform and skinning.

## Sources

- KallistiOS: [repository](https://github.com/KallistiOS/KallistiOS), [kos-chain configuration](https://github.com/KallistiOS/KallistiOS/blob/master/utils/kos-chain/Makefile.dreamcast.cfg), [stable profile](https://github.com/KallistiOS/KallistiOS/blob/master/utils/kos-chain/profiles/dreamcast/stable.mk), [C++ examples](https://github.com/KallistiOS/KallistiOS/tree/master/examples/dreamcast/cpp), [atomics example](https://github.com/KallistiOS/KallistiOS/blob/master/examples/dreamcast/basic/threading/atomics/atomics.c), [PVR headers](https://github.com/KallistiOS/KallistiOS/tree/master/kernel/arch/dreamcast/include/dc/pvr), [AUTHORS](https://github.com/KallistiOS/KallistiOS/blob/master/AUTHORS), [kos-ports SDL](https://github.com/KallistiOS/kos-ports/blob/master/SDL/Makefile), [dcload-ip](https://github.com/KallistiOS/dcload-ip), [libkgl (deprecated)](https://github.com/KallistiOS/libkgl).
- SDL3 for Dreamcast: [GPF/SDL, branch dreamcastSDL3](https://github.com/GPF/SDL), [build script](https://github.com/GPF/SDL/blob/dreamcastSDL3/build-scripts/dreamcast.sh).
- [GLdc](https://gitlab.com/simulant/GLdc); [sh4zam](https://github.com/gyrovorbis/sh4zam); [GCC SH-4 machine model](https://github.com/gcc-mirror/gcc/blob/master/gcc/config/sh/sh4.md); [Hitachi SH-4 (Hot Chips 9)](https://old.hotchips.org/wp-content/uploads/hc_archives/hc09/3_Tue/HC9.S7/HC9.7.1.pdf); [Microprocessor Report on the SH-4](https://www.cecs.uci.edu/~papers/mpr/MPR/ARTICLES/101408.PDF).
- Hardware: [Copetti, Dreamcast architecture](https://www.copetti.org/writings/consoles/dreamcast/), [Dreamcast](https://en.wikipedia.org/wiki/Dreamcast), [GD-ROM](https://en.wikipedia.org/wiki/GD-ROM), [VMU](https://en.wikipedia.org/wiki/VMU), [Flycast](https://github.com/flyinghead/flycast).
- Ports: [KISS Psycho Circus](https://en.wikipedia.org/wiki/Kiss:_Psycho_Circus_-_The_Nightmare_Child), [its Dreamcast review (Pixel Empire)](https://www.thepixelempire.net/kiss-psycho-circus-the-nightmare-child-dc-review.html), [another (Everyeye)](https://www.everyeye.it/articoli/recensione-kiss-psycho-circus-5297.html); Unreal Tournament ([MobyGames reviews](https://mobygames.com/game/587/unreal-tournament/reviews/dreamcast), [Secret Level interview](https://dreamcastlive.net/blogs/post/interview-with-josh-adams-co-founder-of-secret-level/)); [Quake III maps (Blue's News)](https://www.bluesnews.com/s/11909/on-q3a-dc-maps); [unreleased Half-Life games](https://en.wikipedia.org/wiki/Unreleased_Half-Life_games), [Half-Life Dreamcast decompilation](https://www.timeextension.com/news/2026/09/half-life-dreamcast-has-been-decompiled-freeing-source-code-and-adding-online-multiplayer); [Soldier of Fortune](https://segaretro.org/Soldier_Of_Fortune); [Quake II homebrew](https://dcquake.sourceforge.net/); dca3 ([repository](https://gitlab.com/skmp/dca3-game), [Software Engineering Daily interview](https://softwareengineeringdaily.com/wp-content/uploads/2025/04/SED1825-GTA3-on-Dreamcast.txt), [GTA III article](https://www.thedreamcastjunkyard.co.uk/2024/12/grand-theft-auto-3-for-dreamcast-dca3.html), [Vice City article](https://www.thedreamcastjunkyard.co.uk/2025/04/grand-theft-auto-vice-city-has-been.html)); [Doom 64](https://www.timeextension.com/news/2024/08/fan-made-doom-64-port-makes-dreamcast-history); [Super Mario 64 / Star Fox 64](https://www.timeextension.com/news/2026/03/this-super-mario-64-footage-may-not-look-like-much-but-marks-a-huge-change-for-gamecube-devs).
- Blood II requirements: [original manual](https://retrogamer.biz/wp-content/uploads/2016/02/Blood_II_Manual.pdf) (Pentium 166, 64 MB, DirectX 6 accelerator; or P233 and 32 MB without one).
