# Sliding doors and translating world models

> [Compound-shell validation](compound_shell_contracts.md) now accepts independently closed components sharing edges; ambiguous/non-manifold components still fail. Geometry and authored BoxPhysics are unchanged.

> The later [train path contracts](worldpath_contracts.md) supersede blanket
> world-path/without-GOTHRUWORLD omissions below for static BoxPhysics translation.
> Angular paths and moving containers remain unavailable; M2 stays open.

> Extended by [interaction_contracts.md](interaction_contracts.md): original player USE and RotatingDoor
> are connected with bounded atomic collision. Earlier omissions below describe
> their previous increments; the full M2 gate remains open.

> Extended by [trigger_contracts.md](trigger_contracts.md): SelfTrigger ownership,
> key relay and transported-body sensor contacts are now connected. Earlier
> pending statements below describe the sliding-door increment.

This extends [model_and_collision_contracts.md](model_and_collision_contracts.md)
and [movement_contracts.md](movement_contracts.md). It does not close M2.
See [m2_door_sources.json](../implementation/sources/m2_door_sources.json) for source selections and
[m2_doors.md](../implementation/reports/m2_doors.md) for execution evidence.

## Original game rules

The selective `Door` port preserves the original class/property defaults,
closed/opening/open/closing transitions, normalized MoveDir, distance, opening
and closing speeds, OpenDelay, WaitTime, TriggerClosed, RemainsOpen and StartOpen.
ClosingSpeed=0 inherits Speed; OpenDelay=0 becomes 0.01 seconds.
StartOpen swaps endpoints and reverses direction: the displaced endpoint is
still called **closed** by the source state machine. It is not renamed.

TRIGGER opens a closed/closing door; it closes an open TriggerClosed door,
otherwise resets the open timer. TRIGGER during opening does nothing.
TRIGGERCLOSE closes an open/opening door. Commands are case insensitive and
travel through owned original MID_TRIGGER string messages and the base chain.
The four original speed functions are retained: linear, sine, slow-off and
slow-on (the latter three add 10% of base speed). Scheduling remains the original
0.01-second request, dispatched at most once per session step; no fixed-rate
door timing or retail executable parity is claimed.

Strings use owned C++ storage. The typed precreate path also accepts runtime
properties. Nonfinite/out-of-bounds motion parameters, invalid waveforms,
nonzero distance without direction, and unimplemented restore creation reject.
Negative OpenDelay/SoundRadius reject. Retail WaitTime=-1 and ClosingSpeed=-1
are retained: a negative wait closes on the next update, and a negative speed
uses the original signed arithmetic (moving away from the endpoint). Neither
is guessed to mean "never close". Zero distance/direction has a defined
stationary endpoint instead of dividing by zero. Original default speed zero remains a stationary motion request.

The source's unchecked MoveObject result is deliberately adapted:
`MoveWorldModel` returns Moved, Blocked, Invalid or Unavailable. Only a successful
endpoint move changes the door to open/closed. A blocked door retries on its
next update; it does not report arrival, reverse itself or invent crushing damage.

## Named geometry and typed services

OT_WORLDMODEL=2 binds one case-insensitive named DAT model after precreate (the
first in file order when names repeat; see the [duplicate-name rule](object_contracts.md#duplicate-bsp-names)).
Missing names and invalid closed shells fail explicitly, using the
same checked base-ring/topology reader as containers. A separate immutable
`world_model` record retains that shell; containers keep their static contract.

Raw DAT vertices already include WorldTranslation. Authored rotation is applied
once about that pivot. Runtime position is the translated pivot; collision
subtracts its offset before querying the immutable shape. BoxPhysics uses the
transformed shell's actual bounds center, which need not equal the pivot.
Polygon mode uses its closed shell, including interior-start detection.
Body movement, support, stair probes, clearance, touches and weapon rays all
query the current transform. Pending removal stops collision immediately.
The SAT projections now use consistent double arithmetic before rounding normals;
a regression covers point rays against authored rotated faces.

Velocity/acceleration, arbitrary teleport, rotation and dimension changes are
rejected for these records. Translation goes through MoveWorldModel (or the
checked legacy MoveObject wrapper); there is no rotating world-model CCD.
Generic physics never integrates a mover. BoxPhysics is an original choice,
not a fallback for malformed geometry.

Weapon aim rays still see solid doors. The typed fire-ray option skips doors
whose original FireThrough property is true; ordinary rays and body movement
still collide. This is a bounded adaptation of Weapon.cpp's vector continuation.
Glass, world surface penetration, impact effects and other weapon branches remain
outside this increment.

## Replacement translation solver

This is new engine code, not recovered Blood II physics.

- Door's original GOTHRUWORLD lets it slide into a wall pocket. Other translating
  world-model callers without that flag return Unavailable.
- Continuous relative sweeps detect solid bodies crossed by a translating door,
  including a large single move. The body moves by the unconsumed translation,
  with a 0.002-unit contact separation.
- Fresh support queries identify riders. They follow the full translation,
  including descending platforms, unless DONTFOLLOWSTANDING is set or their
  velocity points away from the support. That flag belongs to the rider, as
  documented by the original SDK. Jumping detaches immediately.
- At most 64 pushed/riding bodies are planned. Every proposed path is checked
  against the primary world and other solids before any position changes.
  Failure rejects the whole transaction. No speculative touches occur.
- Other world models are immovable blockers. Their checks conservatively sweep
  the moving model's bounds against their geometry. Chains/stacks of pushed
  bodies are not recursively resolved; another body's old position may block
  a move that a more advanced simultaneous solver could permit.
- Positions commit together, followed by original touch callbacks for affected
  solid bodies. Reentrant mover motion is rejected; removal/unload stays deferred
  and handle checked. Contact forces, non-solid sensor crossings by movers,
  damage/crushing and generic impulse transfer are not implemented.

The shared local player is transported once before MoveMgr runs. MoveMgr rereads
its body, refreshes support/contents and feeds the same-frame camera and weapons.
Transport does not inject an additional player velocity or duplicate integration.
The current solver makes no claim to original blocking-priority arbitration.

## Inspection and remaining gate

The **Doors** tab lists hidden/inactive live doors, shows current transformed
geometry and the travel path, and queues TRIGGER/TRIGGERCLOSE for the frame
boundary. It exposes state, parameters, motion result, blocker, pushes, riders,
FireThrough, sound requests and desired portal state. Rejected/unported source
objects remain in **World**. Stale selections and requests are checked by ID.
The same simulation and smoke commands run without Dear ImGui.

**SelfTrigger is recorded but not spawned.** Touch/AI/object/named activation,
keys and LOCK/UNLOCK relay need the original Trigger implementation. LOCK is
not fabricated as a direct Door lock: the source forwards it to its child.
Audio requests are not played; portal state does not alter visibility.
Save/Load follow the original blocks ([save/load lot 8](save_load_contracts.md#implemented-movers-and-brushes-lot-8)). Automatic deactivation, RotatingDoor/RotatingBrush, trains/path
controllers, rotating rider transport, crushing and full gameplay remain pending.

[Door audio](door_audio_contracts.md) now supersedes request-only busy/stop playback above. Portal and crushing limitations are unchanged.
