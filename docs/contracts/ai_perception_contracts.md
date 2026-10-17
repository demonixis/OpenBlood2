# Original AI perception and candidate lists

> The [smell-trail continuation](ai_smell_contracts.md) now supplies periodic player hints and the original SearchSmellTarget body. Its source guards do not automatically seed a tracking cursor. Earlier blanket smell omissions are historical; M2 remains incomplete.

> Extended by [AI state/action contracts](ai_state_contracts.md): selected original
> state decisions, transitions and stationary animation commands now execute.
> Full AI_Update and production NPCs remain unavailable; M2 is still open.

M2 remains incomplete. Selected original `AI_Mgr::FindObjectInRadius`,
`ComputeStimuli`, `ChooseOpportunityTarget`, faction membership and `ExitHint`
now execute. There is still no production CivilianAI/CultistAI/Gideon registration
or automatic AI update/state machine. These commands are prerequisites exercised
by labelled probes, including queries against the actual original player.
See [source selections](../implementation/sources/m2_ai_perception_sources.json),
[evidence](../../../Documentation/Implementation/m2_ai_perception.md),
[sight](ai_sight_contracts.md) and [target lifetime](ai_target_contracts.md).

## Candidate membership and query rules

The original lists are retained, with initialized links and idempotent insertion.
AI InsertMe joins one faction; the original player InitialUpdate joins **both**
factions, including before client association. Destructors unlink membership on
normal removal, failed entry and unload. A registered AI's faction is fixed;
re-insertion with a changed faction rejects. Lists own no objects. Pending members
remain linked until destruction but queries immediately skip their checked
identities. Hidden and inactive live members remain eligible, as in the original
list traversal; GetNextObject's active-only enumeration is not substituted.

The interface takes a typed candidate-list selector (Enemies/Smells/Exits/Ammo)
instead of using class inheritance to choose the list. This avoids registering
an incomplete AI_Mgr as a creatable world class. Enemy queries use Monster for a
Cabal observer when that list is nonempty; otherwise they use Cabal, preserving
the unusual original fallback. Players therefore participate for either faction.
The source's special BoneLeech/TheHandAI/Thief class requests select the same
faction lists without a specific-class flag; no corresponding NPC is imported.

ExitHint has its original no-update lifetime and prepends to its list. Ammo and
faction lists also prepend; SmellHint appends. Equal nonzero distances retain the
first link. No sort, dead filter, activity filter or arbitrary self exclusion is
added. The original zero-distance nearest sentinel remains in the specific-class
branch. Enumeration is bounded by the registered list count and the session's
65,536-object ceiling; corrupt lists fail explicitly.

Origin is the current animated head, falling back to the checked object position;
forward comes from current rotation in place of unported MoveObj cached vectors.
Distance and direction calculations accumulate in double precision to avoid float
overflow; original float rounding is not claimed bit-identical. Stored strengths
remain floats. The radius boundary is inclusive. FaceObject accepts the entire forward hemisphere,
including the exact side plane. Visible first rejects a player's original
PU_INVISIBLE activation bit, then calls the existing body/head CanSee query.
Sense and smell have no visibility/FOV/invisibility filter. IsPowerupActive uses
real zero-initialized activation storage, with checked indices; pickup acquisition,
duration, visual fade and reset/save behavior for those timed powers remain
unported. Tests set the bit directly; no invisible pickup is claimed implemented.

Specific preserves the original **reversed** IsKindOf(requested, candidateClass)
comparison, and skips direction computation; FaceObject then observes its source
zero dot product. AvoidTarget uses normalized observer-minus-cached-target, and
rejects only when both target and candidate dots are positive. It requires a live,
sampled target. [NPC ammo recovery](ai_ammo_contracts.md) now permits
Specific+AvoidTarget by skipping its unused uninitialized-forward computation:
the source's zero object dot prevents that branch from rejecting a candidate.
Other nonfinite or coincident directions still reject
instead of propagating invalid normalization. Nonpositive finite ranges return a
successful empty query. Unknown flags, invalid poses, corrupt lists and unavailable
collision are failures, not an absence of enemies. Last reports are read-only;
inspection does not reissue queries or advance animation.

## Stimuli and ownership

ComputeStimuli returns original 0/1/3/4 (none/sight/smell/sense), with **-1** added
for required-service/invalid-input failure. Future state code must handle -1
explicitly, never interpret it as a valid stimulus. Original derived ComputeState
uses -1 as its default request to recompute, then tests stimulus truthiness; future
ports must guard failed ComputeStimuli calls before that branch and must not forward
a failure as a state-selection request. Without a recent
attacker it clears strengths and the old target, queries sight/smell/sense in that
order, then assigns priority smell, sense, sight. Strength is 1-distance/range
using **body** position, although candidate selection uses the head. A zero-strength
higher-priority candidate can clear a positive lower-priority candidate. Health
is hit points / maximum hit points, without clamping; invalid/overflow ratios fail.

Smell returns a stimulus without making the hint a combat target. A selected dead
character produces no stimulus and no second-nearest search. Target assignment
uses the checked original link path; it does not sample target position or turn.
A later RefreshTargetPosition/face or AI_Update must do so. Partial failures retain
source operation order: normal acquisition may already have cleared its previous
target. No rollback fabricates successful perception.

The selected AI damage branch retains an attacker identity on accepted ordinary
damage, before subsequent trigger callbacks. GetLastDamager validates the stable
token on every access and clears pending/stale identities immediately. This replaces
the original damage aggregate's overlapping inter-object link, which could break
the AI target's same directed link. It owns nothing and cannot resurrect a reused
slot. Limb-hit selection/multipliers remain unported; this increment does not claim
the complete original AI damage pipeline.

A remembered character attacker bypasses range/FOV/faction/visibility checks and
preserves earlier stimulus strengths. Successful acquisition clears the memory.
A dead attacker returns none while retaining the old target and memory, as in source.
The source clears a non-character attacker but then still casts it to CBaseCharacter;
we clear it and return explicit failure instead. Self-target link acquisition also
rejects under existing contracts. No unsafe C-style character cast is introduced.

ChooseOpportunityTarget preserves its current-weapon guard, clears strengths/target
only after that guard, and runs sight-only selection. It retains the same strength,
boundary and dead-target rules. It does not fire, navigate or run a script.

## Inspection and remaining scope

Skeleton inspection shows faction counts, ranges, stimulus type/strengths, damage
priority, candidate/ray counts, selected checked identities, query origins and
failures, even with a missing model. Actual player lifecycle is connected to faction
membership. The existing GPU character probe now acquires the player through these
commands outside the UI, including while hidden and after reload; its diagnostic
name remains explicit. Source NPC entries remain unsupported.

SubwayA accounting remains 187 created, four transferred sounds, 372 editor-only,
17 unsupported and zero failed, plus seven SelfTriggers. Missing NPC states/actions,
AI_Update retention/death/script/navigation, auditory alerts, attacks/model-key
Fire, timed-power behavior, required client FX and ExitTrigger/session completion
still block M2. No retail timing/physics, other-platform or XR parity is claimed.
