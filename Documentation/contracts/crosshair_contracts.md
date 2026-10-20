# Original crosshair presentation

The user-requested HUD continuation now includes the default original crosshair.
See [source references](../implementation/sources/crosshair_sources.json) and
[validation](../implementation/reports/crosshair.md). M2 remains closed
with its existing GPU reservation; this is not M3 acceptance.

`game/original/ClientCrosshair.cpp` selectively ports the supported visibility and
integer camera-rectangle centering rules from `CBloodClientShell::PostUpdate`.
It uses the original `interface/crosshairs/crosshair01.pcx` image (15x13 in the tested
installation), matching that installation's enabled `Crosshair=1`, `CrosshairNum=1`
defaults. No replacement lines, resizing or invented aiming behavior is used.
The current first-person camera occupies the entire viewport. Odd-size images keep
the source's integer rounding on both odd- and even-size viewports.

The crosshair draws after the status tabs, independently of their numeric/weapon
validity. It is hidden outside the world, for the selected external camera and for
the client's received death state (`death_camera.active`). Death still shows the
statistics tabs. Inspector visibility does not affect either. Source seeing-eye,
orb, scoreboard and level-change third-person modes do not exist in the supported
single-player profile and are not claimed by this port. Zoom overrides, crosshair
cycling/toggle commands and importing/persisting user CFG preferences remain absent;
this continuation enables the original default image. The [binoculars](binoculars_contracts.md)
supersede the zoom-override omission: SMSG_ZOOMVIEW now selects crosshair98 and restores the
saved image.

The original crosshair blit passes NULL for its transparent color: exact black,
not the magenta key used by `CNewStatusBar`. Later LithTech's transparent-color
conversion corroborates the zero-color interpretation. Real-resource GPU tests
compare every crosshair pixel, including black-key holes over a colored background.
That is reconstruction evidence, not an original-executable parity claim.

The existing bounded PCX service loads/caches the external image alongside other
HUD resources. Its path/dimensions appear in the resource inspector, which also
reports current crosshair visibility. Missing/invalid images fail explicitly.
The combined atlas is now 768x75; the crosshair begins at x=656, separated from the
655-pixel statistics strip by a transparent pixel. Images exceeding the remaining
112x75 slot fail; larger modded crosshairs are not supported. GPU cycling, resource
replacement and teardown use the existing HUD owner. No new device, renderer,
dependency pin, shader or gameplay mutation is introduced.
