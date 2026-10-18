# Compact player HUD and numeric weapon selection

> Historical presentation: superseded by the [original statistics HUD](original_hud_contracts.md).
> Numeric weapon input and typed snapshot ownership below remain applicable.
> M2 is closed with its documented GPU reservation.

This is a compact replacement presentation of original player statistics and weapon
slots, not a port of the PCX-based `CNewStatusBar` artwork. Read the
[source ledger](../implementation/sources/m2_hud_sources.json), [player](player_contracts.md),
[weapon](weapon_contracts.md), [input](player_control_contracts.md) and
[level-change](level_change_contracts.md) contracts. M2 remains incomplete.

## Statistics and ownership

Original `CPlayerObj::UpdateStats` chooses the current weapon's primary and alternate
ammo pools. The typed owned `PlayerStats` now carries those values, their ammo types,
current weapon/slot and the ten normal owned-slot IDs, alongside health, armor and
the older bullet diagnostic. It publishes when any field changes or on forced update;
there are no retained inventory/model pointers. M16 alternate grenades must never
be displayed as bullets. Melee shows `--`, not a misleading empty-ammo warning.

The existing client association/queue checks reject stale recipients. New fields
reject nonfinite/negative ammunition, invalid ammo types, invalid slot indices and
unknown weapon IDs. HUD reads only the delivered snapshot; it performs no game
updates, resource queries or mutation. Client exit clears the snapshot. Fresh and
restored entry republish it; whole-game save support is unchanged.

Display values truncate positive fractions; health/armor clamp to 0..65535 and
ammunition to 0..999, preserving the original extra ammo display cap. The typed
snapshot retains raw counts for inspection. Negative health clamps to zero before conversion,
an explicit safe presentation adaptation. Health <=25 and empty ammunition are red.
The secondary count appears only for a distinct nonzero alternate ammo pool. An
external camera hides the HUD, following the original client status-bar gate.
Unsupported/no current weapon or invalid display numbers hide the panel.

## Input and original rules

Physical number-row 1..9,0 and keypad 1..9,0 select normal inventory slots 0..9.
This is the original command-slot ordering, not a hardcoded weapon-ID table: initial
1 is the knife and 2 is the Beretta; later slots follow acquired inventory. P/R/B
retain the separate reserved proximity/remote selection and detonation bindings.

A fresh keydown queues one selection. Auto-repeat is ignored, the last selection
in a frame wins, and sampling consumes it once. Focus loss, player/session replacement,
disabled controls and text editing discard pending selection. Number keys used to
edit inspector fields must not switch weapons afterwards. AZERTY uses physical keys
without requiring Shift. The HUD itself takes no focus or mouse/keyboard events.

`GameSession::command_weapon` applies original dead/movement-blocked rules and the
existing client/camera gates, then calls original `ChangeWeaponSlot` through the
checked session selection path. Empty slots fail without changing weapon; ordinary
owned weapons may be selected with zero ammo. Selecting the already-current type
is a no-op and must not release held fire input. The existing view-model preparation,
rollback on missing assets and fire-input reset remain intact. Diagnostic inspector
selection remains separately available; it does not certify gameplay gates.
Zoom/binocular behavior and unported weapon classes are not added here.

## Rendering and lifetime

The pinned Dear ImGui SDL3/SDL_GPU backend is shared by HUD and inspector through
one `UiRuntime`, including inspector-free builds. Dependency revisions and shaders
are unchanged. Runtime destruction waits for GPU idle before shutting down renderer,
platform and context; the SDL device outlives them. The inspector no longer owns
that shared context. Presentation has no gameplay or asset ownership.

The HUD uses a noninteractive background draw list above the 3D scene and behind
the inspector. It follows viewport dimensions; narrow views omit the weapon strip,
and views below 320x160 omit the panel. F1/collapse do not disable HUD or simulation.
No original health/armor icons, PCX fonts, air/focus/item bars, boss/communication HUD,
weapon wheel or rebinding menu is claimed. Those remain separate presentation work.
