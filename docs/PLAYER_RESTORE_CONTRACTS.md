# Player engine restoration and restored client association

> Continued by [named keep-alive entry](PLAYER_KEEPALIVE_CONTRACTS.md): supported
> captured players can enter an already loaded different world at a named start.
> Automatic NEW_LEVEL still refuses; staged cleanup is not world rollback.

This is an owned, in-memory snapshot of the **supported living local player** in
the same loaded single-player world. It combines selected original inventory,
damage and player Save/Load paths with replacement engine state restoration and
the original shells' saved-client entry branch. It is not a retail save-file
reader, whole-world checkpoint, complete client save or accepted NEW_LEVEL transfer.

## Capture and lifetime

`GameSession::capture_player` requires a live associated, initialized player outside
object callbacks, with active scheduled engine state. It rejects pending door
pushes, attached/dynamic bodies, deactivation, dead/dying players, active powers
without saved timers, and aggregate/fire callback hazards. Failed capture leaves
the caller's previous snapshot unchanged. Every string/vector is owned; no source
HOBJECT, HCLIENT, model pointer, contact, attachment or aggregate pointer is saved.

Saved engine fields are object name, model/skin identities, position, rotation,
scale, raw collision half-dimensions, flags/user flags, blocking priority, animation
track including its consumed-initial-key state, node mask and remaining update
delay. Selected player/aggregate fields follow their existing contracts. Crouched
body/eye state bridges this port's separate collision/camera representation.
Velocity/acceleration are recorded, but original Load/force-position rules later
reset them. Contacts and support are queried anew; no old platform identity survives.

## Restore order and original behavior

1. Require no connected client and no callback/reentrant step. Reject a different
   world, unsupported Caleb profile/resources, invalid vectors/quaternion, body,
   name, eye, clock, clip or node mask before association. A snapshot does not rewind
   the world: movers/NPCs may have changed since capture.
2. Create a new CPlayerObj with `INITIALUPDATE_SAVEGAME`. This binds aggregates and
   faction lists without fresh stats, scent or start trigger. Restore engine values
   on the staged owner; check its exact saved body against current scene collision.
3. Apply original inventory `Restore` (keys retained), damage and player fields,
   then `RestoreModelState`. Its named node reset, exact flags without SHADOW and
   zero velocity intentionally supersede saved engine values. No Respawn occurs.
4. Restore scheduling relative to the destination clock. Associate a new checked
   local client token with the staged owner and its inventory. Typed explicit
   identity replaces retail HCLIENTREF enumeration; do not search names or silently
   fall back to a freshly reset player. The saved-position shell branch sends init
   and facing, retains the eye/body, and never invokes GoToStartPoint.
5. The client's restored-entry mode suppresses fresh SINGLEPLAYER_INIT. Repeated
   restored init requests remain no-ops; the existing fresh-entry duplicate guard
   stays in place. The source
   first-update block derives movement speed from saved attribute 1..6 and jump
   speed 600, publishes physics/position/stats, then clears first-update. This runs
   eagerly at the local association boundary so MoveMgr is ready before stepping.
   **Do not call ValidateAttributes here**: original PlayerObj.cpp:1591-1604 does
   not clamp attributes, recalculate resistance/caps or grant ammo/health.
6. The existing ForcePos handler clears acceleration/velocity and resets local
   movement predictions. Only the associated MoveMgr integrates the player.
   Views and held models are rebuilt, then attachment ownership is checked.

Any staged failure removes the new player and all generated children, clears
client messages/association and leaves existing world objects intact. An active
player is never silently replaced: capture, leave, then restore. This is failure
cleanup for an unassociated/new owner, not rollback of an already destroyed player
or world. Snapshots can survive unload and restore after reloading the same world.

## Inspection and validation

Session exposes **Capture player state**, **Leave world**, and **Restore captured
player**, saved body/clock/animation/stats, attempts/success/error and the actual
restored-client flag. Commands execute outside UI traversal; hidden/folded UI has
no effect on simulation. A failed recapture keeps the previous successful snapshot.
The last restored ObjectId is inspection only and may become stale after teardown.

`player_restore.*` covers nondefault health/armor/ammo, raw level-6 attributes and
caps, pose/scale/body/eye, exact animation/deadline, no fresh reset, checked stale
clients/move codes, real movement and one integrator, malformed records, blocked
placement, early and late resource failures, repeated reload and list/string/
attachment cleanup. Private checks use external SubwayA/Caleb; GPU checks render
the restored player, and `--restore-smoke` repeats association across UI hiding and
world reload with and without the inspector build.

## Explicit limits

No retail serialization/SaveObjects/RestoreObjects success is added. Dead-player
states, power timers, complete client data/HUD/objectives/cinematic state, all
transient animation-selection/random/audio state, whole-world restoration and
safe cross-world keep-alive entry remain unavailable. Restored engine animation is
preserved at entry; subsequent original commands can select/reset its clip. No
retail physics or binary-save compatibility is claimed. NEW_LEVEL still refuses
before teardown; Gideon and required FX/services keep M2 incomplete.

The [Gideon fade/teleport continuation](GIDEON_TELEPORT_CONTRACTS.md) adds the original selected states, live model color/alpha, real transparent model presentation and supported player color preservation. Earlier blanket omissions are historical; production boss lifecycle and M2 remain incomplete.
