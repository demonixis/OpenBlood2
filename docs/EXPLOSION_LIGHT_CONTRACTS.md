# Original explosion point-light contracts

This continues [explosion models](EXPLOSION_MODEL_CONTRACTS.md),
[sprites](EXPLOSION_SPRITE_CONTRACTS.md) and [scene emitters](SCENE_EXPLOSION_CONTRACTS.md).
See [source selections](M2_EXPLOSION_LIGHT_SOURCES.json) and
[evidence](../../Documentation/Implementation/M2_EXPLOSION_LIGHTS.md).
**M2 remains incomplete.** These are evaluated point lights, not rendered surface
illumination. The existing [CLightFX contract](LIGHT_CONTRACTS.md) remains separate:
explosion lights are detached and do not follow a server source or its visibility.

## Original lifecycle

CExplosionLightFX retains original Init/CreateObject/Update color/radius and delay
rules. Typed owned position, RGB and radius replace legacy client OT_LIGHT handles;
FLAG_VISIBLE is retained. There are no resource paths or server object IDs to resolve.
The original uninitialized false cases of m_bChangeColor and m_bScale are explicitly
initialized, including when an instance is reinitialized.

Creation sets color1. Radius is radius1 without delay, or zero with any positive
delay. During delay, color stays unchanged and the effect stays alive. At elapsed
**>= delay**, radius becomes radius1, start time becomes the current time and delay
becomes zero. That update returns immediately without color/radius interpolation.
Delay overshoot is discarded even when it exceeds the duration. Duration therefore
starts when the delayed update actually executes, not at the scheduled deadline.

After delay, color/radius interpolate linearly only when their endpoints differ.
The final update performs source interpolation before returning elapsed < duration.
Terminal overshoot is not clamped; the manager removes that light before inspection
or any future renderer consumes it. Unchanged endpoints remain unchanged. The
CSFXMgr 1 ms cadence and next = now + interval apply to waiting and active lights.
All advancement runs once per session step, outside ImGui, without another player
integration. Nonfinite/backward clocks fail and remove the affected light; errors
are retained and make the session step fail.

Descriptors require finite bounded positions, RGB channels in [0,1], radii in
[0,10000], duration in (0,600] and delay in [0,600]. These are explicit supported
engine bounds, not new gameplay clamps. Invalid descriptors or creation state fail.
These bounds cover all selected recipes; broader original callers are not claimed.

## Selected composite recipes

The same high-detail types as the model/sprite ports are routed from original
CClientExplosionSFX. Normal is checked and normalized; a zero normal fails.

| Types | Position offset along normal | Flash radius | Residual radius |
| --- | --- | --- | --- |
| 0 small / 6 napalm fireball | 10 | 150 to 0 | 50 to 0 |
| 1 medium | 10 | 200 to 0 | 75 to 0 |
| 2 large / 3 grenade | 10 | 250 to 0 | 100 to 0 |

Flash RGB goes (1,.5,0) to (1,1,1) over 1.25 s. The source's light named
“scorch” goes (.2,.2,.2) to (.5,.5,.5) over 25 s: it is a light, **not a decal**.
The source writes fRadius1 twice for this second light and leaves fRadius2 at zero
from the preceding descriptor. Preserve that shrinking radius; do not invent a
constant-radius correction. Both lights have zero delay.

Type 8 Tesla alternate creates one light, offset 30, radius 100 to 350, RGB
(.75,.75,1) to (.5,.5,.5), duration 3 s and zero delay. Other types, including 24,
fail explicitly. No extra RNG draws, quality menu or generic FX caller is added.

The model/sprite/light publications remain separate, owned local deliveries before
the existing null-damager radius call and sound. Light creation does not depend on
model/sprite/audio resource success, and source removal leaves lights alive.
Grouping by component preserves the selected deterministic light values, not the
full retail composite manager's random/global ordering. No damage behavior changes.

## Ownership, failure and inspection

The session owns at most 256 active lights and 64 retained delivery reports.
A report/event ID denotes a delivery attempt; it is not successful illumination.
IDs are never reused across unload. Reports own source names, created light IDs
and errors, without emitter pointers. Partial capacity failure preserves the first
created sibling. Failure, normal expiry and total starts count separately. Unload
clears values/reports/errors/counters, retaining only the monotonic identities.

Scene explosions shows each light's ID/event, position, elapsed time, duration,
pending delay state, update count, RGB swatch and exact radius/endpoints. Retained
reports survive emitter removal; missing components and unsupported illumination
stay explicit. This is diagnostic state, not world lighting. No shader or renderer
claims shadows, fog, surface illumination or scorch decals from these records.
Complete-composite availability therefore stays false even for type 6.

Synthetic tests cover delay boundaries/overshoot, constant and changing endpoints,
1 ms cadence, final interpolation, invalid state/clock/descriptors, capacities,
partial sibling creation, retained report bounds, non-reused IDs and independent
resource failures. Private tests exercise all nine SubwayA type-3 sources through
three player/world cycles: 18 lights, source removal, nine flashes expiring while
nine residuals survive, and teardown. Existing GPU application smoke checks the
same evolving lights while the UI is hidden and after reload, including the build
without ImGui. This proves evaluated state and inspector operation, not light pixels.

SubwayA counts remain 203 created / four transferred sounds / 372 editor-only /
one unsupported Gideon / zero failed sources. Other composite components,
illumination, safe automatic NEW_LEVEL and required services remain open.

Continuation: [original explosion rings](EXPLOSION_RING_CONTRACTS.md) now supplies
selected fire/smoke particle rings and alpha billboards. Earlier missing-ring statements
are historical; fragments/flames, illumination and the full M2 gate remain open.
