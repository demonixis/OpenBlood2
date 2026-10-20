# NPC animation under blocked clearance

The original `AI_Mgr::SetAnimation` (ObjectDLL/AI_Mgr.cpp:1950–1977) ignores the
return from `SetObjectDims2`, selects the requested animation, copies the dimensions returned through
the mutable vector into `m_vDims`, and returns true. The previous port instead propagated
an ordinary blocked resize into command failure, sometimes selecting CrawlUnder
and then permanently suspending AI_Update when that clip also could not resize.

The port now continues the valid animation on `ResizeResult::Blocked`, preserving
that source control flow. The engine's
existing atomic, foot-anchored resize still refuses the physical change: position
and collision dimensions remain unchanged. No clipping body or successful resize
is invented. Later different clips attempt normal clearance; resetting the same
cached clip only resets playback and does not retry resizing, as in the source.

Invalid dimensions, missing models/current playback, unavailable collision and
stale/pending owners remain explicit failures. Invalid requested ordinals follow
the source no-op rule described below. This correction only distinguishes a normal
collision result from a failed service. Existing callback/removal guards, original
animation clocks/keys, fire cadence, damage and activity rules are unchanged.
Initialization uses the same source method. The physical resize solver still does
not implement retail partial resizing/pushing; exact retail collision parity is
unverified. The available later LithTech CLTPhysicsServer::SetObjectDims and
ChangeObjectDimensions confirm that a refused resize can write dimensions back
through the input vector. That implementation returns an attempted partial size
while restoring the old physical body; it is corroborating evidence for an in/out
argument, not a claim of identical 1998 engine behavior.

Our atomic solver has no partial size. On Blocked, SetAnimation explicitly queries
and caches its retained actual body dimensions. This replacement policy keeps AI
clearance and subsequent corpse placement consistent with the collision body. It
never substitutes the impossible requested extent as a valid physical source box.
The requested extent remains separately available for inspection.

`animation_blocked` describes the most recent SetAnimation call; same-clip reset
or a rejected clip clears it. `animation_resize_blocks` counts accepted animations
whose requested body resize was blocked. The Skeletons inspector shows both,
last requested half extents, cached AI half extents and actual physical half extents, including hidden or
inactive actors. Reading these values does not change simulation. Counters expire
with the actor and are not save-state inputs.

Synthetic tests cover ceiling blockage, actual body retention, continuing playback,
state/phase progression, hidden updates, later successful resizing, same-clip reset,
invalid/missing services, pending removal, standing fire, walking, script running
and crawling. An ordinary-death probe also checks transfer/removal after a blocked
animation. The private regression uses authored SubwayA actors/geometry after
the real intro, then two labelled player relocations with damage disabled. It
is a reproducible encounter diagnostic, not a complete playthrough or proof that
every idle NPC report is fixed. See [evidence](../implementation/reports/m2_ai_animation_clearance.md)
and [source ledger](../implementation/sources/m2_ai_animation_clearance_sources.json). M2 remains incomplete.

## Invalid requested animation ordinals

The exhaustive level audit reproduced CivilianAI0 in Slaughter House requesting
ordinal 17 on labtech.abc, whose 17 clips are numbered 0–16. Original
AI_Mgr::SetAnimation does not test the engine animation result and stores the
requested ordinal. Later LithTech animtracker.cpp trk_SetCurAnim returns before
changing playback for an out-of-range index; ILTModel::SetCurAnim returns
LT_NOTFOUND, ignored by the server's void SetModelAnimation wrapper.

The port now keeps a verified valid current clip and its clock for the first
invalid request, attempts the source retained dimensions, and stores the requested
ordinal. Repeating that same ordinal resets actual playback, like the original
same-index branch. No clip-zero substitution or manufactured animation is used.
Actual missing models or invalid current playback still fail. The Skeletons
inspector exposes animation_clip_retained and retained_clip_requests separately
from blocked resizing. This supersedes the earlier blanket missing-requested-clip
refusal; it preserves the source's shared Civilian/LabTech table behavior.

Synthetic state, locomotion, roll, firing and script tests cover continuing the
actual clip; state tests distinguish unchanged time on the first request from a
reset on repetition and still reject missing model services. Private catalog
tests retain their availability counts while checking the no-op behavior.
Slaughter House passes the three-difficulty entry/simulation/recheck audit; this
does not certify all scripted paths or claim that the missing clip is present.

## Unchanged dimensions from a contact callback

End's authored Trigger sends CalebAI0 `Script 1;PLAYANIMATION walk_nogun;`
synchronously during player movement. The clip exists (ordinal 2), and its scaled
user dimensions exactly equal Caleb's physical dimensions. Rejecting every
ResizeObject call while `moving_` is set incorrectly suspended this script.

After live-owner/type, finite positive dimensions and collision availability
validation, exactly equal dimensions now succeed without mutation or a collision
sweep. Later LithTech ChangeObjectDimensions likewise checks for an unchanged
shape before its collision work; this implementation deliberately uses exact
component equality, with no tolerance-based resizing. Actual dimension changes
remain rejected during a move, and missing collision/invalid dimensions still
fail. No callbacks, position changes, penetration acceptance or animation aliases
are added. The original script and its animation name remain unchanged.

A synthetic touch callback checks both unchanged success and changed-size refusal
with stable position/extents. The private End regression observes the actual
Caleb animation after entry, then advances three seconds and rechecks every
runtime service at all three difficulties, followed by clean unload. See
[iteration 16](../implementation/reports/all_level_loading.md#iteration-16-ends-triggered-animation).
