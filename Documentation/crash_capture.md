# Crash stacks and cores

On Linux, install GDB on `PATH`; startup reports whether crash capture is armed.
`--no-crash-capture` disables it. Debuggers, sanitizers or host attachment policies
may prevent arming. Other platforms have no native capture backend yet.

Keep the whole [log session](logging.md):

| File | Meaning |
| --- | --- |
| `crash-capture.txt` | Backend, helper and limits. |
| `crash-context.txt` | Original signal, thread, fault address and instruction pointer. |
| `crash-report.txt` | Capture outcome and `core_saved` status. |
| `crash-stacktrace.txt` | All-thread GDB stacks, registers and loaded libraries. |
| `crash.core` | Completed ELF core. |
| `crash.core.partial` | Failed/incomplete dump. |
| `crash-executable` | Matching executable copy with its available symbols. |

## Inspect

From the session directory:

```sh
gdb -nx -nh ./crash-executable ./crash.core
```

Start with `crash-report.txt`, then match the original thread/instruction pointer
from `crash-context.txt` to the stacks. Useful GDB commands: `info threads`,
`thread apply all bt full`, `thread N`, `frame N`, `info locals`.

The core is taken while the fatal handler waits; its top frame can be `poll`.
Optimized variables/frames and missing shared-library symbols may be unavailable.
The helper has a 15-second watchdog, a 2 GiB core limit and a 512 MiB executable
copy limit. Capture is best-effort: SIGKILL, hangs, power loss, faults outside the
armed lifetime and worker stack exhaustion may leave no usable dump.

Cores contain process memory, potentially including private/commercial data.
They stay local, are ignored by Git and need manual cleanup.
Implementation and detailed limits: [crash_capture.cpp](../src/engine/crash_capture.cpp)
and [validation report](implementation/reports/crash_capture.md).
