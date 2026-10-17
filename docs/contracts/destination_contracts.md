# Read-only destination preparation

> Current continuation: [safe NEW_LEVEL](level_change_contracts.md) promotes a fully prepared
> destination with the supported player, or retains the source on failure. Earlier
> blanket transfer omissions below are historical; each standalone diagnostic API
> retains its documented scope. CityHub and full M2 acceptance remain incomplete.

This is a prerequisite for safe original NEW_LEVEL transfer, not an implementation
of that transfer. [Named keep-alive entry](player_keepalive_contracts.md) still
requires an explicitly loaded destination. The active world/player is never replaced
by inspection, including when all schemas pass. No new original class is registered.

## Parsing, preparation and validation

`parse_world_request` decodes the existing original CMSG_LOADWORLD field order and
normalizes a bounded single-player NEW_GAME or NEW_LEVEL path. It rejects malformed,
trailing, NUL-containing or traversing paths, unsupported modes and difficulty >3.
This is syntax validation only. `decode_world_request` remains the execution-policy
gate: NEW_LEVEL fails with the KeepAliveSave/KeepAliveLoad diagnostic. No caller
gets fresh-game entry as a substitute for saved player transfer.

`prepare_world` takes a normalized request and const mounted Resources, reads the
existing bounded DAT format, and builds a separate primary CollisionWorld. Both
outputs publish together only after successful parsing and collision preparation;
failure leaves the output empty. Fresh NEW_GAME loading uses this same helper before
teardown. Original creation/initial-update failures after fresh-game teardown retain
the existing inspection semantics; this does not add general rollback.

`Objects::validate_properties` is const and shares creation's existing inherited,
case-insensitive property schema checks: exact variant types, the accepted Euler
rotation override, finite values, per-field string limits and no embedded NUL.
It returns an explicit status/error without factories, callbacks, engine diagnostics
or object allocation. It does not validate dynamic resource requests, gameplay
meaning, difficulty filtering or model dimensions. Creation still reports failures
through the ordinary object diagnostics and runs original callbacks normally.

## Inspection lifetime

`GameSession::inspect_destination` rejects reentrant object callbacks without
replacing its previous report. Otherwise it publishes one owned last-attempt record:
source world/revision, normalized destination, bounded requested start name, model
and primary-collision counts, every source entry's class/name/position, and one of
schema-checked, preprocessor-only, unknown-class or invalid-properties. Read/parse
failure replaces old rows with a failed report, never stale successful data. These
reports own strings/values and no live handles; they survive unload and explicitly
show the old source revision alongside the current one.

A completed scan is not a ready destination. Source properties are checked without
running constructors, precreate, initial update, StartPoint triggers, NPC clocks,
movement or render resource loads. Class-specific resource failures, mover binding,
player clearance, start selection/fallback, filtering and scripted side effects
remain unverified. This keeps the current player, client, NPC state, resources,
world revision, audio and simulation clocks intact. The DAT/collision temporaries
are released after inspection; the report is not a reusable staged world or a
content-authenticity guarantee if external files change.

The player message pump still processes every original exit and capture/refusal,
then inspects only the final owned request once when the queue drains. A 256-event
queue therefore does not perform 256 world reads. Server start-name ownership keeps
its original copy-before-broadcast behavior, including failed broadcasts; the report
shows that current server value. Dead/invalid exits do not manufacture a request.
No later frame repeats a scan without a new request. All existing queue/round bounds
and deferred callback rules remain unchanged.

The Session tab offers **Inspect destination without leaving**, available with an
active player. Session and Objectives show the owned report, explicit unchecked
runtime work, source entries and failure details. Drawing never performs a scan.
Automatic blocked exits also produce the report with UI hidden or compiled out.

## Evidence and remaining gate

`world.destination_messages/schema/lifetime/private` covers malformed requests,
shared schema checks, structural and collision failures, output clearing, callback
refusal, no construction, queue coalescing, owned report lifetime and private repeated
SubwayA/CityHub scans. `gpu.objectives` checks the authored delayed exit request with
UI hidden and after reload, preserving the original player/world while inspecting
CityHub. Full existing creation, loading, exit, restore and rendering tests remain.

See [implementation evidence](../../../Documentation/Implementation/m2_destination.md)
and [source ledger](../implementation/sources/m2_destination_sources.json). Safe NEW_LEVEL still needs protection
against destination original initialization/start-trigger failures while preserving
the current world, and must resolve required classes/services. Reloading the old DAT
would lose current NPC/world state and does not count as rollback. M2 remains open.

The [destination initialization rehearsal](destination_trial_contracts.md) extends the structural scan with temporary original callbacks and copied-player named entry.
It isolates the current original globals, destroys the temporary session and resumes the unchanged source gameplay state.
Ordinary second-session binding remains forbidden; no automatic transfer or completed M2 acceptance is claimed.
