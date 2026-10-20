# M2 — original liquid surfaces

Status: static VolumeBrush PolyGrid surfaces implemented; **M2 remains incomplete**.
Implementation commit: `e54401b654171f76419d2a5dbd8c8783cd828a04`.

## Reproduced missing service

CityHub already had working water membership, swimming and air rules, but
VolumeBrush only stored ShowSurface/SPR/height/alpha requests. It never created
the original PolyGrid publisher or client CPolyGridFX. Consequently there was no
water geometry to draw. This increment connects those original paths and renders
the resulting height fields with the existing SDL_GPU alpha pipeline.

The original server placement, 5:6:5 color encoding, integer grid size, normal and
four-ring plasma, palette endpoints, scale oscillation and ON/OFF propagation are
preserved. Shared four-ring phases belong to each session, including destination
rehearsals. A surface outlives removal of its source volume, matching the original
destructor's absence of child removal; removing its own publisher or unloading the
world releases it. The inspector exposes live, hidden, missing and failed surfaces.

The replacement physics loop previously treated every OT_NORMAL as a potential
body. The new visual publisher is explicitly excluded from body integration and
volume-damage polling. This prevents currents moving the surface itself and
preserves existing deferred container callbacks. It is a scoped engine policy;
no original damage, swimming, AI or player-balance rule was changed.

Read [contracts](../../contracts/polygrid_contracts.md) and
[source hashes](../sources/m2_polygrid_sources.json) for exact rules and
adaptations. Reference code and external game files are unchanged.

## Private-content observations

Two CityHub volumes request surfaces:

| Volume | Surface center | Extents from volume | Grid | Resource |
| --- | --- | --- | --- | --- |
| Canal1 | (0,135,-704) | X/Z 1088/1536, surface Y 2 | 8x8, 98 triangles | Textures/reds/waterbrown.dtx |
| Pool | (3488,399,2208) | X/Z 544/608, surface Y 2 | 8x8, 98 triangles | Textures/reds/waterbrown.dtx |

Both grids use the actual SPR frame and authored scale/color/alpha. Checked surface
positions agree with the geometry's top minus half the requested surface height.
There are two additional independent runtime objects; source-entry counts are
unchanged. Private tests enter through original shells, step without an inspector,
prepare frames read-only, remove/reload and require fresh effect identities.

NVIDIA readback checks repeated alpha texture sampling and actual animated private
surfaces. A diagnostic above-bank CityHub view also compares identical world frames
with/without the surface draw: 65,234 of 120,000 pixels differ. Private captures were
visually inspected and remain ignored build outputs, not committed game assets.
This is diagnostic evidence, not a human level traversal or retail pixel comparison.

## Verification

Debug, inspector-free Release and Clang ASan/UBSan each pass 38 targeted
headless checks, including seven private-content checks. NVIDIA Vulkan passes
three GPU checks per Debug/Release build: container inspection, safe level change
and PolyGrid readback. Final targeted reruns also cover rejected frame indices.
The selected builds succeed; the existing ItemPickups enum/float warning remains.
All 10,923 inventoried reference files and six reference repositories are unchanged.

Validation results and reproducible commands are recorded in
[evidence](evidence/m2-polygrids.json). A negative control recompiles the previous
VolumeBrush.cpp against current interfaces and the new lifetime probe: it fails
because the required surface is absent. This isolates the missing publication;
it is not a complete old build benchmark.

The first new lifetime test incorrectly used a complete session step in a synthetic
fixture without loaded shells; it was corrected to step its owned object/effect
services directly. Existing container index callback checks then caught the new
publisher being included as a physical body; the scoped publisher exclusion fixes
that regression with the original assertion unchanged. No assertion was relaxed.

## Limits and remaining gate

High detail and always-update are explicit policies; retail WASDRAWN culling and
low/medium detail are not implemented. Texture pan is an offset, not an invented
velocity. The new mesh uses later-engine XZ/height/UV corroboration; exact old-engine
rasterization is unverified. Surfaces share world/model depth and sort their cells,
but sort separately from glass and other FX. Reflection, refraction, underwater
fog/color/audio and moving volumes remain unavailable. Existing SPIR-V/MSL shaders
are unchanged; native macOS execution of this increment was not automated.

Remaining required services, full restoration, broader combat/AI reports and the
observable roadmap M2 exit criteria remain open. The README milestone estimates
remain unchanged and do not certify acceptance.
