# Original autonomous locomotion and Cultist attacks

> The [animation-clearance correction](ai_animation_clearance_contracts.md) supersedes
> earlier blocked-resize failures/CrawlUnder fallback claims: original SetAnimation
> continues a valid clip while physical bounds stay unchanged on collision refusal.
> The AI caches retained physical dimensions; the request remains inspectable.
> Invalid/unavailable services still fail; the replacement resize solver is unchanged.

This increment ports AI_Mgr obstruction/ledge queries, MC_Walk/Run/Fire_Walk/Fire_Run
and CultistAI close/far attack sequencing. Read [source selections](../implementation/sources/m2_ai_locomotion_sources.json),
[evidence](../../../Documentation/Implementation/m2_ai_locomotion.md),
[cache/path contracts](ai_navigation_contracts.md), [fire contracts](ai_fire_contracts.md),
[state contracts](ai_state_contracts.md) and [object lifetime](object_contracts.md).
No production NPC is registered and no complete AI_Update is provided. M2 stays open.

## Obstruction and ledges

CheckObstructed ignores its direction argument and unused total displacement, as in
source. It tests full 3D velocity magnitude strictly below 10% of requested speed,
and absolute cached vertical displacement <=0.01. Current/previous poses come from
CMovement; query calls do not sample or integrate the body.

CalculateObstruction casts nine rays in right/center/left, upper/middle/lower order.
Side offset is (dims.x+dims.z)/2; upper/lower offsets are ±(dims.y-1); forward length
is 2*(sqrt(dims.x²+dims.z²)+1). Solid-only rays have no filter. The first hit is
retained even if it is the actor itself: self is excluded from flags *after* the
query, without recasting. World polygons also set flags. Missing/invalid collision
returns -1, not zero; partial flags/rays remain diagnostics. No successful avoidance
may be inferred from an unavailable query.

NavigateObstacle returns to last state with no obstruction; search states restart
themselves. Upper+CRAWL has priority over lower+JUMP. An enabled upper branch with
other blocked rows consumes the source else-if chain without selecting recovery.
The lower branch has the analogous rule. Selecting CrawlUnder/JumpOver is supported;
executing those state bodies is still unavailable. FindPosAroundObj requires the
missing GetPointShade service and fails explicitly. The unused, unsafe original
CalculateTurn is not imported. No guessed recovery point is supplied.

IsLedge tests at most once per 0.05 game seconds; a throttled call returns false,
not the last sampled result. Its origin is cached position plus the **unnormalized**
direction times 1.25*sqrt(dims.x²+dims.z²); it casts down 2*dims.y. Any solid hit is
support, without an owner/character/glass filter. The inspector distinguishes last
sample, sampled-on-last-call and query failure. A missing service cannot become a
usable no-ledge result: only a completed hit/miss consumes the throttle interval.
Failed typed requests retry even in the same tick. This is an explicit safety
adaptation for errors the original boolean service could not represent. Clock
and dimensions are checked.

## Four movement commands

Ordinary walk/run select the current fire-type clip, falling back to melee without
a weapon. Moving fire falls back to pistol. Injured leg nodes 10/14 select their
original pistol/melee or rifle walk clips when armed. Initial nonfiring walk slows
any injured leg to one-third speed; a later grounded/gravity continuation restores
full speed. Airborne or nongravity continuations do not rewrite velocity.

Each command writes cached-forward velocity and enables model looping. Moving fire
also resets load/time starts to zero, stores weapon reload time and schedules 0.1s.
There is no extra position integration: existing generic physics integrates NPC
bodies once, while the player keeps its separate single integration.

Continuation checks obstruction first. A successful obstacle state change does not
skip the rest of the source callback. Current support is obtained through the
existing typed standing query; no fabricated world-object handle is needed. A real
blocked foot-anchored animation resize selects CrawlUnder; missing clips/models or
services fail instead. This resize/collision solver remains replacement engine code,
not verified retail physics.

Moving fire uses strict time > last-load+reload. Fire acceptance, including dry fire,
updates load time. Ordinary blocked/unarmed fire clears animation and increments
Metacmd. Playback completion independently stops horizontal velocity, clears animation
and increments it again; both increments may occur in one call. A fire_key may also
fire, independently of the timed shot. Missing required shot/playback services fail.
Checked overflow, reentrancy and pending-owner guards prevent continued work after
a destructive or nested callback; removal remains deferred. State/command diagnostics
are separate from generic physics and do not claim autonomous initialization.

## Cultist attack sequencing

The original phase 1 faces the cached target, phase 2 chooses a weapon, phase 3 fires
standing at <=25% (close) or <=75% (far) of seeing distance, while already firing
standing, or when vertical separation exceeds body height. Phase 4 approaches only
inside (25%,50%) for close, or above 75% for far. It faces the target, cancels that
Metacmd increment, then checks ledge before height/current-command guards. Injured
actors walk; otherwise close runs and far runs firing. Unsafe ledge/height selects
standing fire. Phase 5 calls the existing original decision function.

Distance is raw cached-position distance and overwrites SIGHT, as in source. Lost
targets select SearchVisual at matching height, otherwise ComputeState. Query/pose
failures propagate. Base AI_Mgr attacks remain unsupported; only Cultist overrides
these states. Full initialization/update, deactivation/visibility pings, target
retention, other navigation/recoil/death states, required FX and ExitTrigger still
block production NPC registration and the M2 gate.

## Inspection and verification boundary

The Skeleton inspector reads commands, velocity/vertical thresholds, all nine rays
and hit identities, ledge timing and Cultist distance/phase diagnostics. Removed
hits are labelled instead of dereferenced. It performs no simulation work.

The GPU diagnostic explicitly drives one original Cultist close-attack standing
cycle, then the original moving-fire command. Its isolated placement, explicit
initialization and scheduler are test fixtures, not authored AI_Update. Hidden
inspection and world reload use the same game clocks and deferred ownership. Real
model clip availability is checked without substituting missing civilian fire clips.

The [AI recovery/navigation continuation](ai_recovery_contracts.md) now executes
jump/crouch/crawl and seven destination/recovery/visual-search states. Earlier
blanket omissions for those bodies are superseded. Failed partial routes remain
unusable; shade recovery, other states and full production AI_Update remain open.

The [Gideon attack continuation](gideon_attack_contracts.md) supersedes the selected attack/dodge/recoil sequencing omissions and adds original Bold taunt. Production boss lifecycle and the M2 gate remain incomplete.
