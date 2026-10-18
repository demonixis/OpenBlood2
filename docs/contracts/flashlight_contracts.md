# Original flashlight and bounded point illumination

Read the [source ledger](../implementation/sources/m2_flashlight_sources.json) and
[implementation evidence](../../../Documentation/Implementation/m2_flashlight.md).
This extends [inventory](inventory_state_contracts.md),
[medkit items](medkit_contracts.md), [input](player_control_contracts.md),
[rendering](world_view_contracts.md) and [shader packaging](metal_shader_contracts.md).
M2 remains incomplete.

## Original rules

`FlashlightPU` inherits `ItemPickupCharged`: original item ID 2, quantity 1,
resource 301, Flashlight_pu model/skin and inventory1.wav pickup request. The
existing checked touch/acknowledgement path rejects dead players and full charge,
and defers removal. Only authored single-player creation is supported; historical
TypeAction and string-created bounce behavior remain unavailable.

`CInvFlashlight` starts at 100 displayed charges and 120 seconds. Recharge ignores
the incoming quantity and requires the rounded count to be below 100. Activation
creates the FORCECLIENTUPDATE normal child before testing for an empty battery.
It toggles user-visible bit 1, sends the original item action, and retains the
child while off. Both toggle directions play the original local flashlight.wav
through the existing bounded client queue/audio service. Playback/queue failures
remain visible and do not undo the original toggle. Pickup audio is still only a
request. PCX HUD/localized item-name display remain unavailable.

The inventory aggregate updates the child from the owner's **body** position and
the player's internal pitch/yaw (ordinary owners use their object rotation).
Active time decreases by simulation delta; exhaustion sets count zero and turns
the item off. Positive charge displays `byte(100 * remaining / 120 + .5)`.
Do not add a camera-eye offset, recharge while rounded to 100, or consume battery
while rendering. Invalid owner/pose/child/time fails explicitly; the session
propagates an unsuccessful item update. The child is removed by owned teardown,
including pending owner removal, and checked identities suppress stale lighting.

L is the application binding for the original direct-item command. It is a
press edge and obeys text editing, focus, F2, held-key suppression and player
replacement. Original dead/movement/camera/run gates apply; release Shift to
toggle. No default flashlight is granted to the player.

## Client light and renderer

The selected `CFlashlightFX::Update` casts along the child's forward vector.
A hit places the light at the hit point, clamps distance to at least 10, and
sets radius to distance * .4. The source accidentally queries point shade at
`body - hit`, which is retained as inspectable query metadata. GetPointShade
remains unavailable: the original explicit failure fallback supplies minimum
intensity 132/255. Intensity is `max(minimum, (1000 - distance/2) / 1000)`.
A miss, hidden source or removed source/owner renders no light. Invalid or
unavailable collision fails and hides the effect.

One owned `PointLight` value replaces the client OT_LIGHT object. The existing
bounded ray service excludes the authoritative local player body, which is absent
from first-person client presentation, and tests its supported solid world/object
subset. This is an explicit replacement collision policy, not complete retail
client intersection equivalence. No fake successful point-shade service is added.

World and world-model opaque/translucent passes receive a single point light.
The replacement shader adds radial brightening to the existing unlit base:
`baseRGB * (1 + color * max(0, 1 - distance/radius))`. It does not supply normals,
lightmaps, shadows, occlusion, general dynamic-light rendering or retail lighting
parity. A zero radius disables it. Sky, sprite/FX and near view-weapon passes
retain their existing illumination; alpha/depth policy remains unchanged.

The shared fragment uniform is three float4 values (48 bytes): modulation,
world position/radius and RGB color. Both fragments use uniform slot 0 and
sampled texture/sampler 0; the vertex stage passes current posed world positions.
All six SPIR-V/MSL artifacts must be regenerated together through the existing
pinned ShaderCross toolchain. No new dependency or runtime compiler is added.

## Ownership, preservation and inspection

InventoryState owns optional byte count and finite remaining time in [-60,120].
The lower bound allows source exhaustion overshoot under the engine's checked
frame limit. Save rejects malformed state without changing its output. Staged
Restore and KeepAlive recreate an independent hidden child and bind the item to
the new owner. Original InvItem::Load forces inactive in both modes; no previous
handle or active light is transferred. Failure leaves existing inventory intact.
All source/destination objects continue to be destroyed under their own adapter.

Pickups inspection shows presence, battery/time, toggle state, live child,
ray result, hit distance/position, radius/intensity and service errors, even when
the effect is hidden. Restoration inspection shows saved charge and the inactive
load rule. Reading inspection or rendering does not advance clocks.

CityHub's FlashlightPU is now supported. Its separate missing `fallengrills`
world-model binding still fails and blocks automatic destination promotion.
Class counts are not proof of complete M2 services or a full-level playthrough.

The [baked lighting continuation](baked_lighting_contracts.md) renders baked per-vertex world light and model light. The radial light is now added to that face light before the albedo multiply, `albedo * (faceColor + color * radial)`, as later LithTech adds dynamic lights to baked light. With an unlit face (1) the result equals the former formula. It still has no normals, shadows or occlusion.

On lightmapped faces the [lightmap continuation](lightmap_contracts.md) adds the radial light to the lightmap before the Saturate scale, so the result is `albedo * 2 * min(lightmap + light, 1)`.
