# Original corpse blood pools and client splats

Selected `CClientSplatFX` and `CBloodSplatFX` now execute through typed publication,
owned sprite state and the existing SDL_GPU sprite pass. See the
[source ledger](../implementation/sources/m2_blood_splat_sources.json) and
[implementation evidence](../implementation/reports/m2_blood_splats.md).
This extends [corpse death effects](death_effect_contracts.md) and
[sprite presentation](sprite_view_contracts.md). **M2 remains incomplete.**

## Original caller and ownership

After animation completion, CCorpse keeps its original 40-unit downward segment,
ignores itself, and creates a pool only for a primary-world hit, not another
object. It adds .1 to world Y and creates CClientSplatFX with scale .1 and growth
.02. Burning corpses can request again on subsequent .1-second callbacks; do not
invent a once-only gate. Cosmetic failure remains separate from corpse removal,
health, collision and animation. The corpse inspector retains the ray, event ID
and actual failure.

CClientSplatFX is a runtime-only CClientSFX subclass. Its base chain supplies
FORCECLIENTUPDATE; initial update schedules .0001 seconds and the next update
requests deferred removal. Setup publishes immediately. A typed copied descriptor
replaces the instant network payload; no pointer to the publisher or corpse is
retained. The sprite survives either object's removal. World unload clears the
store before resources; destination rehearsals own independent stores. Authored
creation/save reconstruction remains rejected. Linked server-position overrides
are outside this instant-message subset.

The source passes the normal as both AlignRotation forward and up. The engine
now shares the existing reviewed parallel-axis fallback with CMovement, then
builds the matching quaternion, preserving the established +X right convention.
The SDK documents a replacement up vector; the perturbation order is corroborated
by later LithTech, not by an observed 1998 executable. Zero/nonfinite directions
fail. Movement continues to use the same axis calculation and diagnostics.

## Original CBloodSplatFX behavior

Insertion order replaces the static raw linked list with a per-session vector.
Before creating/loading/drawing randomness for a new sprite, scan existing
sprites, including those shrinking. Distances are **squared**, strictly less than
100. On the eleventh nearby existing sprite, move the nearest encountered so far
to the requested position and discard the new effect. Ties keep the first. The
scan stops there even if a later entry is nearer. This permits eleven nearby
sprites; it is not a ten-element cap. Reuse changes no scale, rotation, resource,
alpha, animation, growth/termination clock or event identity of the old effect.
A retained request reports the reused identity separately from a new creation.

With no supplied name, choose sprites/blood1.spr through blood3.spr. Null and
explicit empty names are distinct; empty/missing SPR fails. Created flags are
VISIBLE | ROTATEABLESPRITE, initial scale is uniform, RGB is (.1,.1,.1), alpha 1.
The portable per-store RNG is independent of gameplay randomness; retail random
sequence parity is not claimed. Three independent axis draws are retained.

Growth starts with four seconds only when growth is nonzero. Cap frame delta at
.25 seconds and multiply each scale axis by Random(1, 1 + growth * delta * 30 *
delta), then subtract the capped delta. Keep the original **delta squared** and
allow the final positive-time callback to overshoot zero. No default lifetime is
invented; a finished pool persists.

Term sets shrinking and resets the clock, returning false to keep the effect
alive. Shrink callbacks first test clock >= 5; otherwise they multiply each axis
by Random(1 - growth * capped_delta * 30, 1), write alpha from the previous clock,
and add the **uncapped** frame duration. The tested explicit termination API does
not implement the original shared SFX-list eviction policy. World unload releases
immediately. The existing 1-ms CSpecialFX callback policy and independent SPR
animation clock apply.

## Bounds, rendering and inspection

Positions/scales obey finite engine bounds; descriptor scale is [0,1000], growth
[0,.1], rotation must be a unit quaternion, resource <=1000 bytes without NUL,
source label <=100 bytes. Reversed/nonfinite or >60-second update intervals fail.
These bounds cover the selected original caller and prevent negative shrink or
unbounded arithmetic. Failed descriptors are not retained as unbounded strings.
At most 256 active sprites and 64 request reports exist; capacity exhaustion is
explicit, never invented eviction. IDs survive clear/reload. Reuse is checked
before capacity, but descriptor validation always precedes mutation. Update errors
remain visible and fail the session step.

The shared pass uses actual external SPR/DTX frames, original surface rotation,
per-instance RGB/alpha and world depth. Its half-extents, sorting and fullbright
rasterization remain replacement presentation. No shader, dependency, platform
selection or commercial asset is changed. Snapshots/inspection never advance
clocks. The Sprites tab shows created/reused/failed counts, original timing,
position/scale, resource/frame and prepared-quad errors, including with hidden UI.

Blood-spurt particles, gib impact callers, generic/recursive gib templates, global
SFX-list eviction, complete save/restore and the remaining required M2 services
remain unavailable. This increment does not claim complete combat or M2 acceptance.

The [blood-spurt continuation](blood_spurt_contracts.md) extends the selected original weapon/fragment/limb callers and impact splats. Earlier blanket omissions are historical; separate flesh sparks, generic debris and M2 remain incomplete.
