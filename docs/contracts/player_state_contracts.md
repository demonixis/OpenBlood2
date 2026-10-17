# Selected original player fields and restore initialization

> Continued by [player restore contracts](player_restore_contracts.md): supported
> engine state and original restored-client association now execute in the same
> loaded world. Earlier blanket omissions below are historical. Full client/world
> state and safe NEW_LEVEL transfer remain incomplete.

> Extended by [node visibility and player model restoration](character_node_contracts.md):
> original named resets now affect rendering/hit candidates, and the selected Load
> model/flags/velocity tail is available before client association. Earlier blanket
> node-hide/Load-tail omissions are historical. Full engine/client restore, limb/gib
> behavior and the M2 transfer gate remain incomplete.

`CPlayerObj::SaveFields/LoadFields` now preserve the implemented part of the
original Save/Load value block. `INITIALUPDATE_SAVEGAME` binds the original
aggregates/faction lists and takes the original early return. These are
prerequisites, **not a complete player restore, KeepAliveLoad or level transfer**.
See [source ledger](../implementation/sources/m2_player_state_sources.json) and
[validation](../../../Documentation/Implementation/m2_player_state.md).

## Values and source semantics

The owned `PlayerState` covers smell timestamp/delay, the player's separate dead
flag, movement blocking, pitch/yaw, air, nullable trigger/player-name strings,
last crouch/forced crouch, character/skin, four attributes and the supported
PMOVE byte (0..14). These values already have original counterparts in the port;
this does not add storage-only stand-ins for every missing original field.

Save stores `smell_time - GetTime()`, and Load adds the destination GetTime().
Negative offsets and negative reconstructed timestamps remain valid; delay is
an unchanged duration. The periodic player smell-spawning loop is still absent,
so this is preservation of its timer, not evidence of a working scent trail.
The player dead flag is distinct from the damage aggregate's raw/effective dead
state and still participates in original pickup refusal. Air continues through
the existing original drain/recovery rules. An unassociated owner's air update
can change air and then fail UpdateStats publication; do not report that as a
successful client service.

Load assigns raw attributes without ValidateAttributes, Respawn, pickup rewards
or stat/ammo reset. The original last-sent health/armor/ammo caches reset to zero,
so subsequent valid client publication sends changed values again. First-update
state is deliberately not saved by the source and is not overwritten by LoadFields.
Current body stance/pose, engine animation and client controls are not reconstructed
from the saved PMOVE/crouch fields.

Null and allocated-empty HSTRING values remain distinct via optional owned
strings. Save replaces its output only after complete validation. Load allocates
both new strings before replacing either old handle; failure releases the stage
and preserves existing fields and ownership. Old handles are released exactly once.
Snapshots contain no engine identity and survive source removal and world unload.

## Restore initialization and limits

The original common InitialUpdate path binds inventory/damage, creates the owned
muzzle light, inserts the player into both faction lists and sets first-update.
The savegame branch then returns before fresh health/ammo grants, scent creation,
TRIGGER allocation, body dimensions, blocking priority and update scheduling.
The port retains zero move/jump speeds on this branch. Defaults such as damage HP=1
are constructor values, not a restored playable player. Engine/body restoration
and the remaining Load tail must supply the missing state before entry.

A failed child/link allocation marks the owner pending, as required by deferred
object lifetime; it is reclaimed with its children at the frame boundary. The
restore branch alone must not create an active associated player or be counted
as successful SaveObjects/RestoreObjects. Those engine services still refuse.

Save/Load require a live initialized owner outside damage/weapon callbacks;
Load additionally refuses an associated owner. Active player powers are refused
because their expiration timers are not yet ported. Nonfinite scalars or clock
sums, negative delay, air outside 0..100, attributes outside 1..6, unsupported
character/skin/PMOVE, and strings over 100 bytes or containing NUL fail explicitly.
The current supported character/skin is Caleb/default. Source Load assigns raw
attributes; the existing later ValidateAttributes behavior is a separate operation.

## Inspection and remaining gate

Exit processing captures these fields alongside the earlier inventory/damage
records, even with the UI hidden. Each record remains separate; their success
does not represent an atomic whole-player checkpoint. Failed capture clears its
candidate and exposes an error. PreStartWorld/unload clears the record. The
Objectives tab shows the actual values and limits; selected-player skeleton
inspection shows restore initialization and field-load state without executing it.

Remaining source values include fall/control state and clocks, power timers and
sound stack, binding/regen/slow-death/imprison/burning state, gun/camera references,
swimming controls, scores and client saved data. The Load tail's model/node reset,
flags, velocity/zoom policy, engine body/animation/scheduling, restored client
association and safe named destination entry are not implemented by LoadFields.
General saves, other characters and multiplayer remain unavailable. NEW_LEVEL
continues to refuse before teardown; M2 remains incomplete.
