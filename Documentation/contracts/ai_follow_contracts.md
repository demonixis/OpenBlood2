# Original scripted locomotion and route following

> The [animation-clearance correction](ai_animation_clearance_contracts.md) supersedes
> earlier blocked-resize failures/CrawlUnder fallback claims: original SetAnimation
> continues a valid clip while physical bounds stay unchanged on collision refusal.
> The AI caches retained physical dimensions; the request remains inspectable.
> Invalid/unavailable services still fail; the replacement resize solver is unchanged.

Selected AI_Mgr helpers now connect original velocity, Script_Walk/Script_Run,
SETMOVEMENT, FOLLOWPATH and MOVETOOBJECT rules to existing typed services. See
[source selections](../implementation/sources/m2_ai_follow_sources.json), [navigation](ai_navigation_contracts.md),
[state actions](ai_state_contracts.md), [targets](ai_target_contracts.md),
[path snapshots](path_manager_contracts.md) and
[evidence](../implementation/reports/m2_ai_follow.md).
These are explicit helper calls, not the full SCRIPT interpreter, AI_Update or
production NPC registration. The M2 gate remains open.

## Velocity and animation

Move writes velocity and never integrates position. It preserves the exact
1e-11 speed sentinel (copy input velocity), unnormalized direction multiplication,
negative speeds and trapped-zero velocity. The ordinary physics step integrates
once; local player MoveMgr retains its existing separate ownership.

Script_Walk/Run choose the current weapon fire-type animation, or melee without a
weapon. Injured left/right leg indices 10/14 select the pistol/melee or rifle walk
variant only when a weapon exists. Any nonzero injury divides the initial walk
speed by three, even without a weapon; subsequent walk calls use full speed.
Running is unaffected. Both variants loop, use the cached CMovement forward basis
and restart on a different command. A completed playback stops horizontal velocity (all axes if trapped) and clears
animating without advancing Metacmd. They do not choose crawl recovery.

Current facing changes do not immediately refresh the movement aggregate: it
samples after the actor's own MID_UPDATE handler and before physics. The initial
velocity after a turn can therefore use the preceding basis. This source ordering
is intentional. Existing MC_FacePos still uses its documented current-transform
adaptation. Animation resize retains the existing foot-anchored replacement solver.
The script source ignored SetAnimation failure; this port returns a failure instead
of pretending an unavailable clip or blocked resize succeeded.

## FOLLOWPATH

SETMOVEMENT accepts exact case-insensitive WALK/RUN and ignores other text without
trimming. FOLLOWPATH owns PathMgr's reverse-enumeration snapshot; later clearing
the manager or removing original PathPoints cannot invalidate its text/positions.
Source identities are inspection-only. Each update faces the first point, replaces
its Y with cached actor Y, and uses the inclusive `distance <= 2 * dt * speed`
arrival test. A trapped actor consumes/actions an arrived point without snapping.
Otherwise arrival requests a checked collision move. DE_OK can mean a clipped
move; source actions still run and the point is consumed. At most one point is
consumed per update. Completion occurs on the following empty-route update.

An optional action message with no target is delivered to self. A present empty
target means no delivery. Named targets use the service's prepended order; identities
are copied before callbacks and pending recipients are skipped. Exact case-insensitive
REMOVE removes named recipients, but an untargeted REMOVE is an ordinary self-trigger.
Deliveries count successful transport, not recipient acceptance or a gameplay effect.
Strings/messages/lists have explicit ownership and are released on failures.

## MOVETOOBJECT

Selection takes the first named result (newest matching live object in the current
service), creates the existing non-owning target link and starts Metacmd 1. No match
completes without changing the prior target link. The done guard prevents following
that old target. Deferred MID_LINKBROKEN clears a removed target and the next update
completes; a target already pending before that callback is an explicit failure.

The four phases calculate a CMovement path, face its next point, move/consume within
the inclusive arrival threshold, then snap to the current target position and
complete. The route is not automatically recalculated if the target moves; the
final snap uses its fresh position with cached actor Y. Failed path calculation is
never followed, even if the original algorithm left partial points. Trapped actors
skip the final snap. Neither command stops residual velocity on arrival/completion;
the enclosing script/state must select its next action. The diagnostic scheduler
stops scheduling on completion but does not invent a velocity reset.

## Safety, inspection and limits

An initialized, valid CMovement sample and live owner are required. Finite speeds,
positions and frame deltas are checked; frame delta is limited to 0..0.5 seconds,
speeds to 0..10 million for script modes (Move permits signed speed). Bounded names
and messages reject embedded NUL. Failed commands latch until an explicit new
selection, preventing replay of actions after partial callback execution. Direct
locomotion/velocity calls may be retried but do not clear that command failure latch.
Reentrant selection/update/locomotion is rejected while an update is active; callback
removal remains deferred and owner validity is checked before continuing. Zero-distance
facing preserves the heading and source Metacmd increment instead of normalizing zero.
These are documented safety adaptations, not claims of binary or retail physics parity.

The Skeleton inspector shows command/mode/outcome, name, Metacmd, speed/injury,
destination, distance/threshold, requested velocity, updates/failures, arrivals,
trigger deliveries/removals and owned remaining points/actions. It reports live or
removed historical identities without dereferencing stale handles. Hidden, inactive
and missing-model objects remain selectable. Inspection runs no commands or queries.

The [opportunity-fire continuation](ai_script_fire_contracts.md) now connects
standing fire in FOLLOWPATH and moving fire in MOVETOOBJECT, preserving their
different aim, cadence and phase rules. Earlier OPPORTFIRE refusals are superseded.
MC_Walk/MC_Run obstacle recovery, shade queries, full SCRIPT parsing/dispatch,
AI initialization/update/deactivation, production CivilianAI/CultistAI/Gideon,
death, required client FX and ExitTrigger remain open. Existing authored counts and
startup refusal are unchanged. Diagnostic actors and targets are labelled and do not
turn missing world classes into successful creations.

The [script-state continuation](ai_script_contracts.md) supersedes the interpreter
omission above for explicit preparation/state dispatch. Production SCRIPT trigger
entry and the complete forced AI_Update remain unavailable.
