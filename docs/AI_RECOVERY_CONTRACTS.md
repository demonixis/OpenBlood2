# Original AI recovery and navigation states

> The [smell-trail continuation](AI_SMELL_CONTRACTS.md) now supplies periodic player hints and the original SearchSmellTarget body. Its source guards do not automatically seed a tracking cursor. Earlier blanket smell omissions are historical; M2 remains incomplete.

Selected original AI_Mgr jump/crouch/crawl commands and seven navigation states
are integrated. Read [source selections](M2_AI_RECOVERY_SOURCES.json),
[evidence](../../Documentation/Implementation/M2_AI_RECOVERY.md),
[locomotion](AI_LOCOMOTION_CONTRACTS.md), [paths](AI_NAVIGATION_CONTRACTS.md),
[states](AI_STATE_CONTRACTS.md), [fire](AI_FIRE_CONTRACTS.md) and
[object lifetime](OBJECT_CONTRACTS.md). No production NPC is registered; M2 is open.

## Recovery commands

MC_Jump selects the current weapon's JUMP clip, or pistol when unarmed, and disables
looping. Initial Y velocity is cached up.y * jump speed (default 500); X/Z use the
cached forward components * run speed. The original epsilon Move mode copies this
mixed velocity instead of scaling it; trapped actors zero all components. Later
calls preserve current Y and rewrite X/Z. Actual support ends the command, stops
horizontal velocity and advances Metacmd; playback completion does not. Support
uses the existing typed standing result, including primary-world polygons without
inventing world-object handles. No gravity flag is required to end a jump. Existing
generic physics integrates once; this command does not move positions itself.

MC_Crouch selects CROUCH with the same pistol fallback, advances Metacmd and stores
command 4. It does not change velocity, looping or the animating flag. MC_Crawl and
MC_Fire_Crawl select CRAWL/FIRE_CRAWL, use walk speed without injury slowdown and
otherwise preserve the shared moving-command timing. On obstruction, both call
NavigateObstacle and then overwrite a successful recovery selection with WalkAround,
as in source. Required shade/query failure aborts instead of being overwritten.
Support rewrites speed only with FLAG_GRAVITY. Fire uses strict reload timing;
fire_key and timed shots remain independent, as do refusal/completion increments.

All four commands require live sampled movement, valid dimensions and real clips.
The original ignored SetAnimation failure; the port explicitly rejects missing
clips and blocked atomic resize instead of running with an incorrect body. A failed
crawl resize does not recursively select CrawlUnder. Animation resize is the existing
foot-anchored replacement, not verified retail SetObjectDims2. Counter bounds,
finite speeds, callback reentrancy and pending-owner checks are safety adaptations.

JumpOver phase 1 runs MC_Jump; phase 2 returns to last state. CrawlUnder phase 1
runs firing crawl only when last state is AttackClose; otherwise plain crawl. Phase
2 returns to last state. It does not synthesize a standing attempt or force looping
crawl animations to complete. Other phases retain the original no-op behavior.

## Destination and search sequencing

WalkToPos/RunToPos, WalkAroundObj/RunAroundObj and SearchVisualTarget share checked
phase plumbing while retaining their original differences. ToPos phase 1 calculates
a path then increments. Around/search face the destination first (incrementing via
MC_FacePos), then calculate. Ordinary blocked planning requests Idle. Required query,
invalid-data and bounded-work failures remain failures, not Idle success.

A failed planning attempt latches `navigation_plan_failed`. Phases 2/3 cannot follow
preserved old or partial points until phase 1 succeeds. This is needed because
original CalculatePath can retain its old route on preflight rejection or leave
partial points on later failure. The points remain inspectable. Other CMovement
queries do not erase this guard; it is independent of their last-operation outcome.

Phase 2 faces the next point. An empty path selects last state for ToPos, SearchSmell
for around/search. **SearchSmell execution remains unavailable.** Phase 3 measures
cached position against the next point with point Y flattened to cached Y. Arrival
is inclusive at `(sqrt(dims.x²+dims.z²)+0.1)/2`. It consumes one point only. ToPos and
around query a ledge only if another point remains; the last-point short circuit is
preserved. ToPos advances to phase 4 at final arrival/ledge and returns to last state
there; around requests ComputeState immediately. Arrival itself retains velocity
until a transition/action changes it. Missing ToPos phase-3 points reject rather
than reproduce the original null dereference. Around/search instead request a decision.

Around/search recompute actual stimuli before each phase. Negative typed perception
results abort. Non-smell positive stimuli use real two-side-ray clearance; around
sets the ray origin Y to destination Y, while search refreshes target position.
Clearance requests ComputeState(stimulus), ordinary obstruction continues, and
unavailable/invalid queries fail. Search without a retained target requests the
original default ComputeState recomputation. Search phase 3 checks a ledge twice
in source order; after a completed first query the second normally returns the
original throttled false. Initial ledges request a decision. Search runs, Walk
states walk, Run states run. No extra cache sampling, physics or time is introduced.

Navigation rejects nested state execution and stops after owner removal or nested
failure in virtual decisions. Last-state exclusions remain those of SetNewState;
ToPos is not an excluded transient state. The state wrappers do not supply missing
full AI_Update, initialization, target retention or activation policies.

## Inspection and validation boundary

Skeleton inspection reads last driven/current state, phase, destination, last
arrival distance/threshold, jump speed, remaining/current path point, failed-plan
guard, counts and errors. Hidden/inactive/missing resources remain represented;
inspection executes no simulation or queries. Existing script GPU diagnostics now
continue into original RunToPos with a real Cultist ABC, normal clocks and physics,
then return to Inactive. Hidden and unload/reentry assertions retain the previous
script checks and additionally require navigation completion and no failed plan.
This explicit isolated driver is not authored production AI_Update.

Synthetic tests use real collision, perception, model clocks, damage and deferred
ownership. A named decision-recording fixture observes requested ComputeState calls;
it does not replace ComputeStimuli or assert complete NPC decisions. Private tests
attempt all four command clips on six real NPC models without substituting missing
animations. GetPointShade/FindPosAroundObj, smell following, escape/recoil/death,
complete NPC initialization/update, required client FX and ExitTrigger remain open.

The [escape/guard continuation](AI_ESCAPE_CONTRACTS.md) supplies selected original
escape/hide/guard and beg bodies, plus checked non-owning tracking. Earlier blanket
omissions for those bodies are superseded; full NPC initialization/update remains open.
