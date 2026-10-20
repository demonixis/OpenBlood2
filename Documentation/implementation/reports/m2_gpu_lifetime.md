# M2 — GPU lifetime attribution

The retained GPU LeakSanitizer failure is reproducible outside Blood2. A new
SDL-only executable reports the same allocation count and NVIDIA allocation
sizes as the complete SubwayA-to-CityHub route. A second standalone Vulkan
program reproduces the same families without linking SDL. This resolves the
previously unknown module attribution; it does **not** fix the platform leak or
claim a clean GPU sanitizer run. M2 remains incomplete pending final acceptance.

Implementation commit: bfa0f75a8ff2dd6ab66a7ad5883acd59dd57a7b5.

## Isolation and results

The SDL executable uses no engine, original game, renderer, commercial data,
ImGui or audio code. It mirrors production device/window cleanup and optionally
repeats it three times. It links only the existing pinned SDL target. The
standalone Vulkan experiment creates an instance, enumerates physical devices
and destroys the instance using the system loader. Both select the NVIDIA ICD.

| Clang ASan/UBSan experiment | Outcome | Leaked bytes | Allocations |
| --- | --- | ---: | ---: |
| SDL video only, one and three cycles | Pass | 0 reported | 0 reported |
| SDL window, one and three cycles | Pass | 0 reported | 0 reported |
| SDL_GPU, one cycle | Fail at process exit | 44,024 | 816 |
| SDL_GPU, three cycles (initial probe) | Fail at process exit | 132,392 | 2,448 |
| SDL_GPU, one cycle with diagnostic D-Bus shutdown hint | Fail at process exit | 44,024 | 816 |
| Existing application GPU smoke | Checks complete; leak failure at exit | 44,024 | 816 |
| Complete original train route and CityHub entry | Checks complete; leak failure at exit | 44,056 | 816 |
| Standalone Vulkan enumeration | Checks complete; leak failure at exit | 21,996 | 408 |

All leak probes use `detect_leaks=1:halt_on_error=1`; UBSan also halts on error.
`allow_addr2line=1` supplies symbols on this host, where llvm-symbolizer is absent.
No suppressions, disabled leak check, retained driver handles or leak-root edits
are used. Ordinary Debug and inspector-free Release each pass the three new
CTest cases (three lifetimes per case). Those ordinary runs are separate from
the failing instrumented GPU runs. No Intel GPU or native Metal run is claimed.
The final formatted executable is rebuilt in all three configurations and its
three-cycle sanitizer probes are retained separately in the ledger.

An exploratory X11 variant exceeded its explicit 90-second process limit and
was killed. Its empty buffered output does not locate the stall; it is a failure,
not evidence of a working alternate backend. The initial CMake edit omitted an
`endif`, failed regeneration and was corrected before successful builds. Both
failed attempts remain recorded. Existing application assertions, dependencies,
original-source imports and gameplay behavior are unchanged.

## Allocation attribution

`LD_DEBUG=files` retains load bases/sizes before the driver libraries unload.
For the first mapped SDL baseline and the train run, the analysis maps every unresolved frame to
`libnvidia-glcore.so.615.78.08` or `libnvidia-glsi.so.615.78.08`. Reused address
ranges retain all candidate offsets instead of inventing a unique offset. The
first allocator caller in every group resolves to one of those libraries or
`libdbus-1.so.3`; there are no unmapped groups in these fresh reports.

The final formatted-baseline recheck produces the same 44,024 bytes / 816
allocations. Its loader reused a range between NVIDIA glcore and gpucomp, so
270 allocation call sites retain both NVIDIA candidates. The analysis does not
guess which unloaded library occupied that range when those allocations occurred.
The table below uses the first baseline, where caller-module candidates are unique.

| Allocator caller | SDL-only bytes / allocations | Full route bytes / allocations |
| --- | ---: | ---: |
| NVIDIA glcore | 26,036 / 540 | 26,036 / 540 |
| NVIDIA glsi | 13,018 / 270 | 13,018 / 270 |
| D-Bus | 4,970 / 6 | 5,002 / 6 |

The standalone Vulkan case reports 270 glcore, 135 glsi and three D-Bus
allocations. The 32-byte difference between SDL-only and full-route runs is in
D-Bus buffers; its exact internal cause is not asserted. Some D-Bus allocation
stacks include SDL initialization. Allocation site does not establish ownership
of later pooled/reused storage. Video/window-only success and the independent
Vulkan failure establish the external reproduction without guessing ownership
from one stack. The three-cycle growth is retained, not dismissed as a harmless
constant allocation. World replacement reuses the application's GPU device;
it does not perform another `SDL_CreateGPUDevice` per level.

The historical 44,056-byte report remains byte-identical. Its old executable
addresses were not symbolicated against the current binary. This attribution is
based on fresh loader maps and current binaries, whose identities are recorded.

## Session checks retained

The fresh sanitizer route drives the original intro, mouse/keyboard input,
Beretta combat, ten Cultist deaths, item contacts, all ten delayed relay sends,
fade and the authored NEW_LEVEL into CityHub. It renders three checkpoints with
179,200 non-background pixels each and no fallback pixels, restores the player,
updates the destination and tears down objects, messages, strings, lists,
clients and voices. It reaches all assertions, then LeakSanitizer exits 1.
It is therefore a completed behavioral scenario with a failed overall process,
not a passing GPU test. Its complete service rows remain in the raw log.

The previous [98-case session/startup checks](m2_session_start.md) and
[991-case full non-GPU audit](m2_session_cost.md) remain the evidence for clean
headless ASan/UBSan paths. They were not needlessly rerun for a diagnostic-only
change. This run builds the diagnostic in Debug, Release and Clang sanitizers,
checks ordinary NVIDIA device lifetimes and repeats the real sanitized route.
No new inspector state is invented for a process-exit report; existing resource,
service, startup and ownership inspection remains unchanged.

## Reproduction and remaining gate

Build `platform_lifetime_tests` with the project's sanitizer configuration, then
run `build/sanitize/tests/platform_lifetime_tests gpu 1` with the NVIDIA ICD and
the sanitizer options above. Use `video 3` or `window 3` for the controls.
The [contracts](../../contracts/gpu_lifetime_contracts.md) define
ownership and CTest registration. The standalone Vulkan source, compiler command,
loader logs, attribution script, JUnit files and exact environment are retained
in the [evidence ledger](evidence/m2-gpu-lifetime.json). No commercial images or
binaries are committed; local captures stay in the ignored build directory.

The [M2 exit gate](../../history/Roadmap/03_milestones.md) has not been relaxed. Its final
review must reconcile the selected session's required service paths and explicit
resource exceptions with the current evidence, and retain this open graphics
platform failure. Attribution alone is not a sanitizer pass. Further unrelated
weapons, music parity or campaign features cannot repair this standalone driver
reproduction and should not be added as invented M2 exit requirements.

All 4,430 local Markdown links validate. Changed owned code and maintained Markdown
pass formatting/whitespace checks; raw logs and patch context retain original bytes.
All 10,923 inventoried reference files and six reference repositories remain
unchanged. The user-edited workspace README is byte-identical.
