# Persistent diagnostic logging — 2026-10-18

User request: prepare file logging to investigate problems and crashes. Implementation
commit in the independent Blood2 repository: `725d184`, based on `53a5967`.
This is application/engine infrastructure; no original-game rules, dependency pins,
graphics optimization or milestone acceptance changed. M2 remains closed with
reservation M2-GPU-001, and M3 remains unaccepted.

## Implemented behavior

- Automatic session directories under SDL's OpenBlood2/Blood2 user directory, with
  optional `--log-dir` and `--log-level` overrides. Logging starts before content and
  platform initialization, records parse/config failures and outlives resource teardown.
- Existing console output is retained. A synchronous spdlog sink writes UTC timestamps,
  severity, process/thread IDs and forwards emitted SDL messages with category/priority.
  Every record is flushed; formatting remains outside signal handlers.
- Per-session rotation: 4 MiB per file, two backups, explicit 64 KiB record truncation.
  Startup prunes completed history to ten previous sessions. Interrupted/active sessions
  are preserved, so total storage is not globally bounded.
- Stable build metadata survives rotation. Successful close publishes `finished.txt`
  with the actual application exit code, including nonzero handled failures. Missing
  completion means running or interrupted/logging failure, not a proven crash.
- Independent concurrent launch files; console fallback on directory/open/write/rotation
  errors. No exceptions are used for expected file-operation failures.
- Normal frontend logs now include new-game/direct entry, committed level changes,
  preference-write failures and detailed simulation failures already available in the
  diagnostic frontend. Simulation, menu pause and resource ownership are unchanged.

See the maintained [logging guide](../../logging.md) for usage, file
locations, retention and limitations. No original-source import was needed.

## Validation

Linux, GCC 16.2.1 for Debug/Release and Clang 22.1.8 for ASan/UBSan. GPU smoke logs
identify **NVIDIA GeForce RTX 3060 Laptop GPU**, selected locally through the existing
Vulkan ICD environment override. Commercial content remained external.

| Run | Result | Evidence |
| --- | --- | --- |
| Debug logging/config/CLI/object/menu suite | 41/41 passed | [debug.log](evidence/logging-2026-10-18/debug.log) |
| Inspector-free Release logging/config/CLI suite | 22/22 passed | [release.log](evidence/logging-2026-10-18/release.log) |
| Clang ASan/UBSan, logging/config tests, leak detection enabled | 15/15 passed | [sanitizers.log](evidence/logging-2026-10-18/sanitizers.log) |
| Debug GPU lifecycle and original menu traversal | 2/2 passed | [debug-gpu.log](evidence/logging-2026-10-18/debug-gpu.log) |
| Release GPU lifecycle and original menu traversal | 2/2 passed | [release-gpu.log](evidence/logging-2026-10-18/release-gpu.log) |

The ten logging tests cover immediate file visibility, SDL forwarding/literal payloads,
UTF-8 paths, severity filtering, size rotation/truncation, failed initialization,
rotation failure, retained interrupted sessions, concurrent thread writes, `_Exit(23)`,
SIGABRT and application CLI/concurrent launches. Abrupt/signal cases run in child
processes: the final record survives and no completion marker is created. SIGABRT
tests disable core-file output; the application itself leaves native crash handling
untouched. These are synthetic termination checks, not a diagnosed game crash.

The initial runs had two failed assertions in `logging.basic` and `logging.level`:
the test expected doubled braces in a zero-argument spdlog message to be formatted.
That overload preserves literal text. The fixture was corrected to pass an explicit
payload through `"{}"`, then both suites passed. Raw initial failures are retained:
[Debug](evidence/logging-2026-10-18/initial-debug-failures.log),
[sanitizers](evidence/logging-2026-10-18/initial-sanitizer-failures.log).

Actual Release menu output was also checked on disk after exit:
[log](evidence/logging-2026-10-18/example-session.log),
[metadata](evidence/logging-2026-10-18/example-session-metadata.txt),
[completion](evidence/logging-2026-10-18/example-session-finished.txt).
The generated build identity records the configure-time base revision plus `dirty`,
as expected for validation before committing. The pinned SDL SHA is recorded separately
from SDL's runtime revision string, which in this checkout derives from the enclosing
repository and is not an authoritative dependency identifier.

Commands run from `Blood2/` (output redirected to the evidence logs above):

```sh
cmake --preset debug
cmake --build --preset debug --target blood2_app logging_tests config_tests object_contract_tests menu_tests --parallel 4
ctest --preset debug -R '^(logging|config|cli|objects|menus)\.' --output-on-failure

cmake --preset release
cmake --build --preset release --target blood2_app config_tests logging_tests --parallel 4
ctest --preset release -R '^(logging|config|cli)\.' --output-on-failure

cmake -S . -B build/sanitize
cmake --build build/sanitize --target logging_tests config_tests --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(logging|config)\.' -E '^logging.cli$' --output-on-failure

VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset debug -R '^gpu\.(lifecycle|menu_traversal)$' -V
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset release -R '^gpu\.(lifecycle|menu_traversal)$' -V
git diff --check
```

The existing sanitizer cache uses Clang with `BLOOD2_SANITIZERS=ON`; no GPU sanitizer
test was run or suppressed. An initial all-target Debug build was intentionally
interrupted and replaced by the targeted builds above; no full-suite build/test claim
is made. Existing enum/float warnings in imported game files remain unrelated.

## Limits

No stack trace, minidump, automatic upload or signal/exception handler is installed.
`fflush` preserves completed process writes but is not power-loss durability. Raw
stderr and sanitizer/native crash reports need separate capture. Retention may discard
older rotated records; incomplete sessions require manual cleanup. The Windows file-open
branch and other operating systems were not built or executed. This work does not
resolve M2-GPU-001, establish campaign parity or accept another milestone.
