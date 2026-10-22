# Diagnostic logs

```sh
./build/dev/blood2 --log-dir ./diagnostics --log-level debug --data-dir "/path/to/Blood II"
```

Every launch prints its `Log session:` directory. Default: SDL's
`OpenBlood2/Blood2/logs`, normally `~/.local/share/OpenBlood2/Blood2/logs` on Linux
(or `$XDG_DATA_HOME/OpenBlood2/Blood2/logs`), or `$BLOOD2_USER_DIR/logs` when that variable
is set ([user directory](getting_started.md#controls)). Keep the whole session plus reproduction steps.
Levels: `trace`, `debug`, `info` (default), `warn`, `error`.

| File | Meaning |
| --- | --- |
| `session.txt` | Configure-time Git/build/compiler/platform/SDL identity. |
| `blood2.log` | Latest messages, with UTC time, level, process/thread IDs. |
| `blood2.1.log`, `blood2.2.log` | Older rotated messages. |
| `finished.txt` | Normal teardown finished, with the actual exit code. |

A missing finish marker means running, interrupted or failed logging; it alone
does not prove a crash. Nonzero exits remain failures. Reconfigure after changing
revisions when an exact build identity is needed.

Each log is 4 MiB with two backups; records over 64 KiB are truncated. Startup
retains ten previous completed sessions plus the current run. Incomplete sessions
need manual cleanup. File failures fall back to console output and are reported.
Writes are flushed, without a power-loss guarantee.

Linux can collect [crash stacks and cores](crash_capture.md) with GDB on `PATH`.
Raw stderr and sanitizer output need separate capture, for example
`2>diagnostic-stderr.txt`. Files remain local; logs can contain local paths.

Implementation: [logging.cpp](../src/engine/logging.cpp).
Validation: [report](implementation/reports/logging.md).
