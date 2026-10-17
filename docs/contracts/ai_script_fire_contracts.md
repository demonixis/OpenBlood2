# Original scripted opportunity fire

Selected AI_Mgr Script_Fire_Stand/Walk/Run now serve the OPPORTFIRE flag in
FOLLOWPATH and MOVETOOBJECT. See [source hashes](../implementation/sources/m2_ai_script_fire_sources.json),
[script movement](ai_follow_contracts.md), [interpreter](ai_script_contracts.md),
[perception](ai_perception_contracts.md), [weapons](ai_fire_contracts.md) and
[evidence](../../../Documentation/Implementation/m2_ai_script_fire.md). M2 remains open.

## Standing opportunity fire

FOLLOWPATH faces the next path point first. Outside the arrival threshold, bit 2
calls Script_Fire_Stand. A successful no-target decision permits ordinary scripted
walk/run; acquisition or weapon-service failure is distinct and aborts the command.
The typed method returns service success plus an `engaged` output rather than
conflating these cases in the original boolean result.

Each standing call selects an opportunity through the existing original faction,
visibility, facing and range rules. It refreshes the chosen position, faces it and
cancels that first Metacmd increment. Initial command 7 selects the fire-type clip,
disables looping and resets both start clocks to zero, preserving primary reload
time. No new scheduling interval, crawl recovery or velocity reset is added.
FOLLOWPATH skips its locomotion command while engaged, but retains residual velocity
exactly as the source does; it is not an immediate physical stop.

Later calls fire only when time is strictly greater than last-load plus reload.
The extra timed face increments Metacmd without cancellation. Accepted and dry
attempts update last-load. A blocked shot clears animating without advancing the
phase or consuming cadence. Completed playback independently clears animating,
with no phase increment. Missing required services still fail explicitly.

## Moving opportunity fire

MOVETOOBJECT phase 3 chooses Script_Fire_Walk or Script_Fire_Run when bit 2 is set,
then restores phase 2 as in source. These methods use commands 8/9 and looping
fire-type clips; without a weapon, the source fallback is TYPE_PISTOL. They always
use full walk/run speed, without the ordinary Script_Walk initial injury slowdown.
Velocity comes from CMovement's previous sampled forward vector, before any target
turn; the engine remains responsible for integrating it once.

Initial selection does not search or shoot. Subsequent calls move first, then
attempt acquisition only after the strict reload deadline. No target leaves the
clock unchanged. A target causes a face and Fire attempt; moving fire consumes
cadence even when that shot is blocked. Invalid/unavailable firing services fail
instead of being silently ignored as in the unchecked source. Completion stops
horizontal velocity (all axes when trapped), clears animating and does not advance
the phase.

The moving source does **not** refresh m_vTargetPos after ChooseOpportunityTarget.
The port preserves that cached-position aim and calls the checked MC_FacePos helper
without falsely marking the new target's pose as freshly sampled. This differs from
standing fire. MOVETOOBJECT also shares m_hTarget with opportunity acquisition:
reacquisition may replace the named movement target while the calculated route
remains unchanged. Do not introduce a separate hidden target or silently refresh
the path/aim to repair these source quirks.

## Safety and inspection

Existing object, model, animation-resize, time, speed, path and query bounds remain.
Missing clips still fail. The [animation-clearance correction](ai_animation_clearance_contracts.md)
continues valid clips after ordinary resize blockage, retaining the physical body. The existing motion
guard rejects recursive script-fire calls. After Fire, verify the owner and command
failure state and detect state changes or nested script failure before updating
cadence or playback. Destruction remains deferred. Missing weapon during ordinary
standing acquisition is a valid no-target decision; a shot-service failure is not.

The Skeleton inspector exposes the last script-fire mode, last-call engagement and
call count, with reload/last-attempt clocks and existing shot/blocked counts. These
are historical observations, not a claim that OPPORTFIRE is currently enabled.
Inspection never selects targets, fires or advances clocks.

Tests cover the distinct timing/phase/velocity branches, real damage and dry fire,
cached moving aim, script dispatch, path resumption, MOVETOOBJECT phase handling,
missing services, pending removal, reentry and state replacement during a shot.
Private diagnostics use a real Cultist ABC and original animation table in all
three modes through independent reloads. They are labelled diagnostic placements,
not proof of an authored opportunity-fire encounter or complete traversal. Existing
NPC death/gib/fire services and the actual M2 gate remain separate work.
