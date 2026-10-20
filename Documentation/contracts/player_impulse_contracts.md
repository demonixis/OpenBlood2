# Original player damage impulses

The living local player now enables the original CDestructable damage-physics
default. See [source selections](../implementation/sources/m2_player_impulse_sources.json)
and [execution evidence](../implementation/reports/m2_player_impulses.md).
The [damage state](damage_state_contracts.md), [movement](movement_contracts.md),
[fall damage](fall_damage_contracts.md) and [player restoration](player_restore_contracts.md)
contracts still apply. M2 remains incomplete.

## Rules and local-body adaptation

Accepted non-Death damage uses the existing original impulse calculation after
resistance and the character's nigh-invulnerability modifier, before armor:
normalized direction times `damage * 2500 / mass`, added to current velocity.
Directions with squared length below .01 are ignored. Nonpositive aggregate mass
is replaced by 1. The admitted player's normal mass is 250. God mode and existing
damage refusal gates still prevent the call; rejected engine velocity leaves
motion unchanged and does not cancel otherwise accepted health damage.

The source single-player exclusion now applies: if the player is also the
attacker, health damage proceeds but there is no impulse. This includes the
original self-addressed falling damage and self explosions. It must not become a
blanket self-damage immunity. No multiplayer session is admitted by this port.

Retail has a separate server character and client physics model. Its server
sets velocity and accumulates the delta for UpdateClientPhysics/PSTATE_ADDVELOCITY;
MoveMgr then applies it to the other body. This engine deliberately shares one
checked body. The existing typed SetVelocity changes that body once, and MoveMgr
reads it before each substep. Do not add a second queued delta or integrate the
player in generic physics. Several hits accumulate; the existing once-consumed
rotating-door push adds to them. Movement drag, speed caps, collision clipping and
the contact callback velocity-delta rule remain unchanged.

The current aggregate rejects damage to already-dead owners before physics,
whereas the donor places its dead check after ApplyDamagePhysics. This existing
difference is not claimed as solved. Production NPC impulses remain disabled;
their trapped/recoil behavior is a separate integration task. No explosion pusher,
crush response, dedicated dynamic-body contact force or retail motion parity is
implied by this player-only connection.

## Restoration and inspection

The existing original damage snapshot already preserves the physics flag. Fresh
players enable it; restored players receive the saved value. Original Load and
force-position handling clear velocity even if a live checkpoint records an
impulse in progress. There is no pending client impulse to replay after restore
or NEW_LEVEL. This does not add whole-world saves or dead-player restoration.

Movement / Motion details shows whether damage physics is enabled, the count of
accepted SetVelocity operations and the last attempted delta/result (or no
attempt yet). These are historical values, not current acceleration or proof of
displacement. Inspection does not apply forces or reset counters. Existing body
velocity and movement counters show the actual response independently.

Generated tests cover additive hits, resistance/armor order, exact direction
threshold, self damage, god mode, nigh-invulnerability, disabled physics, rejected
velocity, instant death, restoration without replay, door composition and solid-wall
collision. The
real MoveMgr trajectory is checked at 30/60/120 Hz, including the generic-physics
skip. External-data tests use a production Cultist's acquisition, animations and
shots on labelled diagnostic geometry. Continuous SubwayA traversal and NVIDIA
rendering are separate checks; commercial assets remain external.
