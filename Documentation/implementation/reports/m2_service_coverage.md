# M2 — auditable service coverage

Status: bounded adapter coverage and missing lifecycle instrumentation connected;
**M2 remains incomplete**.
Implementation commit: `23743a40b3db3bf1dcf597319895b00d6ae071d4`.

## What changed

The previous inspector listed only counters that happened to exist. That hid
unexercised services and several missing instrumentation points. It also omitted
restored-player entry and the player portion of explicit teardown from normal
tracing. The Services tab now shows a reviewed catalogue with two independent
facts: implementation status and observed attempts.

- 171 ServerDE/CSBase declarations, plus client connect/disconnect and model-string
  callback boundaries, are listed even when unexercised.
- Six missing method counters now cover GetPathManager, PlayClientSound,
  PublishBloodSpurt, PublishFleshGibs, RecordProjectile and ReportSkyFailure.
- Trace names use actual interface names, including sky methods. Historical
  descriptive suffixes no longer form accidental alternate service identities.
- Restored player entry, explicit player exit, message pumping and weapon commands
  preserve nested trace scopes on
  success and failure. Ordinary UI observation does not invoke or count services.
- Generic unavailable-request history survives unload. New-world loading resets
  it alongside the call counters; existing destination rejection behavior stays
  unchanged. Old shell/inspector text claiming player entry and all audio were
  still absent is replaced with the current bounded-session/acceptance status.

[Contracts](../../contracts/service_coverage_contracts.md) define the exact scope.
A positive counter is an attempt, including invalid input or a nested adapter
call. It is **not proof of success**. An unclassified name is shown explicitly.
The three generic unsupported APIs remain SaveObjects, RestoreObjects and
EndHMessageRead; supported player KeepAlive restoration does not replace them.
Subsystem-specific resource/mode failures retain their existing return values and
inspection. This adds no gameplay rules, SDK facade or successful stubs.

## Observed private session

The harness enters SubwayA, advances ten seconds with original updates, records its typed exit request before source destruction, performs
NEW_LEVEL to CityHub, advances ten seconds, leaves the player and tears down the
world. Each report contains every catalogue row. This is a stationary diagnostic,
not a walk through both levels or a combat completion test.

| Checkpoint | Reached boundaries | Unexercised boundaries | Generic unavailable requests | NPC runtime errors |
| --- | ---: | ---: | ---: | ---: |
| SubwayA player entry | 75 | 99 | 0 | 0 |
| SubwayA after ten seconds | 98 | 76 | 0 | 0 |
| SubwayA typed exit request | 100 | 74 | 0 | 0 |
| CityHub restored player entry | 85 | 89 | 0 | 0 |
| CityHub after ten seconds | 104 | 70 | 0 | 0 |
| CityHub after teardown | 108 | 66 | 0 | 0 |

Counters reset for CityHub; these are not cumulative across worlds. Restored
client association is now observable, and teardown includes DisconnectClient and
original child removal. A separate, labelled post-teardown probe deliberately
calls unsupported SaveObjects, then checks that a fresh load clears its history.
It is excluded from the accepted session checkpoints above.

The remaining 66 unexercised CityHub entries are not automatically missing
implementations, nor declared unnecessary. Acceptance must determine which are
required by the selected slice and exercise those paths. This catalogue does not
cover the entire historical SDK or all direct client-shell operations. Existing
subsystem tests remain evidence for contacts, animation keys, combat and resource
failure handling; this short trace does not replace them or certify campaign
traversal. M2's original exit criteria are unchanged.

## Verification

The selected suite has 41 headless checks: coverage, object ownership/messages,
world loading/rejection/NEW_LEVEL and player restoration, including six private
checks. Debug, inspector-free Release and Clang ASan/UBSan with leak detection
are checked. The new source-format guard compares every declared service with
the catalogue and checks one canonical counter inside each implementation.
Negative controls add a declaration without a catalogue entry and remove a real
counter in temporary source fixtures; both must fail. Unknown observations,
refused calls, nested scopes, failed restoration, pure snapshots, teardown and
reset are checked separately.

NVIDIA gpu.lifecycle renders the Services table and asserts inspection does not
mutate coverage. The captured Debug image was reviewed. NVIDIA gpu.level_change
retains its four rendered world replacements, real pixel checks and audio-output
ownership checks. Both tests run in Debug and Release; the latter deliberately
has no inspector. No shader, stack pin, reference repository or commercial asset
is changed. These Linux checks add no native macOS/mobile/VR validation.

See [manifest](evidence/m2-service-coverage.json) for exact commands, logs, the
complete private trace, source delta and integrity verification. An initial test
compilation used the wrong restoration-counter field name; it was corrected
without changing production behavior. No test expectation was relaxed to close M2.
