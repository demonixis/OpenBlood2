# Keyboard fire and relative mouse controls

This application adapter connects existing typed movement, USE, view rotation and
weapon input. The original weapon animation/FIRE_KEY, damage, ammunition, camera
restrictions and MoveMgr remain responsible for gameplay. No game rules or new
integrator are introduced. See [evidence](../../Documentation/Implementation/M2_PLAYER_CONTROLS.md)
and [provenance](M2_PLAYER_CONTROL_SOURCES.json). **M2 remains incomplete.**

## Bindings and inspector

Physical WASD/ZQSD, Shift, Space, Ctrl, arrows and E retain walk/run/jump/crouch/look/USE.
F fires primary; G fires alternate. F2 enables/disables these game controls.
Entering a player through the inspector now requests play automatically: game controls
are enabled and SDL relative mouse capture hides the inspector once the window gains
focus. The explicit Play / resume button does the same. Hiding inspection with F1
returns to play. F3 still toggles relative mouse mode for an entered player.
Mouse motion changes yaw/pitch, left button fires primary and right fires alternate.
Uncaptured mouse buttons belong to the UI and never fire. Escape releases a captured
mouse without quitting; release restores the inspector in development builds. F1 opens
inspection and releases the mouse if captured. Otherwise its existing hide/show action
preserves keyboard controls. Native title-arrow folding and resizing remain available.
`--view-world` initially requests capture. A request waits for the first focus event
instead of being discarded while the compositor is still presenting the window.
An explicit focus-loss event cancels it; focus gain alone never resumes a released
session. See the [playtest correction](../../Documentation/Implementation/M2_PLAYTEST_INPUT.md).
The inspector displays bindings, actual capture state and SDL capture failures.

Relative motion accumulates per frame and is consumed once, independent of frame
seconds. The explicit replacement device policy is .0025 radians per SDL relative unit,
wrapped mouse yaw and pitch limited to [-1.48,1.48]. Finite deltas are bounded to 10,000
units per accumulated axis; nonfinite events are ignored. Suppressed frames discard
motion instead of replaying it. These bindings/sensitivity are not recovered retail
configuration; rebinding, controller/touch/VR input and sensitivity UI remain pending.

Only events belonging to the app window change the state. Focus loss clears keys and
mouse state; capture does not automatically return on focus gain. Disabled controls,
text editing and absent player release capture. A held firing key suppressed during
text editing or disabled input must be released before it can fire again. Repeat-only
USE/fire presses following focus loss are ignored. Player identity changes clear
latched input and capture, including unload/reload or replacement between frames.
A failed SDL mode change is reported, never represented as a successful capture.

## Original dispatch and verification boundary

At each frame boundary, the app clears its prior weapon input, applies queued inspector
commands, then merges keyboard/mouse primary/alternate buttons. It updates the existing
client rotation before normal session stepping. Mouse/keyboard input never calls a
weapon Fire method or mutates ammunition directly. The source CViewWeapon state machine
and crossed FIRE_KEY markers still generate server requests. Releasing a button may
finish an attack already committed by the original animation; it does not cancel it.
Camera restrictions remain in the original shell; the SubwayA intro is not skipped in
normal use. Tests explicitly advance it before their firing scenarios.

The existing GPU weapon smoke now injects SDL F/G events through the production app
loop instead of assigning WeaponInput directly. A separate real-window private test
checks successful SDL relative mode, once-consumed mouse motion reaching client camera
orientation, both mouse firing buttons, Escape, uncaptured clicks, focus loss/recovery,
actual source FIRE_KEY, ammunition and fire voices. It is an event-driven harness, not
a human playthrough. Pure tests cover foreign windows, repeat keys, focus/text/control
gates, finite mouse bounds and player-change reset. Existing collapse/movement/UI-free
Release paths remain checked. No reference source, commercial asset or dependency changed.

The later playtest correction also checks primary/secondary mouse fire against an
authored Cultist, including original damage, corpse transfer and checked reload.
This is bounded encounter evidence, not all required services, safe NEW_LEVEL or M2.

The [medkit continuation](MEDKIT_CONTRACTS.md) adds original MedKitPU acquisition,
CInvMedkit recharge/healing, H direct-item input, local activation audio and owned
charge preservation through Restore/KeepAlive. Other items, original inventory HUD,
pickup audio and safe NEW_LEVEL remain unavailable; M2 stays incomplete.

The [proximity continuation](PROXIMITY_CONTRACTS.md) adds original weapon49 in reserved slot10, item7 collection, P selection, charged/secondary throws and independent CProximityBomb lifecycle. Owned snapshots now contain thirteen slots, with only the first reserved slot implemented. The shared grenade/bomb service is named AdvanceModelProjectile. Earlier omissions/counts remain historical; M2 stays incomplete.
