# Homebrew port roadmap

Exploratory study, 2026-10-25, at `35122a8a`. No code was written and no target is
a milestone: the M0–M9 [roadmap](roadmap.md) is unchanged. It asks, for each
console the user named, whether a port is realistic with the open homebrew SDKs,
what it would cost, and which shared work would serve several of them.

Targets: Sega Dreamcast, PlayStation 3, R36S, Steam Frame, Nintendo Switch, PSP
and PS Vita. The Dreamcast already has a detailed
[study](implementation/dreamcast_study.md) (2026-10-22); this document summarizes
it, adds the [SH4ZAM](https://sh4zam.com/) evaluation and places it among the
other targets. Web facts were checked on 2026-10-25; [sources](#sources) are at
the end. Claims marked *unconfirmed* could not be verified from a primary source.

## Conclusion

| Rank | Target | Verdict | Size | Main risk |
| --- | --- | --- | --- | --- |
| 1 | Steam Frame | Cheapest. Native Linux arm64 + SDL_GPU Vulkan + OpenXR is the Quest code path on SteamOS; it overlaps M8 lot 8.12 (PCVR on Linux). | M | SteamOS packaging and the SteamVR ARM64 runtime, not the code. |
| 2 | R36S | Realistic. Linux aarch64, 1 GB, 640×480 is the original menu resolution. | M–L | A Cortex-A35 CPU and a two-core Mali-G31; GLES backend unless PanVK works. |
| 3 | Switch | Realistic. Enough RAM and CPU; no public SDL3 and no Vulkan in homebrew. | L | An SDL3 platform layer and a GL/deko3d backend. |
| 4 | PS Vita | Plausible. Upstream SDL3, GCC 15.2, 512 MB; native GXM backend with Cg shaders. | L–XL | A 444 MHz Cortex-A9 against the replacement physics and the software music. |
| 5 | PS3 | Plausible but expensive. Ample memory; big-endian, no SDL3, RSX backend. | XL | Endianness and the platform layer before any rendering. |
| 6 | PSP | Research project. Hardware transform helps, but 32–64 MB and no double-precision FPU. | XL+ | Memory (like the Dreamcast) and `double` in the collision solver. |
| 7 | Dreamcast | Research project; the study's conclusion stands. SH4ZAM is the right math layer for the PVR backend. | XL+ (30–40 lots) | 16 MB of RAM against 47–76 MB measured. |

Three decisions follow, all consistent with the Dreamcast study:

- **One repository, one preset per target, game only** (like iOS, Android and
  Quest). No fork: `src/game` is most of the value and still changes daily.
- **A high-level scene backend**, split from `ScenePresenter` as a pure refactor
  (DC.5 of the study), is the prerequisite of every target except Steam Frame and
  possibly R36S. Backends then follow the hardware families: SDL_GPU (desktop,
  Quest, Steam Frame), OpenGL / GLES 3 (Switch, R36S, and desktop GL as a side
  effect), Cg on GXM or RSX (Vita, PS3), fixed function (PSP GU, Dreamcast PVR).
- **Keep SDL3 as the platform API.** Where SDL3 has no public port (PS3, Switch),
  write the SDL3 platform backend out of tree, as a local patch like the existing
  `PatchSdlXrSwapchain.cmake`, rather than adding a second platform layer to the
  application.

Recommended order, if the user starts this work: Steam Frame (with 8.12), then
R36S, then the scene-backend split and a GLES 3 backend that serves R36S and
Switch, then Vita. PS3, PSP and Dreamcast wait for the memory and endianness
groundwork.

## What the code base already gives

Measured or counted at `35122a8a` unless the Dreamcast study is cited.

| Fact | Value | Consequence |
| --- | --- | --- |
| Language/toolchain | C++20, CMake ≥ 3.28, GCC/Clang. Apple targets need `std::from_chars` for floats. | Every target below has GCC ≥ 13.2 on its CPU (SPU excepted). |
| Dependencies | SDL3 (static, pinned), spdlog, ImGui (inspector), miniz; platform HTTPS stacks for the demo download. | spdlog and the inspector compile out; the demo download can be disabled on consoles. |
| SDL surface | 89 distinct non-GPU SDL functions; SDL_GPU referenced in 51 files (engine renderers, app presentation, XR). | The non-GPU half is what an SDL3 platform port must supply. |
| Gameplay | `src/game` uses no SDL_GPU or ImGui. | Portable in principle; the cost is CPU and memory. |
| Rendering | HLSL compiled offline to SPIR-V and MSL; original and enhanced shader sets; GPU skinning with the CPU `sample_vertices` path kept for tests. | SPIR-V can feed SPIRV-Cross (GLSL/ESSL); HLSL is close to Cg for GXM/RSX. The CPU skinning path is the Dreamcast/PSP path. |
| Formats | `formats/reader.h` reads explicit little-endian, bounded, without host-endian casts; save files are explicit little-endian. | A big-endian port starts from a good base, but pixel, lightmap and PCM blocks still need an audit. |
| Precision | `double` in 168 engine/game files, including the collision solver (exact SAT contracts) and the DLS sampler. | Free on ARMv7/ARMv8/PPU, slow on SH-4, software-emulated on the PSP. |
| Memory | Simulation only: 47.5 MB peak (SubwayA), 76.1 MB (CityHub), macOS arm64 ([Dreamcast study](implementation/dreamcast_study.md#measurements)). | Fits everything but PSP (32–64 MB) and Dreamcast (16 MB). |
| Audio | Software mixer in float, software DLS sampler and IMA sequencer on the main thread. | Real CPU cost on A35, A9, Allegrex and SH-4. |
| Data | Retail install 635 MB; commercial data stays external. | Each target needs a documented way to copy the user's own REZ files. |
| Display | Original menus are 640×480 canvases. | Native on R36S and Dreamcast, scaled on Vita/Switch, downscaled to 480×272 on PSP. |

## Targets

### Sega Dreamcast

The [Dreamcast study](implementation/dreamcast_study.md) remains the reference:
KallistiOS with GCC 15.2 (16.x in testing profiles), GPF's SDL3 fork for input,
audio and timers without SDL_GPU, a native PVR scene backend, an offline data
converter (VQ textures, ADPCM sounds, per-level packs), and memory as the
dominant risk. Its lots DC.1–DC.9 are not repeated here.

**SH4ZAM.** [SH4ZAM](https://sh4zam.com/) 0.10.0 (Falco Girgis and contributors,
MIT) is a hand-optimized SH-4 math library that grew out of the inline assembly
written for the GTA III / Vice City ports (dca3). Facts relevant to this project:

| Aspect | SH4ZAM | Use here |
| --- | --- | --- |
| API | Interoperable C11 and C++17 APIs, extras under C++20; headers `<sh4zam/shz_sh4zam.hpp>`, `-lsh4zam`; in kos-ports or built with CMake. | Fits the C++20 build directly. |
| Portability | A software back-end for cross-platform code bases. | The Dreamcast backend and its tests can build on desktop CI. |
| Modules | Scalar (`<math.h>` replacements), trigonometry (`FSCA` sine/cosine pairs), 2D–4D vectors, 2×2 to 4×4 matrices, quaternions (slerp/squad), `XMTRX` (the SH-4 back-bank matrix registers), complex/FFT, and memory (`memcpy`/`memmove`, store queues, cache control). | `XMTRX` + per-vertex transform for world and model vertices; store queues to stream vertices to the tile accelerator; quaternion slerp for ABC animation sampling; cache prefetch in the vertex loops. |
| Compiler | Tuned for SH-GCC 16.x. | Prefer a kos-chain 16 profile over the 15.2 stable one. |
| Users | Super Mario 64 DC, Mario Kart 64 DC, Diddy Kong Racing DC, Sonic Mania DC, Quake 2 DC (matrix math), Simulant engine; twenty showcased projects. | Established in the current Dreamcast scene. |
| Evidence | Validated on hardware per its authors; the site publishes no benchmark numbers. | Gains must be measured on hardware (Flycast does not model cache timing). |

Where it applies, and where it does not:

- **Yes:** the PVR backend's world/model transform and clip loop, CPU skinning
  (one 3×4 matrix per node loaded into `XMTRX`), sprite and particle billboards,
  animation interpolation, and fast inverse square roots for normals.
- **No:** `src/game` and the collision solver. Their contracts depend on the
  current `double` arithmetic; SH4ZAM is single precision. A float collision path
  would be a separate, documented Dreamcast replacement policy with its own tests,
  decided after profiling (study, constraint 2). SH4ZAM types stay inside the
  Dreamcast backend and skinning code, never in shared game code.
- **No:** audio. Effects and music belong on the AICA (study, constraint 4).

SH4ZAM lowers the CPU risk of rendering; it does nothing for the 16 MB memory
budget, which is still the gate (DC.1–DC.3). The study's order stands: profile
memory per world first, then decide.

### PlayStation 3

| Resource | Value | Consequence |
| --- | --- | --- |
| CPU | Cell: one PPU (PowerPC 64, 3.2 GHz, two hardware threads, in order, **big-endian**) and SPUs (256 KB local store each). | Single-thread performance is adequate for a 1998 game; SPUs could take audio/skinning later. |
| Memory | 256 MB XDR main (part reserved by the OS) and 256 MB GDDR3; RSX can also address main memory. | The 76 MB simulation fits; textures go to GDDR3. |
| GPU | RSX, GeForce 7 class: separate vertex/fragment programs, shader model 3, 720p. | Original renderer and probably per-pixel model light; the enhanced renderer and post-processing are doubtful. |
| SDK | PSL1GHT with ps3toolchain: PPU GCC 13.2.0, SPU GCC 9.5.0, active (last commit 2026-10-11). ps3libraries packages SDL 1.2/SDL2 PSL1GHT ports, Tiny3D, libcurl, pthread-emb. | C++20 on the PPU is mostly available (GCC 13); no SDL3. |
| Graphics API | The PSL1GHT RSX/GCM-style command API, Tiny3D on top; RSXGL (OpenGL 3.1 core subset with runtime GLSL) is a 2012 hobby project, *status unconfirmed*. | A native RSX backend with Cg programs; HLSL ports to Cg with modest changes. |
| Tooling | RPCS3 for early runs; hardware via HEN/CFW. | Emulator first, hardware for timing. |

Work specific to the PS3, in order:

1. **Big-endian CI** before anything else: build and run the headless tests on a
   big-endian Linux under QEMU user mode (ppc64 or s390x). It finds host-endian
   reads in pixel, lightmap, PCM and save code without console hardware, and it
   protects all other targets from regressions.
2. **SDL3 platform backend for PSL1GHT** (video surface, pad, audio, threads,
   timers, filesystem), derived from the SDL2 PSL1GHT port as a local patch.
3. **RSX scene backend** with Cg programs compiled offline (PSL1GHT ships a Cg
   compiler path, *unconfirmed which version*), textures in GDDR3.
4. Audio stays on the PPU at first; SPU offload only on measured need.

### R36S

| Resource | Value | Consequence |
| --- | --- | --- |
| SoC | Rockchip RK3326: four Cortex-A35 up to ~1.5 GHz; Mali-G31 MP2. | The weakest CPU of the Linux targets; GPU suits 640×480 with the original renderer only. |
| Memory | 1 GB (DDR3L or DDR4 depending on the batch). | Fits. |
| Display/input | 3.5-inch 640×480 IPS; two sticks, d-pad, face/shoulder buttons; two microSD slots. | Native menu resolution; the M7 gamepad layout applies. |
| OS | ArkOS (stock or community), ROCKNIX, AmberELEC; ports distributed through PortMaster. | Package as a PortMaster port; build against an old glibc sysroot. |
| Graphics | ROCKNIX lists libmali (GLES 3.2) and Panfrost (GL/GLES 3.1) for RK3326. Mesa lists PanVK at Vulkan 1.3 for Bifrost v7 (G31 included; conformance submitted for G52). | **Try SDL_GPU on PanVK first**: if it runs, no new backend. Otherwise the GLES 3 backend. |
| SDL | Upstream SDL3 KMSDRM video with EGL/GLES or Vulkan, statically linked as today. | No SDL fork. |

Work: an `r36s` (generic `linux-arm64-handheld`) preset with a cross toolchain and
sysroot, a PanVK experiment on ROCKNIX, then detail defaults for the device
(original renderer, `LIGHTING PER VERTEX`, model shadows off, post-processing
off). The CPU is the risk: the replacement collision solver, AI activation rays
and the software music sampler must be profiled on the device at CityHub; the
inspector-free Release build is the only meaningful measurement. Clones with
different panels or controls exist; target the PortMaster device database, not
one board.

### Steam Frame

| Resource | Value | Consequence |
| --- | --- | --- |
| SoC | Snapdragon 8 Gen 3 (Adreno 750), 16 GB LPDDR5X, 256 GB/1 TB UFS plus microSD. | Same class as the Quest 3's Snapdragon XR2 Gen 2, with more memory. |
| Display | 2160×2160 per eye, up to 120 Hz (144 Hz experimental). | Render scale (50–100 %) already exists for XR. |
| OS | SteamOS (Arch based) on arm64. Valve's documented paths: Windows x86 through Proton + FEX, Android through Lepton, native arm64. Native Vulkan; GL through Zink on Turnip. | Run natively: a Linux arm64 build, no emulation. |
| XR | SteamVR's ARM64 OpenXR runtime on the headset; community ports (OpenMW-VR, Quake VR, Takedawn) render natively through it. Controllers currently via SteamVR binding emulation of Index/Touch profiles. | Same OpenXR code as the Quest, plus Index/Touch suggested bindings (planned in 8.12). |
| Release | Retail availability date varies by source (spring 2026 versus 2026-09-14, *unconfirmed*). | Verify on the user's device. |

This is not "no PCVR yet": the Frame runs the game **standalone**, like the Quest,
on Linux instead of Android. The desktop OpenXR path of lot 8.12 (system
`libopenxr_loader.so.1`, Index/Vive profiles, Linux) is exactly what the Frame
needs, built for arm64. Work:

1. `linux-arm64` desktop preset with `BLOOD2_XR`: SDL_GPU Vulkan on Turnip, the
   system OpenXR loader, the XR runtime contract unchanged (LOCAL_FLOOR → STAGE →
   LOCAL, UNORM views of sRGB swapchains, two-layer swapchain).
2. Interaction profiles beyond Touch (`valve/index_controller` and the Frame's own
   profile once published); the Touch layout maps onto them.
3. Packaging: a self-contained tarball or a non-Steam shortcut first (as the
   community ports do), with data in the user's home directory; Steam Linux
   Runtime compatibility checked before any store packaging.
4. Performance: the Quest detail defaults and lot 8.4 multiview carry over;
   SteamVR reprojection makes a steady frame rate more important than a high one.

The same build also gives a desktop Linux arm64 game (Asahi, ARM laptops) for free.

### Nintendo Switch

| Resource | Value | Consequence |
| --- | --- | --- |
| SoC | Tegra X1: four Cortex-A57 at 1.02 GHz (three for applications), Maxwell GPU (256 cores, 307–768 MHz). | Comfortable for the simulation; original renderer at 720p, enhanced renderer at reduced settings. |
| Memory | 4 GB, about 3.2 GB for a title; homebrew launched as an applet gets far less (*~400 MB, unconfirmed*). | Launch through title takeover; the build must still fit applet mode for the menus. |
| SDK | devkitPro: devkitA64 (GCC 16.x in the current build scripts), libnx; portlibs include SDL2 2.28.5, Mesa 20.1 (nouveau: OpenGL and GLES), deko3d. | C++20 is fine. |
| SDL3 | Upstream SDL3 supports Switch and Switch 2 only under Nintendo NDA, in a separate repository. devkitPro ships SDL2 only. A public SDL3 libnx backend exists as a personal, AI-generated fork. | Write or adapt an SDL3 libnx backend as a local patch; do not depend on the NDA port. |
| GPU APIs | Mesa nouveau OpenGL (runtime GLSL) or deko3d (NVN-like, shaders precompiled from GLSL with `uam`). No Vulkan driver in homebrew Mesa 20.1. | SDL_GPU has no GL backend (upstream has Vulkan, Metal and D3D12), so a project backend is required. |

Work: the scene-backend split, then the **GLES 3 / GL 4 backend** shared with
R36S (SPIRV-Cross from the existing SPIR-V, generated offline and checked in like
MSL), then the SDL3 libnx backend, an NRO package, and data on the SD card. deko3d
is a later performance option if nouveau GL is too slow; it reuses the same GLSL.
Switch 2 homebrew is not considered: its homebrew scene is not established.

### PSP

| Resource | Value | Consequence |
| --- | --- | --- |
| CPU | MIPS Allegrex at 333 MHz with a **single-precision-only FPU** and the VFPU vector unit; the Media Engine is a second, separately programmed core. | Every `double` (collision solver, DLS sampler, clocks) becomes software emulation. |
| Memory | 32 MB (PSP-1000), 64 MB (later models; homebrew can use most of the extra 32 MB); 2 MB eDRAM VRAM. | The Dreamcast memory problem, slightly relaxed. |
| GPU | Graphics Engine: fixed function, hardware transform and skinning (up to eight matrices), CLUT4/8 and DXT textures, 480×272. | The best fixed-function target: no CPU transform, unlike the Dreamcast. |
| SDK | pspdev (pspsdk, GCC from the `allegrex-v15.2.0` branch); upstream SDL3 has a PSP backend and a PSP 2D renderer. | Toolchain and platform layer exist. |
| Storage | Memory Stick (Pro Duo, microSD adapters), 16 GB internal on PSP Go. | Data fits; read speed matters. |

The PSP shares the Dreamcast's prerequisites (memory profile and reduction, an
offline converter, a fixed-function backend) and adds one of its own: the
`double` arithmetic must be measured, and possibly replaced by a float path under
an explicit replacement policy. The 480×272 screen also requires menus and HUD
scaled from 640×480 with legible fonts. Do it after the Dreamcast memory lots, if
at all; it gains from them almost entirely.

### PS Vita

| Resource | Value | Consequence |
| --- | --- | --- |
| CPU | Four Cortex-A9 (three for applications), 333 MHz by default, 444 MHz available to homebrew; NEON, VFPv3 with doubles. | `double` is correct and acceptably fast; single-thread speed is the risk. |
| Memory | 512 MB main (homebrew can extend its heap well beyond the default) and 128 MB CDRAM. | Fits with room for textures. |
| GPU | PowerVR SGX543MP4+, programmable (GXM, Cg shaders), 960×544. | Original renderer; per-pixel model light and simple post-processing are possible, the enhanced renderer probably not. |
| SDK | VitaSDK with GCC 15.2.0; upstream SDL3 has Vita video, input and audio backends plus a GXM 2D renderer (no SDL_GPU). | No SDL fork. |
| GPU APIs | Native GXM with Cg compiled at runtime through the console's `libshacccg.suprx` (extracted by the user; vitaGL also requires it for GLSL) or precompiled; vitaGL (actively maintained, last commit 2026-10-22) offers a GL subset. | A native **GXM backend with Cg** sharing its shader sources with the PS3 RSX backend. vitaGL is a first-light shortcut. |

Work: the scene-backend split, a GXM backend (or vitaGL first light), the
HLSL-to-Cg port of the original shader set (cached compiled programs), detail
defaults for the device, and a VPK with data under `ux0:`. Profile the
simulation at 444 MHz early: if CityHub's AI and collision exceed the frame
budget, move audio mixing and music to a second core before anything else.

## Shared work

These lots serve several targets; each is useful on its own and none changes
original behavior. Sizes use the M9 scale (S ≈ 1 lot, M ≈ 2–3, L ≈ 4–5,
XL ≈ 6–8).

| # | Lot | Serves | Size |
| --- | --- | --- | --- |
| H.1 | `linux-arm64` preset (desktop and XR), cross sysroot and CI build | Steam Frame, R36S, ARM Linux desktops | S |
| H.2 | Per-world heap and CPU profile; budget build with a counting allocator and selectable limits (16/32/64/512 MB) in CI (study DC.1 generalized) | Dreamcast, PSP, Vita, R36S | M |
| H.3 | Big-endian headless CI under QEMU user mode; fix host-endian reads | PS3 | M |
| H.4 | Scene-backend split from `ScenePresenter`, pixel tests byte-identical (study DC.5) | All but Steam Frame | M |
| H.5 | GLES 3 / GL backend for the original renderer; SPIRV-Cross artifacts checked in with the others | R36S (if no PanVK), Switch, desktop GL | L |
| H.6 | Cg shader set for the original renderer, shared by GXM and RSX | Vita, PS3 | M |
| H.7 | Platform detail profiles: per-device defaults (renderer, lighting, shadows, post-processing, render scale), UI scaling for 480×272 and 960×544 | All | S–M |
| H.8 | Data converter: the user's install to per-target packs (texture formats, audio formats, per-level packs) (study DC.4 generalized) | Dreamcast, PSP; optional elsewhere | L |
| H.9 | Memory reductions (study DC.3), shared with mobile and Quest | Dreamcast, PSP | XL |
| H.10 | Out-of-tree SDL3 platform backends (libnx, PSL1GHT) as local patches with their own tests | Switch, PS3 | L each |

Per-target lots after the shared ones (rough):

| Target | Needs | Target-specific lots |
| --- | --- | --- |
| Steam Frame | H.1, M8 8.12 | Interaction profiles, packaging, on-device performance: S–M |
| R36S | H.1, H.7, then H.4 + H.5 if PanVK fails | PortMaster package, device profiling: M |
| Switch | H.4, H.5, H.7, H.10 | NRO packaging, controller layouts, applet/title-mode memory: M |
| PS Vita | H.2, H.4, H.6, H.7 | GXM backend, VPK, CPU budget work: L–XL |
| PS3 | H.3, H.4, H.6, H.10 | RSX backend, packaging, optional SPU audio: XL |
| PSP | H.2, H.4, H.7, H.8, H.9 | GU backend, float collision decision, ME audio: XL+ |
| Dreamcast | H.2, H.4, H.8, H.9 | Study lots DC.2, DC.6–DC.9 with SH4ZAM: XL+ |

## Rules that carry over

- Commercial data stays external on every target: the setup screen detects the
  user's copy; the demo download is disabled where no HTTPS stack is wired.
- The original renderer stays the reference; a backend that cannot reproduce a
  feature records an explicit, inspectable omission, never a silent substitute.
- Replacement policies needed only by one target (float collision, cut levels,
  reduced particle limits) are documented in [provenance](implementation/provenance.md)
  and gated by a platform profile, never applied to desktop.
- Device runs on consoles, handhelds and headsets happen only with the user's
  explicit approval, each time, like the Quest rule in AGENTS.md; agents build.
- Homebrew distribution follows each scene's conventions (PortMaster, NRO, VPK,
  PKG, EBOOT, CDI) and never includes game data.

## Sources

- SH4ZAM: [site](https://sh4zam.com/), [repository](https://github.com/gyrovorbis/sh4zam). Dreamcast sources: see the [Dreamcast study](implementation/dreamcast_study.md#sources).
- PS3: [ps3toolchain](https://github.com/ps3dev/ps3toolchain) (`scripts/002-gcc-newlib-PPU.sh`: GCC 13.2.0; `006-gcc-newlib-SPU.sh`: GCC 9.5.0), [PSL1GHT](https://github.com/ps3dev/PSL1GHT), [ps3libraries](https://github.com/ps3dev/ps3libraries) (SDL2 PSL1GHT, Tiny3D), [RSX](https://en.wikipedia.org/wiki/RSX_Reality_Synthesizer), [RSXGL (Phoronix, 2012)](https://www.phoronix.com/news/MTE5MTg), [Tiny3D](https://github.com/wargio/tiny3D).
- R36S: [ROCKNIX RK3326 device page (Odroid Go Super)](https://rocknix.org/devices/hardkernel/odroid-go-super/), [ROCKNIX 20241029 notes](https://androidpctv.com/rocknix-20241029/), [R36S overview](https://androidpctv.com/r36s-retro-console/), [Mesa Panfrost/PanVK support table](https://docs.mesa3d.org/drivers/panfrost.html).
- Steam Frame: [Valve developer wiki](https://developer.valvesoftware.com/wiki/Steam_Frame), [UploadVR announcement](https://uploadvr.com/valve-steam-frame-official-announcement-features-details), [Tom's Guide](https://www.tomsguide.com/computing/virtual-reality/valve-announces-steam-frame-vr-headset-a-premium-standalone-rival-to-the-meta-quest-3), [iFixit](https://www.ifixit.com/Device/Steam_Frame), [GamingOnLinux on Valve's standalone documentation](https://www.gamingonlinux.com/2026/01/valves-documentation-highlights-the-different-ways-standalone-games-run-on-steam-frame/), [OpenMW-VR on Steam Frame](https://github.com/stephen-moskal/openmw-vr-steam-frame), [Quake VR ARM64 PR](https://github.com/vittorioromeo/quakevr/pull/75), [Takedawn VR PR](https://github.com/xenophobentx/takedawn/pull/68).
- Switch: [SDL3 README-switch](https://wiki.libsdl.org/SDL3/README-switch/raw), [devkitPro build scripts](https://github.com/devkitPro/buildscripts), [devkitPro packages](https://github.com/devkitPro/pacman-packages) (SDL2 2.28.5, Mesa 20.1.0), [Switchbrew news archive](https://switchbrew.org/w/index.php?diff=5360), [community SDL3 libnx fork](https://github.com/neomody77/sdl3-switch).
- PSP: [pspdev](https://github.com/pspdev/pspdev), [pspdev GCC branches](https://github.com/pspdev/gcc), [psptoolchain-allegrex](https://github.com/pspdev/psptoolchain-allegrex), [SDL3 README-psp](https://wiki.libsdl.org/SDL3/README-psp/raw).
- PS Vita: [VitaSDK build scripts](https://github.com/vitasdk/buildscripts) (`cmake/Components.cmake`: GCC 15.2.0), [SDL3 README-vita](https://wiki.libsdl.org/SDL3/README-vita), [vitaGL](https://github.com/Rinnegatamante/vitaGL).
- SDL upstream backends: [SDL GPU drivers](https://github.com/libsdl-org/SDL/tree/main/src/gpu) (Vulkan, Metal, D3D12), [video drivers](https://github.com/libsdl-org/SDL/tree/main/src/video) (PSP, Vita, PS2, N3DS, KMSDRM).
