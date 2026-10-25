# Demo ventilation computer, paired doors and failure diagnostics

2026-10-25. User reports: a Demo_01 simulation stop at time 93.280262 with only
`SecScreen: Missing named world model` in the object log, and doors becoming
unusable after operating the computer that stops the blade/ventilation in the
blood channel. [Demo contract](../../contracts/demo_contracts.md),
[door contract](../../contracts/door_contracts.md#replacement-translation-solver).

## Findings

The mounted demo's `sawbladetrigger` toggles `sawbladedamage`, starts `c2keyframer`,
sends OFF to `sawblade1`, schedules `obtrig2`, starts `procconv` and triggers
`spawn101`. It contains no door-lock command. The camera keys activate `cam2`, link
it to `cam2spot1`, then deactivate it after four seconds. The checked sequence
releases the camera and leaves player movement available.

The nearby paired sliding leaves reproduce a separate mover defect. After opening
and closing, `2TempDoor2` remains Closing, about 0.105835 units from its closed
endpoint, blocked by already-closed `2TempDoor1`. The conservative brush sweep
reports a **nonpenetrating hit at fraction exactly 1**. The mover used to reject
every hit, so it could never finish this move. The private regression records 677
blocked retries on its first cycle. A synthetic pair of adjacent leaves reproduces
the same failure without commercial assets.

`MoveWorldModel` now accepts exact endpoint contact with another world model.
Initial penetration, contact before the endpoint and invalid queries remain
refused; crossing an entire leaf to a clear destination remains blocked. A move
away from a touching leaf is allowed by the existing sweep rules. This is a
correction to the replacement engine solver, with no new epsilon or changes to
authored data, original Door/Trigger algorithms or computer messages.

## Simulation log

`SecScreen` is the existing missing-BSP omission at Demo_01 entry, source row 674.
The frontend used to dump the whole retained object and startup trace on any later
failure. That made an unrelated old omission look like the cause of the crash.

`GameSession::step` now records failed stages and boundaries for new object/session
diagnostics. An object failure serial handles the 32-entry history ring even when
its size stays constant. The frontend prints these current-frame diagnostics;
entry omissions remain available in entry/inspection history. Failed physics
MoveObject calls include class, name, position and result; failed projectile flight
includes class, name and its error. Existing failure returns and simulation ordering
are unchanged; no error is suppressed to keep playing.

The reported crash is **not certified fixed**. Demo_01 passes 131.7 seconds with the
reported delta, and the computer sequence passes from a checked playable floor.
An exploratory unchecked teleport into the channel produced a later MoveMgr
failure after falling beyond the coordinate limit; it is not a reproduction of
the user's input history and was excluded from the regression. No out-of-world
teleport workaround is shipped.

## Verification

- Pre-fix failures are retained for [synthetic adjacent leaves](evidence/demo-doors-seam-before.txt)
  and the [authored computer/doors](evidence/demo-doors-computer-before.txt).
- Both formerly failing tests [pass after the change](evidence/demo-doors-targeted-after.txt).
  The synthetic case checks three cycles, movement away from contact, rejection
  of penetration and rejection of tunnelling, with unchanged positions on rejection.
- `demo.computer_doors` places the player on a collision-checked floor near the
  computer, delivers its original trigger, verifies the blade stops and the camera
  releases, then exercises the two original door sensors and both glass leaves for
  three complete open/close cycles. It uses the reported delta for the cycles.
- `demo.door_cycles` runs 13,000 frames at that delta before three cycles of the
  four authored unlocked rotating doors. The locked door remains untouched.
- `session_playback.policy` checks fresh failure attribution with a full diagnostic
  ring and retained startup trace, plus clearing on the next successful frame.

Final validation on macOS:

- Inspector-free Release application and affected test targets build successfully.
  The existing duplicate-spdlog linker warning remains.
- [Release regression](evidence/demo-doors-release.txt): **64/64 pass**, all headless,
  including 15 tests using external private assets. This covers object contracts,
  doors, triggers, USE/rotation, world paths, session startup, the demo and the
  SubwayA train route both normally and across a save/load.
- [ASan/UBSan synthetic regressions](evidence/demo-doors-sanitizers.txt): **4/4 pass**
  (door seam, pushing, riders, failure diagnostics).
- [ASan/UBSan authored computer sequence](evidence/demo-doors-computer-sanitizers.txt):
  direct `demo_tests computer_doors` exits 0; all four leaves finish all three cycles
  with zero blocked moves. This build does not register private-data CTest entries,
  so the same private test is invoked directly with an isolated `BLOOD2_USER_DIR`.
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`
  were used on macOS; this is not LeakSanitizer certification.

Build logs: [Release](evidence/demo-doors-build.txt),
[sanitizers](evidence/demo-doors-sanitize-build.txt). No GPU test, mobile runtime,
headset or simulator is claimed or run for this solver/logging correction.

These are labelled trigger deliveries and checked diagnostic placement, not a human
walkthrough or a replay of the original crash. The independently observed seam defect
is fixed; they do not prove every reported door or later demo interaction works.
M2 remains closed with M2-GPU-001; M3 acceptance is unchanged.
