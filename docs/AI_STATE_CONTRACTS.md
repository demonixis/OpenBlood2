# Original AI state decisions and stationary actions

> Extended by [CMovement contracts](AI_NAVIGATION_CONTRACTS.md): original cached
> transforms and obstacle-path calculation are available for explicitly initialized
> actors. Current-transform adaptations below remain; full AI following is still absent.

> Extended by [AI command/hearing contracts](AI_TRIGGER_CONTRACTS.md): player sound
> alerts, original hearing reactions and selected trigger commands now execute.
> Complete production NPCs and M2 remain open.

> Extended by [AI weapon-action contracts](AI_FIRE_CONTRACTS.md): supported weapon
> choice, aiming, clearance, standing fire and AI fire_key now execute. Earlier
> blanket weapon/fire omissions are historical; full NPCs and M2 remain open.

M2 remains incomplete. This ports selected AI_Mgr, CivilianAI and CultistAI
methods from [these source ranges](M2_AI_STATE_SOURCES.json). It builds on
[perception](AI_PERCEPTION_CONTRACTS.md), [targets](AI_TARGET_CONTRACTS.md),
[animation tables](CHARACTER_ANIMATION_CONTRACTS.md) and
[object lifetime](OBJECT_CONTRACTS.md). Execution evidence is
[here](../../Documentation/Implementation/M2_AI_STATES.md).

No production NPC class is registered. The two new subclass types contain state
rules, not complete original constructors, properties or initialization. Their
world entries remain unsupported. The diagnostic scheduler calls DispatchState
explicitly; it is **not AI_Update**. Automatic deactivation, target retention,
tracked objects, death processing, support/standing flags, navigation, fleeing,
combat, damage reactions and scripts remain absent. Selecting AttackFar or
EscapeRun succeeds as a decision; executing either returns an explicit failure.

## Transitions and decisions

State IDs retain original numeric values. StateStrToInt performs bounded ASCII
case folding without mutating its input, preserves prefix matching and the
source's four-character SEARCH comparison (SEAR). SPECIAL10 consequently matches
SPECIAL1. There is no whitespace trimming. Unknown/empty strings select Inactive;
embedded NUL and more than 31 bytes reject rather than overflow the original
32-byte array. SetNewState rejects undefined numeric IDs.

SetNewState remembers the old state except while leaving the six source transient
obstacle/search states. It resets Metacmd to 1, current command to 999 and the
animating flag even when the requested state equals the current state. A targeted
actor requesting Idle enters SearchVisual until time is **strictly greater** than
its saved deadline. Other requests set that deadline to current server time +120s.
The targeted bit is not automatically set by explicit target assignment: the
missing AI_Update owns that source operation. Inspection exposes it as stored
state, never proof that retention is implemented.

StopVelocity preserves Y and zeroes X/Z; the source Move(MATH_EPSILON) trapped
branch instead zeroes all axes. It changes velocity only, with no second physics
integration. The port validates owner/time and performs this fallible service
before committing state fields, so a failed transition retains the previous
state. It does not clear acceleration or animation playback: the next action
selects the clip, as in the source.

Cultist ComputeState preserves health thresholds (<.25 escape, <.5 far), the
inclusive sight <=.5 far branch, target-height test, last-Guard behavior and
FindAmmo/FindHealth restoration. EscapeRun's no-stimulus branch performs its first
Idle transition and then falls through: the subsequent decision observes the
changed last state and deadline. Current checked position replaces MoveObj's
cached position; target position remains the previously stored sample.

Civilian ComputeState returns to its authored state without a stimulus. With a
stimulus it preserves EscapeHide, otherwise latches scared and chooses EscapeRun
if already scared or health strength <1. Healthy calm actors return to their
authored state. These choices do not implement escape actions.

The optional -1 input means recompute, matching the original method signature.
Every recomputation checks the perception result before using it; a -1 failure
never becomes a positive stimulus or a second recomputation request. Explicit
values outside 0..4 and nonfinite decision inputs reject. Callers receiving
ComputeStimuli's failure must check it before passing a value to ComputeState.

## Animation commands and dimensions

