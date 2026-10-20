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

Invalid dimensions, missing clips/models, unavailable collision and stale/pending
owners remain explicit failures. This correction only distinguishes a normal
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
