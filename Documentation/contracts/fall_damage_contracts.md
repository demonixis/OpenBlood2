# Stopping contacts and original falling damage

The selected `CPlayerObj::ObjectTouch` single-player path now consumes real
stopping-force callbacks. [Evidence](../implementation/reports/m2_fall_damage.md)
and the [source ledger](../implementation/sources/m2_fall_damage_sources.json)
separate Blood II gameplay from replacement physics. The [movement](movement_contracts.md),
[movement audio](movement_audio_contracts.md), [death](player_death_contracts.md)
and [restore](player_restore_contracts.md) contracts still apply. M2 is incomplete.

## Original rules

A newly queued rotating-door push returns before falling damage. An already
pending push does not take that early return. Otherwise the active original rule
requires either `m_bFalling` or `m_bServerFalling`, Y velocity **strictly below
-1100**, and positive `(force - 3000) / 25`. The donor's older two-second fall rule
is commented out and is not implemented. Negative reciprocal forces and force
exactly 3000 cannot damage the player.

The self-addressed typed MID_DAMAGE carries direction (0,-1,0), current position,
Normal damage type and the player as attacker. Existing resistance, armor,
god-mode and original death handling decide the outcome; a delivered message is
not necessarily lost health. No extra death, Incorporeal, free-movement, cooldown
or minimum-height gate is invented in ObjectTouch. Invalid force/query/message
failures remain visible. This remains the admitted single-player profile, not a
multiplayer falling-damage option.

`UpdateLandingSound` now also samples server falling from the existing fresh
support result. Primary-world support has a polygon and null object identity;
`!standing` replaces the old `!GetStandingOn().object` test. Client falling retains
its walkable-normal check. Both flags are preserved by the selected player field
snapshot. A blocked/dead player still retains the original history.

Fresh player initialization connects original mass 250 and friction 0 to the
existing typed character-body service. The shared player still skips generic
physics while associated: MoveMgr remains its only integrator. Live capture
requires the engine mass to equal the saved damage aggregate mass, zero friction
and a nonpushable character body. Restore configures those values after loading
the damage aggregate, before client association. No additional historical save
format or successful SaveObjects/RestoreObjects service is claimed.

## Replacement collision boundary

Blood II's SDK specifies stopping force based on mass and velocity, but the 1998
engine implementation is unavailable here. Later LithTech corroborates world
force `normalDeltaVelocity * mass * .01`, box-pair force based on relative normal
velocity and combined mass, and touch notification before applying the stopping
velocity. These formulas inform this engine; they do not establish retail parity.

Ordinary `MoveObject` collects only accepted blocking contacts. Primary polygons
and bound world models use the moving body's mass. Ordinary boxes use the absolute
relative normal speed times `.01 * (movingMass + otherMass)`, with a negative
reciprocal callback. World-model reciprocal forces stay positive. Configured
character/world/model bodies use their validated mass; otherwise mass defaults to
1. Force vectors accumulate per contacted object (or primary world) over the
bounded sweep. Sensors, speculative stair paths, successful horizontal pushes,
zero inward speed and initial penetration do not fabricate a stopping force.
The accepted stair landing can produce its actual normal force.

Final positions commit before callbacks. The moving body's incoming velocity is
still visible to GetVelocity during dispatch; afterwards its solved velocity plus
any callback velocity delta is committed. No extra motion integration occurs.
Contact order remains slot ordered for objects, followed by the primary-world
contact; it is not a time-of-impact callback ordering claim. Primary-world touch
uses null HOBJECT with actual force, never a fabricated world object. Deferred
removal, reciprocal suppression after removal and recursive-move guards remain
intact. Required query failure does not publish speculative impulses.

This does not reproduce the later engine's friction-zone residual, reciprocal
box motion, continuous rigid-body simulation or retail BSP solver. Dedicated
world-body, translating/rotating pusher and projectile dispatch paths still have
their previous zero-force callbacks; their impact/crush response remains a separate
service gap. The new forces cover ordinary moving player/character/normal bodies
against actual blocking geometry. Do not infer complete physics from them.

## Inspection and validation

Objects exposes the last nonzero successfully dispatched force, incoming velocity
and notification count. These are historical values and never grounding authority.
Movement exposes both falling flags and original raw damage requests/deliveries,
force, Y velocity and error. Read-only inspection does not dispatch contacts.

Generated tests isolate exact thresholds, both falling flags, god mode, invalid
force, door priority, real primary-world/world-model/box forces, zero-force sensor
contacts, callback velocity changes, reentrancy, deferred removal, live restore and
no repeated damage while resting. The external Caleb test falls under the real
MoveMgr at 30/60/120 Hz on a labelled diagnostic floor, then checks self damage,
original death/corpse/camera and complete cleanup. This is not an authored retail
fall measurement. Broader gameplay, lifetime, sanitizer and NVIDIA checks are
recorded in the evidence; other required services keep the M2 gate open.
