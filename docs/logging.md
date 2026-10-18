# Diagnostic log files

Every application launch, including CLI validation and help, creates an isolated
session directory before content or SDL/GPU initialization. Existing spdlog output
still appears in the console and is also written to files. Emitted SDL messages
are forwarded with their category and severity. No game clock, renderer, original
rule or dependency revision is changed by logging.

## Finding and collecting logs

The console prints `Log session: ...` at the default `info` level. The default root
is `SDL_GetPrefPath("OpenBlood2", "Blood2") / "logs"`, beside menu preferences.
On Linux this is normally `$XDG_DATA_HOME/OpenBlood2/Blood2/logs`, or
`~/.local/share/OpenBlood2/Blood2/logs` when XDG_DATA_HOME is unset. SDL chooses the
platform-specific user directory on other systems; those platforms have not been
validated for this feature.

Use an explicit directory to collect a reproduction:

```sh
./build/debug/blood2 --log-dir ./diagnostics --log-level debug --data-dir "/path/to/Blood II"
```

`--log-dir` overrides only the logging location; relative paths use the working
directory. `--log-level` accepts `trace`, `debug`, `info` (default), `warn`, `error`.
It filters console and file output equally. SDL's own category filtering still
applies before forwarding; this option does not enable GPU validation by itself.

Send the whole relevant `session-*` directory, the steps to reproduce and whether
the window closed, froze or displayed an error. Paths in logs may contain the local
user name/content location. Files remain local; there is no automatic upload.

| File | Meaning |
| --- | --- |
| `session.txt` | Version, configure-time Git revision/dirty state, build type, compiler, platform and SDL revision; retained across rotation |
| `blood2.log` | Newest messages: UTC time with milliseconds, level, process and thread IDs |
| `blood2.1.log`, `blood2.2.log` | Older messages, newest backup first |
| `finished.txt` | Application returned after normal resource teardown, with its actual `exit_code` |

Exit code zero means the requested application operation returned successfully.
A nonzero code records a handled failure; it is not a successful session. Missing
`finished.txt` means the process is still running or did not record its finish
(crash, forced termination, power loss or logging failure). It is not by itself
proof of a crash. If the process froze, copy its files before stopping it.

## Retention and failures

Each message is written synchronously and `fflush`ed, including informational
messages, rather than waiting for shutdown or a background queue. Each file is
limited to 4 MiB with two backups (12 MiB per session); individual formatted
records over 64 KiB are explicitly truncated. Rotation discards the oldest records.
Avoid per-frame logging on hot paths; use bounded, event-based diagnostics.

At startup, keep the ten newest **previous completed** sessions, plus the current
run. Only known files in completed `session-*` directories are removed, with no
recursive deletion. Incomplete sessions are deliberately retained and must be
removed manually after investigation; total disk usage is therefore not globally
bounded. Concurrent launches create directories atomically and never share files.

A directory/open/write/rotation failure is reported to the console. The application
continues with console logging; a failed file sink is disabled, without retrying on
every message. Failed file output never produces a successful completion marker.
The logger outlives application resource teardown and restores the previous SDL
callback and default logger when destroyed.

## Crash-analysis boundary

These logs preserve the last completed writes, not a stack trace or memory dump.
No signal handler, exception handler or crash-recovery logic is installed. This
avoids calling allocators, SDL, mutexes or spdlog from a fatal signal handler and
leaves operating-system crash dumps, debuggers and sanitizers in control.

`fflush` drains process buffers to the operating system; it is not `fsync` and does
not guarantee survival of power loss, storage failure, or a crash during a write.
Raw stderr, third-party output bypassing SDL/spdlog, sanitizer reports and native
crash backtraces are not redirected into `blood2.log`. Capture them separately when
needed, for example by adding `2>diagnostic-stderr.txt` to the launch command, or
by running under a debugger. Native dump/backtrace collection remains separate work.

The build identity is captured at CMake configure time; reconfigure after changing
revisions if an exact reproduction label is needed. This is new engine diagnostic
code, with no original-game import. M2 remains closed with M2-GPU-001; these logs
neither fix that platform issue nor accept M3.

Implementation: [logging.h](../src/engine/logging.h),
[logging.cpp](../src/engine/logging.cpp), [application entry](../src/app/main.cpp).
Tests: `ctest --preset debug -R '^(logging|config|cli)\.' --output-on-failure`.
Recorded results: [logging validation](../../Documentation/Implementation/logging.md).
