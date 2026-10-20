from pathlib import Path
import hashlib,json,subprocess
root=Path('/home/yann/projects/lithtech'); project=root/'Blood2'
contract='''# Platform lifetime diagnostics

`platform_lifetime_tests` isolates the SDL/platform boundary from the engine and
original game. It links only the pinned SDL target. No game, renderer, asset,
ImGui, audio mixer or engine object is constructed.

The `video`, `window` and `gpu` modes perform one or three complete lifetimes.
All initialize the same video/gamepad subsystems as `Runtime`; the latter two
create a window, and `gpu` creates a debug SDL_GPU device, claims the window,
submits an empty command buffer and waits. Cleanup waits for the GPU, releases
the window claim, destroys the device and window, then calls `SDL_Quit`.
Partial initialization failures take the same explicit cleanup path. No device
or connection is deliberately kept alive to change the leak detector's roots.

The executable prints the actual video driver/device and reports whether SDL's
own allocation tracker is available. An unavailable tracker is not zero leaks.
Only LeakSanitizer measures leaked host allocations in these experiments; this
test makes no claim about total GPU memory or vendor-internal allocation counts.

With `BLOOD2_GPU_TESTS=ON`, CTest registers `gpu.platform_video`,
`gpu.platform_window` and `gpu.platform_gpu`, each using three cycles, a real
display, serial execution and the unchanged 30-second ceiling. The executable
is also built when GPU tests are not registered, allowing explicit sanitizer
probes. All modes use normal process exit and retain sanitizer failures.

## Current Linux result

The NVIDIA 615.78.08 Vulkan stack reports leaks after GPU destruction in this
baseline and in the full train route. Video/window-only sanitizer probes pass.
A separate system-Vulkan reproducer without SDL also reports the same allocation
families. Loader maps identify all formerly unknown frames in NVIDIA glcore/glsi;
D-Bus allocations are kept separately. This localizes the reproducible failure
outside the game, but does not prove that every possible application path is clean
or turn the failing GPU sanitizer processes into passes.

Do not add leak suppressions, disable leak detection, retain unloaded libraries,
or set `SDL_SHUTDOWN_DBUS_ON_QUIT` in production to hide this result. The pinned
SDL header explicitly reserves that hint for debugging because other libraries
can still use D-Bus. An isolated hint experiment did not resolve the report.
No new platform-specific production dependency or driver workaround is introduced.

See [measurements and exact commands](../../../Documentation/Implementation/m2_gpu_lifetime.md).
'''
(project/'docs/contracts/gpu_lifetime_contracts.md').write_text(contract)
report='''# M2 — GPU lifetime attribution

The retained GPU LeakSanitizer failure is reproducible outside Blood2. A new
SDL-only executable reports the same allocation count and NVIDIA allocation
sizes as the complete SubwayA-to-CityHub route. A second standalone Vulkan
program reproduces the same families without linking SDL. This resolves the
previously unknown module attribution; it does **not** fix the platform leak or
claim a clean GPU sanitizer run. M2 remains incomplete pending final acceptance.

Implementation commit: IMPLEMENTATION_COMMIT.

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
The analysis maps every unresolved frame in the fresh SDL and train reports to
`libnvidia-glcore.so.615.78.08` or `libnvidia-glsi.so.615.78.08`. Reused address
ranges retain all candidate offsets instead of inventing a unique offset. The
first allocator caller in every group resolves to one of those libraries or
`libdbus-1.so.3`; there are no unmapped groups in these fresh reports.

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
The [contracts](../../Blood2/docs/contracts/gpu_lifetime_contracts.md) define
ownership and CTest registration. The standalone Vulkan source, compiler command,
loader logs, attribution script, JUnit files and exact environment are retained
in the [evidence ledger](evidence/m2-gpu-lifetime.json). No commercial images or
binaries are committed; local captures stay in the ignored build directory.

The [M2 exit gate](../Roadmap/03_milestones.md) has not been relaxed. Its final
review must reconcile the selected session's required service paths and explicit
resource exceptions with the current evidence, and retain this open graphics
platform failure. Attribution alone is not a sanitizer pass. Further unrelated
weapons, music parity or campaign features cannot repair this standalone driver
reproduction and should not be added as invented M2 exit requirements.
'''
(root/'Documentation/Implementation/m2_gpu_lifetime.md').write_text(report)
line='\nThe [GPU lifetime investigation](Documentation/Implementation/m2_gpu_lifetime.md) reproduces the retained NVIDIA LeakSanitizer report in SDL-only and standalone Vulkan programs. Read [contracts](Blood2/docs/contracts/gpu_lifetime_contracts.md); preserve ordinary teardown, leak detection and raw failures. Module attribution is resolved for fresh probes, but the GPU sanitizer failure is not fixed or suppressed. M2 acceptance remains open.\n'
with (root/'AGENTS.md').open('a') as f:f.write(line)
with (project/'AGENTS.md').open('a') as f:f.write(line.replace('(Documentation/', '(../Documentation/').replace('(Blood2/docs/', '(docs/'))
updates={
 root/'Documentation/Implementation/m2.md':'\nThe [GPU lifetime investigation](m2_gpu_lifetime.md) isolates the retained shutdown leak outside the game, with SDL-only and standalone Vulkan reproductions and fresh NVIDIA module attribution. Full-route gameplay assertions still complete, but the GPU sanitizer process fails; no suppression or completion claim is introduced.\n',
 root/'Documentation/Roadmap/03_milestones.md':'\nThe [GPU lifetime investigation](../Implementation/m2_gpu_lifetime.md) reproduces the graphics shutdown leak without the game, including a standalone Vulkan control. Fresh NVIDIA module attribution is complete; the failing GPU sanitizer result remains visible. Final M2 acceptance remains open under the unchanged gate.\n',
 project/'docs/contracts/readme.md':'\n- [Platform lifetime diagnostics](gpu_lifetime_contracts.md): isolated SDL controls, explicit sanitizer failures and NVIDIA module attribution.\n',
 project/'docs/implementation/readme.md':'\n- [GPU lifetime attribution](../../../Documentation/Implementation/m2_gpu_lifetime.md): SDL-only and standalone Vulkan controls for the retained NVIDIA LeakSanitizer failure.\n',
 project/'docs/implementation/sources/readme.md':'\n- [GPU lifetime diagnostic sources](m2_gpu_lifetime_sources.json): project teardown baseline, pinned SDL APIs and system Vulkan probe; no original-game import.\n'}
