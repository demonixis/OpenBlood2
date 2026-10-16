# Original explosion sprite contracts

Continuation: [explosion point lights](EXPLOSION_LIGHT_CONTRACTS.md) now evaluate
selected original transient colors/radii with independent lifetime. Earlier light
omissions are superseded only for that state; surface illumination stays unavailable.

Continuation: [original explosion models](EXPLOSION_MODEL_CONTRACTS.md) now supply
selected model components with independent lifetime and alpha rendering. The earlier
model omission is superseded only for that subset; complete composite FX remain unavailable.

M2 remains incomplete. This extends [scene emitters](SCENE_EXPLOSION_CONTRACTS.md)
and [sprite presentation](SPRITE_VIEW_CONTRACTS.md). See
[evidence](../../Documentation/Implementation/M2_EXPLOSION_SPRITES.md) and
[source selections](M2_EXPLOSION_SPRITE_SOURCES.json).

## Original behavior and supported routing

The selected CExplosionSpriteFX Init/CreateObject/Update code retains the original
scale interpolation (linear, square root, square), six alpha modes, duration and
final update before removal. Other waveform bytes retain the original linear
fallback. Fade 2 starts at one on its first update, fade 3 at zero; creation itself
uses the authored alpha. Fade 5 changes branch at the exact half duration. An
overshooting terminal update is not clamped: it is removed before presentation.
The originally indeterminate false case of m_bScale is initialized explicitly.

CClientExplosionSFX publishes typed client effects before its original null-damager
radius call and detached sound. Its cached initial position, command/retrigger
rules and deferred removal are unchanged. No damage is invented. A selected
CExplosionFX detail >= 2 recipe normalizes the direction and creates these sprites:

| Type | Sprite | Normal offset | XY scale | Duration | Alpha / waveform / alignment |
| --- | --- | --- | --- | --- | --- |
| 0 small | fireball2 | 25 | .25 constant | 1.5 s | 1 / linear / camera |
| 1 medium | fireball2 | 25 | .5 constant | 1.5 s | 1 / linear / camera |
| 2 large | fireball2 | 25 | .75 constant | 1.5 s | 1 / linear / camera |
| 3 grenade | fireball2 | 25 | .5 constant | 1.5 s | 1 / linear / camera |
| 0 small | smokepuff2 | 25 | .25 to 1.5 | 5 s | .85 / square root / camera |
| 1 medium | smokepuff2 | 25 | .5 to 2 | 5 s | .85 / square root / camera |
| 2 large / 3 grenade | smokepuff2 | 25 | .5 to 2.5 | 5 s | .85 / square root / camera |
| 6 napalm fireball | debrisimp | 0 | .25 constant | .5 s | 1 / linear / normal |
| 6 napalm fireball | smokepuff2 | 20 | .25 to 1.5 | 5 s | .85 / square root / camera |
| 8 Tesla alternate | TesAltLoop | 30 | .1 to .75 | 3 s | .75 / linear / camera |

Paths are original `Sprites\*.spr`, all recipe Z scales are zero and fade mode is
1. These are deterministic sprite branches, not the full CExplosionFX switch or
a quality-setting implementation. Selected model branches now use their own
documented client RNG stream; unimplemented particle/fragment branches
consume no random draws. Full composite random ordering is not claimed.
Other preset types fail explicitly, including type 24 RIFT_2 (which has no sprite
branch here). Brush ExplosionEffect, weapon and other FX callers are not routed.
The complete-composite availability flag remains false even after sprite success.

## Ownership, time and bounded failures

Typed owned settings replace legacy client strings and visual handles. A null
resource selects the original Explosn.spr default; an authored empty string fails.
Position/scales use existing finite physics bounds, scales are nonnegative,
duration is (0,600] seconds, alpha is [0,1], fade is 0..5 and resource paths are at
most 1,000 bytes without embedded NUL. Alignment 0 faces the camera, alignment 1
uses the shared +Y-up forward convention (deterministic vertical fallback).
Random alignment 2 is explicitly unsupported. Zero recipe normals fail before
normalization. SPR loading checks manifests/resources; actual DTX decoding remains
a separate current-frame presentation operation and can still fail visibly.

Client visual IDs and event IDs are non-reused across world unload. An event ID
means a retained delivery report, not successful FX. Creation failures preserve
successful siblings as original independent CreateSFX calls do. At most 256 sprites
are active and the last 64 reports are retained with resource/capacity errors and
missing component lists. Reports own strings and IDs, never source pointers.

The session advances visual state once after object callbacks, outside ImGui.
SPR frame clocks use elapsed simulation time; effect updates follow the original
CSFXMgr minimum 1 ms interval and next = current time + interval rule. Creation
starts from the current level clock. Nonfinite/backward clocks and reader limits
fail; runtime update failure increments the failure count and fails the session
step. Expiry increments a separate counter. Source removal does not kill effects.
World unload clears effects and reports before resource reuse; no sprite advances
from a rendering or inspection callback and no player integration is added.

## Rendering and inspection

Client sprites join the existing SpriteView depth order, resource cache and shared
world/model depth pass. SpriteAlpha multiplies actual DTX alpha by the per-instance
alpha via a fragment uniform; opaque world/model pipelines remain unchanged.
There are no new tint, additive, illumination, particle or shadow services.
Existing sprite half-extents, disabled chromakey, mode errors and Vulkan limits
still apply. Expired effects cannot submit terminal negative/overshooting alpha.

The Scene explosions inspector shows frame/time, scale, alpha, resource path,
decoded dimensions, prepared geometry or resource failure, lifetime totals and
retained deliveries after emitter removal. Prepared geometry is not proof of a
visible on-screen pixel. GPU smoke verifies hidden/reloaded submissions; pixel
readback separately verifies opacity multiplication and depth/blending behavior.

SubwayA has nine supported type-3 emitters, each creating fire and smoke. Exercising all
nine produces 18 client sprites plus 12 DetailSprites, with checked resource
preparation across three load/player/unload cycles. This does not add source-world
objects: counts remain 203 created, 4 transferred sounds, 372 editor-only, one
unsupported Gideon and zero failed sources. Safe automatic NEW_LEVEL, remaining
required FX/services and the actual M2 acceptance criteria remain open.

Continuation: [original explosion rings](EXPLOSION_RING_CONTRACTS.md) now supplies
selected fire/smoke particle rings and alpha billboards. Earlier missing-ring statements
are historical; fragments/flames, illumination and the full M2 gate remain open.

Continuation: [original explosion flame particles](EXPLOSION_FLAME_CONTRACTS.md)
now implements the selected Tesla component. Earlier blanket flame omissions are
historical for that recipe; surface illumination and the actual M2 gate remain open.

Continuation: [weapon impact flash/smoke](WEAPON_SPRITE_CONTRACTS.md) supersedes the earlier blanket impact-visual omission for wood/stone bullet sprites only. Other composite components and M2 remain open.