SetAnimation validates the actual model/clip before mutation. The same cached
clip resets playback; a different clip reads original user dimensions, scales
them per axis and selects the clip. The source's fallback to cached dimensions
when the user-dimension query fails remains. The initial cached clip is 999 and
dimensions are (1,1,1). Animation ownership remains exclusive to the actor; callers
replacing a model must rebuild its shared table and reset the cached animation.

The old SetObjectDims2 is replaced explicitly by the existing **atomic,
foot-anchored ResizeObject**. This checks primary geometry/live solids and returns
Blocked/Invalid/Unavailable without changing shape, position or clip. It does not
recreate the old engine's partial resize/pushing behavior. A repeated cached clip
only resets and performs no clearance query. This replacement is shared with the
current player shape policy; retail NPC physics parity is not established.

MC_Idle draws 0..3 and MC_Talk draws 0..4 only when starting. Both use original
command ID MC_IDLE (0), so switching from one to the other during playback keeps
the current clip until completion. MC_Special uses ID 25 and indices 0..8;
while it is already active, a newly supplied index is ignored, as in the source.
A new invalid index or missing clip fails explicitly, without substituting clip
zero. All three start nonlooping playback. MS_PLAYDONE clears animating and advances
Metacmd once; counter overflow fails before changing those fields. Animation
advancement and model-key callbacks remain in the normal session clock. These
commands never advance time themselves or duplicate character audio/motion keys.

Base Idle retains smell-first selection and .001s stimulus / .1s idle scheduling.
Base Passive checks attacker memory without ordinary perception, uses .1s and
resets Metacmd at step 3. Their Cabal MC_BestWeapon branch is explicitly unavailable;
no successful inventory placeholder is inserted. Base ComputeState and subclass
special handlers without implementations also fail. DispatchState implements
Idle, Passive, the eight special slots, and Inactive; other actions fail instead
of silently invoking Idle. It preserves the source Metacmd<=0 repair. Direct
handlers require a live owner. A caller must stop/retry on false; only diagnostic
drivers currently arm these callbacks, and they disable scheduling on failure.

Civilian Idle skips perception while calm until its animation completes; a scared
civilian checks stimuli before the action. Special1 faces the stored target and
cancels the face command increment before MC_Talk. Special2..8 retain typing,
hand-warming, sleeping, study, right-hand subway, left-hand subway and fetal
animation indices. Special3 and Special6 draw their index on **every** step-1
update, even when MC_Special continues its current clip. The portable seeded RNG
preserves ranges/order, not the retail CRT sequence. Hide/beg movement, Special9,
recoil and remaining state actions are unavailable.

## Inspection and lifetime

The Skeleton panel shows state/request/last dispatch, authored state, both command
indices, idle deadline/targeted bit, trapped status, requested/cached clip,
selection, starts/finishes, cached dimensions/scale and recorded failures. This
works without a model and reads state only. Production worlds gain no test object.

The GPU character scenario adds a clearly labelled, hidden diagnostic female
civilian outside the playable geometry. An actual SPECIAL4 sleeping clip runs,
finishes and restarts with ImGui hidden; reload invalidates the old identity and
repeats the sequence. It uses unchanged external assets and the original state
methods, with a small explicit test scheduler. It does not claim an authored NPC,
AI_Update, source class initialization, target perception or navigation.

Synthetic tests cover deadline equality, last-state exclusions, trapped velocity,
source decision boundaries/fallthrough, shared idle/talk IDs, random draw ordering,
blocked/invalid shape changes, missing services, stale/pending ownership,
inactivity/reactivation, repeated unload and removal inside the first model key
(skipping the following key and destroying once). Private tests exercise all 240
clips across six NPC models and 19 available special slots, plus two 16-second
SubwayA diagnostic action cycles. Missing special slots remain failures.

SubwayA stays 187 created, 4 transferred sounds, 372 editor-only, 17 unsupported,
0 failed, plus 7 owned SelfTriggers. The unsupported entries are five CivilianAI,
ten CultistAI, Gideon and ExitTrigger. Required client FX and the complete session
also remain open. --start-game continues to return 6. Other platforms, XR and
retail behavior are not validated by these Linux tests.
