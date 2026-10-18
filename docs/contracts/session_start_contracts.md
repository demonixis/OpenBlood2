# Checked original-session startup

`GameSession::run_world` now performs a bounded startup check instead of always
returning the historical M2 refusal. This is a replacement engine entry boundary,
not a new original game rule or a declaration that M2 is complete.

Read [world loading](world_contracts.md), [player entry](player_contracts.md),
[session playback](session_playback_contracts.md), [service coverage](service_coverage_contracts.md)
and [level replacement](level_change_contracts.md) before changing this boundary.

## Admission and ownership

Only the bound session outside object callbacks can run the check. Reentrant and
foreign calls leave its previous report unchanged. A loaded collision world is
required; a queued level transition rejects startup without consuming the intent.
Every source object must have a disposition. Unknown classes, failed entries and
recorded unavailable services reject admission. Editor-only, difficulty-filtered
and the documented missing-named-BSP omissions retain their distinct meanings.
A missing BSP is never a created object or a general resource-error exemption.

Existing live NPC runtime failures reject the check before player entry and again
after entry callbacks. A new player enters through the existing original server
and client shells. An existing player is checked, never respawned or duplicated.
The server identity, live object, client in-world state and client player handle
must agree, and a dead player cannot pass a fresh startup checkpoint. A start
trigger requesting another level rejects the check.

The shared `world_validation` helpers retain NEW_LEVEL's strict CPU presentation
preflight: actual world DTX/SPR frames, hidden model/skin resources, sprite frames,
particle-stream textures, pending sky references and client view-weapon skins.
They use the existing mesh/image limits, including 4,096 images and 128 MiB decoded
pixels. Models with retained load errors fail. No GPU allocation or simulation
clock advances during validation. Recorded client music admission/transport failures
also reject startup; missing music is not replaced by a successful silent service.

On rejection, a player newly entered by this attempt is detached and destroyed
through normal shell cleanup. A player that existed before the attempt is retained.
**This is not world rollback:** original start triggers may already have changed
objects or started voices. Those changes remain visible. A failed fresh attempt
that queued a recursive exit clears that new player's queued intent during cleanup.
Use the existing NEW_LEVEL transaction for failure-safe world replacement.

## State and inspection

`Ready` records a successful explicit startup checkpoint. It does not promise that
all future resource requests or game branches will succeed. Diagnostic `step` and
`enter_player` remain independently usable; they do not masquerade as startup
validation. Leaving a validated player returns to `Inspection`; unload returns to
`Empty` and clears the report. A failed loaded-world check reports `Blocked`.

The owned report contains attempt count, world revision, simulation time, result,
new-player-entry observation, source dispositions, omissions, unavailable services,
decoded-image count and error. The World inspector displays the last result and
its time, clearly labelled as a snapshot. Reading it runs no callbacks. Rechecking
an existing session does not reset clocks, player stats, identities or NPC state.
The engine object's clock remains monotone across reloads; loading does not imply
that its absolute value is zero.

## Application modes

`--start-game` is a finite headless check: load, validate original player entry,
advance one 1/60-second update, recheck, then unload. It exits 0 on success, 6 on
startup rejection and 5 on loading/update/cleanup failure. Player entry precedes
the first update, avoiding the old no-player AI initialization order. Combining
it with `--inspect-player` still uses the same player, not a duplicate.

`--view-world` uses the same startup check before its first graphical frame.
Explicit inspection and smoke APIs retain their diagnostic entry behavior.
A successful check does not certify later combat, the complete campaign, native
Metal execution, autosave or all M2 acceptance evidence.

See [implementation and validation](../../../Documentation/Implementation/m2_session_start.md).

## Container textures

Startup presentation preflight checks the textures of every model that can be
drawn: the main world, models bound to world-model objects and unbound models
(runtime debris may draw them later). Models bound only to containers (Container,
VolumeBrush: conveyors, liquids, ladders…) are never drawn, so a missing texture
there no longer blocks startup. Retail 05_CDM's conveyors use the editor texture
`default`, which no archive provides.