for path,text in updates.items():
 with path.open('a') as f:f.write(text)
ledger={'date':'2026-10-18','implementation_parent':subprocess.check_output(['git','rev-parse','HEAD'],cwd=project,text=True).strip(),'original_imports':[], 'changes':['Add SDL-only diagnostic lifetime executable and three optional display-dependent CTest cases.','Retain standalone system-Vulkan reproduction as evidence, not an engine dependency.'],'production_changes':False,'dependencies_changed':False,'references':[]}
for p,usage in [('src/engine/runtime.cpp','Match explicit GPU idle, release-window, device/window destroy and SDL_Quit ownership order.'),('build/sanitize/_deps/sdl3-src/include/SDL3/SDL_hints.h','Pinned SDL debug-only shutdown hint contract; not enabled in production.'),('build/sanitize/_deps/sdl3-src/src/core/linux/SDL_dbus.c','Pinned SDL connection cleanup and process-global shutdown policy.')]:
 ledger['references'].append({'path':p,'sha256':hashlib.sha256((project/p).read_bytes()).hexdigest(),'use':usage})
ledger['frozen_sources']={p:hashlib.sha256((project/p).read_bytes()).hexdigest() for p in ['tests/platform_lifetime_tests.cpp','tests/CMakeLists.txt']}
(project/'docs/implementation/sources/m2_gpu_lifetime_sources.json').write_text(json.dumps(ledger,indent=2)+'\n')
