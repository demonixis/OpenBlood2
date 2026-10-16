# Original AI escape, hiding and guard states

Later [production NPC lifecycle contracts](AI_RUNTIME_CONTRACTS.md) supersede
this snapshot's absence of Cultist/Civilian initialization, AI_Update and activation.
The existing action/service limitations remain unless explicitly superseded.

Selected CultistAI/CivilianAI Escape_RunAway and Escape_Hide, AI_Mgr GuardLocation
and MC_Taunt_Beg execute through existing typed services. M2 remains incomplete;
these classes are not registered as production NPCs. See [source selections](M2_AI_ESCAPE_SOURCES.json),
[evidence](../../Documentation/Implementation/M2_AI_ESCAPE.md), [navigation](AI_RECOVERY_CONTRACTS.md),
[perception](AI_PERCEPTION_CONTRACTS.md), [targets](AI_TARGET_CONTRACTS.md),
[hearing](AI_TRIGGER_CONTRACTS.md) and [object lifetime](OBJECT_CONTRACTS.md).

## Escape selection and phases

Cultists select visible ExitHints with AvoidTarget and recompute actual stimuli
before every phase after phase 1. Any positive stimulus requests ComputeState;
required perception failures abort. Civilians select visible hints without that
avoidance filter and do not recompute perception mid-route. Cultists use cached
AI dimensions; civilians query current dimensions. Both arrival thresholds are
inclusive at `(sqrt(x*x+z*z)+0.1)/2`.

No matching hint sends Cultists to AttackFar and civilians to EscapeHide. After
assignment, both explicitly sample the checked tracked position. Civilians first
compare full 3D distance against the arrival threshold, then face the hint and
compare the hint forward vector to the **preceding movement-cache forward**.
The cache is not refreshed after facing. A nonpositive dot requests Hide. Both
classes plan a path to an accepted hint. An ordinary blocked plan requests Hide;
missing queries or invalid data fail. The existing failed-plan latch prevents
following old/partial points until successful phase-1 replanning.

Phase 2 faces the next point; an empty path sends Cultists to Hide and civilians
back to EscapeRun. Phase 3 flattens the point Y to cached position Y. Arrival consumes
one point, faces stored target position on the last point, or returns to phase 2.
Otherwise MC_Run executes. There is no escape ledge query. Phase 4 requests the
original class decision. A missing phase-3 point fails rather than reproducing the
source null dereference. Arrival retains velocity until another action changes it.

ExitHint remains an ordinary original marker with no newly invented collision
flags. CanSee still requires hitting the selected target through the existing
solid-or-ray-hit query. A non-ray-hit marker is not made visible just because an
escape state needs it. Synthetic/GPU scenarios explicitly give their diagnostic
hints FLAG_RAYHIT and dimensions; this does not change authored world objects or
claim to resolve retail marker visibility behavior.

## Hide, beg and guard

Both Hide implementations request ComputeState when there is no combat target.
Otherwise they face its stored position, cancel the face command's Metacmd
increment, run MC_Taunt_Beg in phase 1 and request ComputeState in phase 2. A
pending target remains a checked failure until its ordinary link callback clears
it; no dangling object is dereferenced.

MC_Taunt_Beg selects TAUNT[6], disables looping and moves along cached right by
m_fRollSpeed. The selected source defaults are AI_Mgr 2, Cultist 105 and Civilian
3.5. These defaults do not implement the rest of their constructors/initialization.
The source's lateral movement is intentional preservation, not replaced with a
stationary pose. Playback completion clears animating and increments Metacmd but
**does not stop velocity**. Missing clips, failed atomic resize, invalid speed or
counter bounds reject; no fallback animation is substituted. Existing generic
physics and model clocks run once, independently of inspection.

GuardLocation recomputes actual stimuli on each call. Smell requests SearchSmell,
whose execution is still unavailable; other positive stimuli request ComputeState.
Phase 1 uses full 3D distance to guard position, skipping to idle phase 4 when near.
Otherwise it faces/plans; ordinary blockage requests Idle. The guard point must be
supplied by a caller: source Init's automatic spawn-position capture is not yet
integrated. Phase 2 faces a point, or requests EscapeRun when empty. Phase 3 checks
empty/ledge before distance, flattens the point Y, then performs the second source
ledge check. Its throttle normally makes that second result false. Arrival consumes
one point and eventually faces stored target position; otherwise MC_Run executes.
Phase 4 runs MC_Idle; phase 5 requests ComputeState. Failed plans remain latched.

## Tracking and lifetime

The private track identity is non-owning. Original escape assignment creates no
engine link, so SetTrackedObject does not manufacture one or share ownership of
the combat target's unique directed link. Stable checked identities reject self,
pending, stale and foreign objects. Queries immediately exclude pending objects.
RefreshTrackedPosition clears an invalid identity, marks the sample unavailable
and retains historical coordinates. Reused object slots cannot revive it.

If a legitimate existing link emits MID_LINKBROKEN for the same identity, the
original track branch clears it independently of the combat target branch. Clearing
one combat link while continuing to track its old object does not invalidate a live
track. Source gun-impact hearing also writes m_vTrackObjPos; that write now marks
the object-position sample unavailable until explicitly refreshed. No automatic
AI_Update refresh loop, smell-link policy or target retention is invented.

State/command guards reject nested calls and detect removal or nested failure
across virtual decisions. All removals remain deferred. Inspection reads recorded
positions, checked identity, sample flag, guard point, hint dot, roll speed, state,
phase, route and failure counters without executing actions or position queries.

## Verification boundary

Ten synthetic cases exercise both original classes, real queries/path planning,
normal motion/model clocks, current-versus-cached dimensions, facing-cache order,
source fallback states, guard/ledge sequencing, failures and deferred lifetime.
Decision-recording overrides are explicitly test fixtures; actual ComputeStimuli
and original class decisions remain covered separately.

The private catalog attempts TAUNT[6] on six real NPC assets: five resolve and
execute; Gideon has no clip and explicitly fails. The existing GPU character test
adds a labelled female-civilian escape/beg driver with a real ABC and diagnostic
ray-hit hint. It schedules only until Beg starts; clocks/physics continue normally.
Hidden inspection and unload/reentry require arrival, beg, nonzero lateral velocity,
no state/navigation failures and invalidation of the old identity. Its explicitly
assigned target, position and speed are not authored production AI_Update.

Shade recovery, smell following, assist/pickup/recoil/death states, full production
NPC initialization/update/activation, required client FX and ExitTrigger/session
services remain open. SubwayA still has 17 unsupported source entries. Other
platforms, XR and retail gameplay/physics parity remain unverified.
