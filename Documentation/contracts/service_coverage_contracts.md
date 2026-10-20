# Bounded service coverage

M2 requires evidence of the services reached by the selected session. The
[static catalogue](../../src/game/service_catalog.inc) covers every declaration in
[the port interface](../../src/game/legacy/server_de.h), plus client connect/disconnect
and the model-string callback boundary. It is **not** the entire historical SDK,
all client-shell operations, or a list of completed gameplay features.

## Two independent dimensions

- **Implemented (bounded)**: a concrete implementation exists, subject to its
  subsystem contracts. Invalid handles, missing resources, unsupported modes and
  exhausted limits may still fail. This label never means every input works.
- **Explicitly unsupported**: SaveObjects, RestoreObjects and EndHMessageRead
  still reject their requests. Supported player KeepAlive restoration is a
  separate typed path; it does not implement general world save files.
- **Unclassified**: an observed name absent from the catalogue remains visible.
  It is never silently counted as supported.
- **Observed calls** are attempts at a boundary, including refused calls and
  nested calls between implementations. Zero means unexercised in this trace,
  independently of the implementation classification. Neither a positive count
  nor the absence of a generic failure proves successful gameplay.

The catalogue contains 173 ServerDE/CSBase entries and three supporting boundaries.
Trace names are canonical method names; old descriptive suffixes and the sky
aliases are historical. Six previously missing service boundaries now trace directly:
GetPathManager, PlayClientSound, PublishBloodSpurt, PublishFleshGibs,
RecordProjectile and ReportSkyFailure. Restored player entry and explicit player
exit also gain trace scopes; these are scope corrections, not extra API entries.

## Scope and ownership

World loading resets counters after old-world teardown. Existing scopes cover
loading, fresh player entry, normal session updates and object teardown. Explicit
player exit, restored player entry, message pumping and weapon selection commands
now have nested, noncopyable scopes that
restore the previous tracing flag on every return. Ordinary inspection outside
these scopes does not add calls. Diagnostic direct service calls must explicitly
enable tracing if they are to appear; the catalogue does not fabricate activity.

Counters survive unload for inspection until the next successful world load.
Generic unavailable-request history now also survives unload; the current-world
rejection map retains its previous reset behavior for destination validation.
Sky and PolyGrid failures use the same historical recording path. Failure history
is collected even outside a trace scope, so an unavailable request can coexist
with zero traced calls. It is not an exhaustive catalogue of every possible
validation error; return values, object diagnostics and subsystem errors remain
necessary evidence. No errors are changed into successful return values.

The Services tab separates name, boundary, implementation and observed attempts,
including all unexercised entries. It also shows retained unavailable reasons.
Snapshots own their strings/counts, do not mutate the session and execute no
original callbacks. No source session pointers or counters are merged into a new
world during NEW_LEVEL. Capture a report before replacement to retain that world.

## Verification and remaining gate

`coverage.catalog` checks declaration/catalogue equality, unique canonical trace
names and a trace inside each current implementation. Its deliberately narrow
source-format scanner is a development check, not a production C++ parser; a
new declaration syntax requires updating that guard. The compiled catalogue is
ordinary immutable C++ data, with no runtime parser or new dependency.

`coverage.observations` distinguishes unsupported/unexercised, counts a failed
velocity request without calling it successful, exercises all three generic
unsupported APIs, retains unknown observations, checks pure snapshots, nested
scope restoration, failed restoration/weapon commands/message pumping and teardown history.

`coverage.private` loads SubwayA, enters the original player, advances ten seconds,
records the typed exit request under an explicit diagnostic scope, performs real NEW_LEVEL to CityHub, checks restored-client tracing, advances ten
seconds, then checks original player/object teardown and fresh-load reset. Reports
include all catalogue rows and current NPC runtime failures. This stationary,
limited-duration trace is not campaign traversal or evidence for every weapon,
trigger, AI action, client service or future resource request. M2 remains open
until the complete selected-slice acceptance review passes.

The [session-cost continuation](session_cost_contracts.md) removes redundant work while preserving these ownership, timing and inspection rules.

The [final M2 review](m2_acceptance_contracts.md) now consolidates this trace with continuous combat, startup, malformed-resource, callback and lifetime evidence. M2 is closed with the user-authorized M2-GPU-001 reservation; external GPU platform qualification remains open, and unexercised services are not certified by closure.
