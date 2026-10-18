# Crash stacks and core dumps

Linux application launches automatically arm a separate collector when GDB is
available on `PATH`. It is re-executed before SDL initialization, so collecting
stacks and memory does not depend on the crashed game's allocator or logger locks.
Other platforms currently report the backend as unavailable; ordinary log files
continue working. This does not qualify Windows/macOS/mobile crash capture.

The startup log says whether capture is ready or why it is unavailable. Use
`--no-crash-capture` when native OS/debugger handling alone is desired. An attached
debugger, a non-dumpable process, an existing fatal-signal handler/child reaper or a sanitizer
build prevents installation; existing diagnostics remain in control. GDB is a
runtime diagnostic dependency, not a new linked engine or rendering dependency.

## Files to retain

The artifacts live in the same [log session](logging.md) as `blood2.log`:

| File | Meaning |
| --- | --- |
| `crash-capture.txt` | Armed backend, GDB path, helper PID and configured limits |
| `crash-context.txt` | Empty until a fatal signal; then signal, PID/thread, fault address and original instruction pointer (x86-64/AArch64) |
| `crash-report.txt` | Original signal/code/thread, debugger outcome, binary-copy outcome and `core_saved` status |
| `crash-stacktrace.txt` | GDB stacks for all threads, source locations where available, registers, loaded libraries and capture errors |
| `crash.core` | Successfully generated ELF core, for offline examination |
| `crash.core.partial` | Incomplete or failed capture; never treated as a successful dump |
| `crash-executable` | Copy of the running executable, preserving its matching symbols even after rebuilding |

Retain the whole directory together. Dumps contain process memory and may include
private data and commercial content; they remain local, are not uploaded, and are
excluded from Git by filename. Crash artifacts are not automatically pruned because
the process never writes `finished.txt`. Delete old crash directories manually after
investigation. Normal completed sessions retain their existing ten-session policy.

## Reading the result

Start with `crash-report.txt` and `crash-context.txt`, then locate the faulting Linux
thread ID in `crash-stacktrace.txt`. The interrupted frame below the signal-handler
frame identifies the original fault. The top frame can be the collector's `poll`:
the core is a snapshot taken while the fatal handler waits, not a kernel core taken
after its default signal action. The original instruction pointer is recorded
separately to make that distinction explicit.

From the session directory, inspect the retained core and executable:

```sh
gdb -nx -nh ./crash-executable ./crash.core
```

Useful debugger commands are `info threads`, `thread apply all bt full`, `thread N`,
`frame N`, `info registers` and `info locals`. Debug/dev builds retain fuller variable
information. Release now keeps minimal line/function debug information (`-g1` on
GCC/Clang) while preserving its optimization settings; optimized-out values, inlining
and eliminated frames cannot always be reconstructed. Stripping the binary removes
information. Shared-library symbols may require the corresponding system packages;
the collector does not download them automatically.

## Safety, failure behavior and limits

The Linux handler covers SIGSEGV, SIGBUS, SIGILL, SIGFPE and SIGABRT. It writes a
small fixed-buffer context record to a pre-opened descriptor, sends a fixed-size
socket message and waits for the collector. It does not unwind, allocate, format
with spdlog, enter SDL, spawn a process or invoke GDB. A separate 64 KiB alternate
stack protects main-thread stack-overflow handling. Worker-thread faults on usable
stacks are captured; worker stack exhaustion is not guaranteed without a per-thread
alternate stack. A second simultaneous fatal signal terminates rather than nesting
collection. SIGKILL, power loss and a frozen process without a fatal signal cannot
trigger this handler.
Crashes before arming or after disarming (including static initialization/destruction
outside the application lifetime) also remain the responsibility of native handling.

The helper invokes GDB with an argument vector, no shell, no init/auto-load scripts
and no debuginfod network requests. Only this helper and its descendants receive
the process-specific `PR_SET_PTRACER` authorization; system ptrace settings are not
changed. Host policy may still reject attachment. On completion or failure, the
faulting thread restores the default action and re-raises its original signal;
normal crash exit status and native core handling remain available. The OS may
therefore also store its own core, independently of this session directory.

The debugger has a 15-second watchdog and a 2 GiB per-file limit. The waiting handler
allows a further five seconds for coordination. A stuck debugger is killed/reaped;
collector death also kills its debugger child so it cannot remain attached. Parent
death closes the socket and ends an idle collector. Normal shutdown restores prior
signal/stack state and reaps the helper before log completion is published. The
executable copy is limited to 512 MiB. Disk stalls, kernel stops and catastrophic
process corruption can still defeat best-effort collection; this is not recovery.

Failed dumps remain `.partial`, with errors in the report/stack output, never a
successful `crash.core`. GDB honors the process's coredump filter and excluded
mappings, so GPU/device memory and deliberately excluded mappings may be absent.
Keeping the executable alone does not preserve every external shared library or
resource. A readable dump helps investigate the fault but does not by itself prove
the root cause of earlier memory corruption.

Implementation: [crash_capture.cpp](../src/engine/crash_capture.cpp).
Tests: `ctest --preset debug -R '^crash\.' --output-on-failure`; these deliberately
crash child processes and reopen their dumps to verify stacks and memory contents.
See [validation evidence](../../Documentation/Implementation/crash_capture.md).

Design references: [GDB core generation](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Core-File-Generation.html),
[Linux signal-safe operations](https://man7.org/linux/man-pages/man7/signal-safety.7.html),
and [Yama process-specific ptrace permission](https://cdn.kernel.org/doc/html/latest/admin-guide/LSM/Yama.html).
