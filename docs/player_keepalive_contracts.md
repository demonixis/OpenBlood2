# Named keep-alive player entry

> Current continuation: [safe NEW_LEVEL](level_change_contracts.md) promotes a fully prepared
> destination with the supported player, or retains the source on failure. Earlier
> blanket transfer omissions below are historical; each standalone diagnostic API
> retains its documented scope. CityHub and full M2 acceptance remain incomplete.

This extends [player restoration](player_restore_contracts.md) with the original
new-level entry branch **into an already loaded destination**. It is not an
accepted ExitTrigger/NEW_LEVEL transaction, a retail Keep.sav implementation or a
whole-world checkpoint. Automatic exit requests still refuse before teardown.

## Source behavior and ownership

`GameSession::restore_player` accepts explicit `PlayerRestoreMode::KeepAlive` and
an owned-at-call start name. It requires no current client, a started single-player
world and the same supported living-player snapshot validation as saved-position
restoration. The source world may differ. Ordinary SavedPosition still requires
the same world and refuses a supplied start name. Unknown modes, NULs and names
over 100 bytes fail. Capture/output ownership is unchanged; no old handle crosses
worlds. This API never loads or unloads a world itself.

Original inventory Load uses type 1: keys are discarded, while supported weapons,
raw ammo and selection survive. Damage/player values, raw attributes/caps, original
Load overrides and relative clocks use the existing restore path. Neither shell
entry nor client initialization calls Respawn or fresh profile validation.

The original RespawnPlayer keep-alive branch calls GoToStartPoint after association
and the init message. An empty supplied name selects the source default `start`.
Matching is case-insensitive, excludes multiplayer markers, and falls back to the
last enumerated solo marker if the name is absent. FindNamedObjects prepends stable-slot entries to its list; fallback uses the last
solo entry in that list, not the last-created object. This is not claimed retail-order equivalent. With no solo marker, the port fails rather
than teleporting a partially initialized player to zero.

GoToStartPoint copies the selected marker name for inspection, takes its yaw and
zero pitch, samples its position, then **sends its trigger before teleporting**.
The marker can remove itself; no marker fields are read after dispatch. A trigger
that removes the player prevents further entry and causes staged cleanup. The
existing ground helper and physics/force-position messages run, then first-update
speeds/stats are published eagerly so the single MoveMgr integrator is ready.
The saved source-world position is not clearance-tested against destination geometry;
final destination placement is checked with the restored body. Animation/body/eye
retain existing restore semantics, including crouched snapshots.

Failures discard newly staged player/children/client association. They do not
reload or rewind the destination, undo start-trigger effects, or resurrect a
previously unloaded world. Missing ground/resources and blocked placement report
failure. This limitation is why automatic NEW_LEVEL remains refused. Future
transaction work must validate/stage the destination without discarding the current
session; merely reloading the old DAT would lose world/NPC state.

## Inspection and validation

Session retains the same-world restore button and adds a destination start field
and **Enter loaded world with captured player**. Capture first, explicitly load a
destination, then enter with the saved player. Last successful mode, destination,
requested/selected start and fallback status are owned diagnostic values; failures
have a separate error. The UI never advances the entry path while drawing.

`player_keepalive.named/fallback/failures/private` checks source ordering, key
removal without modifying the snapshot, raw stats, facing, real movement, missing
and multiplayer markers, fallback, malformed mode/name, ground/clearance failures,
callback removal and repeated list/string/attachment teardown. Private checks
explicitly load CityHub after capturing SubwayA, then enter at `01_egressa`.
This is harness-directed entry, not evidence of a completed authored level exit.
`--keepalive-smoke` exercises the same destination entry across hidden UI and reload.
See [implementation evidence](../../Documentation/Implementation/m2_player_keepalive.md)
and [source ledger](m2_player_keepalive_sources.json).

Full client state (objectives/cinematics/music), missing player powers/death state,
automatic safe transfer, remaining world classes and required FX/services still
keep M2 incomplete. CityHub inspection does not imply all of its classes work.

The [destination preparation continuation](destination_contracts.md) shares const property validation and structural CPU preparation with fresh loading.
Blocked exits now perform read-only destination lookup after refusing transfer; earlier blanket resource-lookup omissions are historical.
No destination callbacks run and no active session is discarded; safe automatic NEW_LEVEL and M2 remain incomplete.

The [destination initialization rehearsal](destination_trial_contracts.md) extends the structural scan with temporary original callbacks and copied-player named entry.
It isolates the current original globals, destroys the temporary session and resumes the unchanged source gameplay state.
Ordinary second-session binding remains forbidden; no automatic transfer or completed M2 acceptance is claimed.
