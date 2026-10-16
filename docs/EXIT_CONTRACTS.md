# Original level-exit contracts

> Continued by [named keep-alive entry](PLAYER_KEEPALIVE_CONTRACTS.md): supported
> captured players can enter an already loaded different world at a named start.
> Automatic NEW_LEVEL still refuses; staged cleanup is not world rollback.

> Continued by [player state contracts](PLAYER_STATE_CONTRACTS.md): selected player
> values and the original savegame initialization early return now execute.
> Earlier blanket player-field/initialization omissions are historical; full
> engine/client/player restoration and level transfer remain unavailable.

> Continued by [damage state contracts](DAMAGE_STATE_CONTRACTS.md): original damage
> Save/Load values and death counter/delay/nigh-invulnerability now execute.
> Earlier blanket damage-save omissions are historical. Whole player restoration
> and safe level transfer remain unavailable; M2 stays open.

> Extended by [inventory state contracts](INVENTORY_STATE_CONTRACTS.md): supported
> original inventory Save/Load and exit-time capture now execute. Full player
> preservation/association and safe destination entry remain unavailable; NEW_LEVEL
> is still refused and M2 remains open.

`ExitTrigger` now executes its original property and activation branches and
broadcasts an owned typed `SMSG_EXITWORLD` equivalent. The client shell applies
`HandleExitWorld`'s dead-player guard and constructs the original
`CMSG_LOADWORLD` request with `LOADTYPE_NEW_LEVEL`. **Level transfer is unavailable**:
the load decoder refuses the required `KeepAliveSave/KeepAliveLoad` path before
resource lookup, world replacement or player removal. A queued message or a
registered ExitTrigger must never count as a completed level exit or M2 completion.

## Source behavior

- Preserve Trigger ancestry, active-command guard, activation counts, sender/key
  filters and deferred callback lifetime. ExitTrigger defaults to no touch/AI/object
  activation, with player/relay activation enabled and zero reset time. With zero
  reset it remains inactive after activation unless explicitly rearmed.
- Copy `NextWorld` and `StartPointName` before parent precreate; the default start
  name is `start`. Save/restored construction remains rejected.
- Preserve the source spelling mismatch: registration declares `EndEpisode`, but
  ReadProp reads `EndOfEpisode`. No alias fixes this. The latter is accepted as an
  optional, non-defaulted property because the source reads it (as with the existing
  DifficultyInsane adaptation). When present/true it takes precedence over SubWorld.
  Exit type values are world=0, subworld=1, episode=2. The type does not choose a
  different load mode in the original HandleExitWorld branch.
- A nonempty start name is copied into server-shell ownership **before** broadcasting,
  even if no client exists or its queue is full. An empty name leaves the previous
  name intact. PreStartWorld does not reset it; explicit new-game/session teardown
  clears it as an adaptation that prevents stale diagnostic intent from crossing runs.
- Immediate SendMessages sends one packet, independent of target/message slots.
  The delayed path uses original virtual SendMessage: zero-delay slots require
  both target and message; pending delayed slots call it even with empty names.
  Each dispatch broadcasts the whole exit request, not a named trigger message.
- Dead clients ignore the payload before changing their last request. The port uses
  the checked original player's IsDead state; the retail asynchronous client dead
  notification is not reconstructed. Empty NextWorld reaches the client invalid-data
  branch; it is not silently converted to a valid world.

## Ownership and refusal

Strings are owned values (100-byte maximum, no embedded NUL), replacing HSTRING
and unchecked fixed buffers. Enum values outside 0..2 fail before publication.
The existing single local client has a 256-event queue. Broadcast results measure
queue delivery only. Removing the source cannot invalidate queued data; disconnect
clears events, and identities are not reused. Dead/invalid/unsupported exits do not
make the message pump disconnect the player.

The message pump runs outside object callbacks and applies the same load decoder
used by explicit load_world requests. NEW_LEVEL is explicitly refused, with the
KeepAlive diagnostic and a bounded last-result record. No fresh-game fallback,
recreated default inventory, dummy save file or success-shaped transfer exists.
The client retains its loaded state, weapons, objectives and audio because no
replacement is accepted. Music/menu/loading-screen teardown and first-world state
changes from the rest of the original HandleExitWorld are deliberately deferred
with the unavailable transfer, not represented as completed services.

Supported captured-player restoration, restored-client association and named
keep-alive entry into an already loaded destination are now implemented (see the
continuation above). Before enabling NEW_LEVEL, destination preparation still must
validate required resources/classes and protect the current session on refusal;
staged player cleanup alone cannot roll back world initialization or start triggers.
Unsupported saved fields must continue to fail explicitly.
General saves, addon character swaps, multiplayer and demo EndGame handling remain
outside this increment. The original commented-out world-revisit save is not active
behavior to reintroduce.

## Inspection and tests

The Objectives tab includes hidden/inactive ExitTrigger instances, source names,
requested world/start/type, broadcast result/counters and the client's explicit
transfer refusal/dead-ignore counts. Existing object/property inspection remains
available. The player TRIGGER button goes through the owned command queue and is disabled
when player activation is disallowed.

`exits.*` covers source defaults/spelling/precedence, delayed virtual dispatch,
no-client/start-name ownership, blocked transfer without player/inventory loss,
dead guards, queue bounds, source/client lifetime and three private SubwayA cycles.
The private `exit2` targets `02_cityhub` / `01_egressa`, rejects direct players,
and accepts its referring Trigger through ObjectActivate (TriggerRelayActivate
is false). Its otherwise empty first slot waits two seconds. Both the private
case and gpu.objectives scenario send from that authored referring identity, then
advance 119 fixed 1/60-second steps plus 0.05 seconds through GameSession::step.
They verify no early delivery and an explicit transfer refusal. The GPU scenario
runs with the UI hidden and after reload, preserving player, inventory and world
revision. This isolates the exit dispatch; it does not claim the upstream level
completion chain or a real transfer. Diagnostic rearming is labelled, not an
original reset rule. See [evidence](../../Documentation/Implementation/M2_EXITS.md).

The [destination preparation continuation](DESTINATION_CONTRACTS.md) shares const property validation and structural CPU preparation with fresh loading.
Blocked exits now perform read-only destination lookup after refusing transfer; earlier blanket resource-lookup omissions are historical.
No destination callbacks run and no active session is discarded; safe automatic NEW_LEVEL and M2 remain incomplete.
