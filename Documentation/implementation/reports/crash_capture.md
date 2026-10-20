# Crash stack/core capture — 2026-10-18

User request: extend persistent logs with call stacks and dumps to diagnose crashes.
Blood2 implementation commit: `17758ca`, based on logging commit `725d184`.
This is a Linux diagnostic backend behind a platform-independent interface. It does
not change original gameplay, SDL/dependency pins or rendering optimizations. M2
remains closed with M2-GPU-001, and M3 remains unaccepted.

## Implemented behavior

`CrashCapture` re-executes a clean helper before SDL initialization and connects it
through a private socket. The Linux fatal-signal handler records a small context
using a pre-opened file and fixed stack buffers. It does not allocate, unwind,
invoke a logger, call SDL or launch a process. The helper runs GDB separately to
capture all-thread stacks, registers and an ELF core, and copies the matching
executable. Files are written alongside the existing log session.

The application handles SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT, then restores the default
action and re-raises the original signal. An alternate main-thread stack supports
the tested stack-overflow case. GDB has a watchdog and size limit; collector death
kills its debugger child. Failed or truncated cores remain `.partial`, never a
successful `crash.core`. Idle helpers exit on parent EOF; normal teardown restores
signal/alternate-stack state and reaps its owned child before marking the logs finished.

Capture is enabled when Linux/GDB/process policy permits. `--no-crash-capture` opts
out. Existing debuggers, fatal-signal handlers, child reapers and sanitizer builds
retain control. Only the collector/descendants receive process-local ptrace permission;
no system sysctl, core-pattern or persistent user setting is changed. Other platforms
report unavailable rather than simulating successful dump support.

Release retains minimal source-line/function symbols without changing optimization
settings. Copied executables remain usable after rebuilding the original path.
See [the maintained guide](../../crash_capture.md) for artifact names,
GDB commands, limits and the distinction between the original fault frame and the
signal-handler frame at the top of a captured core.

## Validation

Linux x86-64, GCC 16.2.1, GDB 17.2; Clang 22.1.8 for sanitizer runs. Private content
remained external. GPU runs used the locally selected NVIDIA GeForce RTX 3060 Laptop
GPU and ordinary non-sanitized binaries. No M2 GPU sanitizer failure was rerun,
suppressed or reclassified.

| Run | Result | Evidence |
| --- | --- | --- |
| Debug crash/logging/config/CLI/object tests | 50/50 passed | [debug.log](evidence/crash-capture-2026-10-18/debug.log) |
| Inspector-free Release crash/logging/config/CLI tests | 36/36 passed | [release.log](evidence/crash-capture-2026-10-18/release.log) |
| ASan/UBSan with leak detection: logging/config and capture-disabled contract | 16/16 passed | [sanitizers.log](evidence/crash-capture-2026-10-18/sanitizers.log) |
| Debug GPU lifecycle/original menu traversal with capture armed | 2/2 passed | [debug-gpu.log](evidence/crash-capture-2026-10-18/debug-gpu.log) |
| Release GPU lifecycle/original menu traversal with capture armed | 2/2 passed | [release-gpu.log](evidence/crash-capture-2026-10-18/release-gpu.log) |
| Injected SIGABRT in the actual Release frontend after a rendered menu frame | Passed: original signal exit, readable core and main.cpp source frames | [app-probe.txt](evidence/crash-capture-2026-10-18/app-probe.txt) |

Each native suite includes fourteen crash cases: missing GDB, pre-existing fatal
handler, pre-existing child reaper, normal install/restore/reap, an attached debugger,
a genuine inaccessible-memory fault, worker-thread fault, abort, main-thread stack
overflow, dead helper, collector death after debugger launch, hung debugger timeout,
failed core output and the small-file-size limit. The tests verify that failures
never publish a successful dump and that collector death leaves no live debugger.

Successful fixture cores are reopened with the copied executable. GDB must recover
the nested `crash_leaf`/`crash_middle`/`crash_outer` frames, source lines and the
`0x13572468` memory sentinel, or the test fails. The overflow fixture verifies an
interrupted recursive frame. Fixtures disable only their own native core limit to
avoid duplicate system dumps; GDB-generated cores are still required.

Representative synthetic evidence: [signal context](evidence/crash-capture-2026-10-18/fixture-context.txt),
[collector result](evidence/crash-capture-2026-10-18/fixture-report.txt),
[reopened Debug core](evidence/crash-capture-2026-10-18/fixture-debug-core.txt),
[reopened Release core](evidence/crash-capture-2026-10-18/fixture-release-core.txt),
and [failure outcomes](evidence/crash-capture-2026-10-18/failure-outcomes.txt).

The application probe launches a new Release process with real SDL/GPU/menu assets,
waits for its frame capture, then sends SIGABRT to that owned PID. This is an injected
signal, not a newly discovered game bug. The retained core is reopened separately
and must contain application source locations. Report/text evidence is committed;
the binary, core and rendered commercial artwork stay in ignored local build output.
See the [application report](evidence/crash-capture-2026-10-18/app-report.txt),
[signal context](evidence/crash-capture-2026-10-18/app-context.txt) and
[reopened stacks](evidence/crash-capture-2026-10-18/app-core-stacks.txt).

The sanitizer run deliberately tests refusal to replace sanitizer handlers; it is
not sanitizer qualification of the active native crash handler. Native capture is
validated separately by the genuine faults and core-readback checks above. No full
campaign, full regression-suite or other-platform acceptance is implied.

## Commands and scope

Commands were run from `Blood2/` using the existing local private-content and GPU
test configuration. Final scoped builds and checks:

```sh
cmake --preset debug
cmake --build --preset debug --target blood2_app crash_capture_tests config_tests logging_tests object_contract_tests --parallel 4
ctest --preset debug -R '^(crash|logging|config|cli|objects)\.' --output-on-failure

cmake --preset release
cmake --build --preset release --target blood2_app crash_capture_tests config_tests logging_tests --parallel 4
ctest --preset release -R '^(crash|logging|config|cli)\.' --output-on-failure

cmake -S . -B build/sanitize
cmake --build build/sanitize --target crash_capture_tests logging_tests config_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(crash|logging|config)\.' -E '^logging.cli$' --output-on-failure

VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset debug -R '^gpu\.(lifecycle|menu_traversal)$' -V
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset release -R '^gpu\.(lifecycle|menu_traversal)$' -V
git diff --check
```

The application probe used `--frames 1000000 --capture <probe>/menu.png --log-dir
<probe>/sessions`, the same local ICD, dummy audio, an external `--data-dir`, and a
process-local zero native-core limit. It waited for the captured menu frame, sent
SIGABRT and required an actual SIGABRT exit, no `finished.txt`, `core_saved=1`, and
successful `gdb -nx -nh -batch ... crash-executable -c crash.core` readback. Absolute
local paths and dump size are recorded in the probe evidence. GDB init/auto-load
scripts and debuginfod were disabled during both collection and readback.

The build identifier is the existing configure-time revision/dirty record, not a
claim that an uncommitted build was clean. The copied executable is the exact binary
used by its core. Minimal Release symbols do not recreate optimized-out variables;
external libraries still need matching symbols for their own source locations.

Windows/macOS/mobile dump backends, worker-thread stack-exhaustion guarantees,
SIGKILL/power-loss capture, hang-triggered snapshots, crash recovery and automatic
upload are not implemented. Native system core handling remains independent.
