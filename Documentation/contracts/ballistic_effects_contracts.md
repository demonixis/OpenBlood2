# Original shared ballistic effects — lot 4

2026-10-25. Extends the [shotgun effect store](shotgun_contracts.md#original-vfx-completion-lot-2-2026-10-25)
to original bullet and BMG callers. The historical `ShotgunEffects`/`PublishShotgunEffects`
names remain internal API names; their settings now carry the ammo type.
See [source hashes](../implementation/sources/ballistic_effect_sources.json) and
[verification](../implementation/reports/ballistic_effects.md).

## Original selection

- `CWeapon::BeginFire` arms the one-shot ejection bit for BULLET/SHELL ammo. Shotgun's
  override keeps alternate-only ejection. `UpdateWeaponFX` consumption still precedes
  impact suppression: a fire-through/non-liquid-volume first contact consumes the bit.
  A complete miss sends no composite. Subsequent contacts/pellets cannot eject again.
- Beretta, MAC-10 and M16 primary use the shared bullet branch: one `bshell.abc` /
  `bshell.dtx` casing at scale 1.5. Shotgun keeps two `sshell` casings at 2.25. BMG
  does not arm ejection and the original casing selector has no BMG model. Left-hand
  ejection mirrors the side velocity. Existing lifetime, gravity, bounces and sounds stay.
- Bullet marks use scale .05; shell/BMG marks use .035. Original material selection,
  mark absence on glass/sky/flesh/unknown, wood/stone fragments, liquid splashes/ripples
  and first-contact muzzle smoke are shared. Bullet/BMG smoke uses 1.5 seconds, alpha .5,
  scale .05 to .25, forward velocity 2 and shot-direction deceleration -.035.
  Shell smoke keeps its existing recipe. Underwater substitutions produce four bubbles
  for bullet/BMG and one for shell; existing shared impact sprites do not double-emit.
- Minigun's original empty `AddImpact` is preserved: no casings, tracers, marks or
  impact composite are added. M16 alternate still fires its grenade; sniper alternate
  still zooms. Damage, spread, ammo costs, save payloads and collision rules do not change.
- View weapons now load the authored primary/alternate muzzle sprites for every
  definition, including non-ballistic weapons with an authored flash. Empty paths remain
  empty. Flash scale/duration come from the definition; dual hands and the existing
  desktop muzzle-node projection / tracked XR placement are preserved. Model-key audio
  outside shotgun and other unported weapon animations are not included.

## Tracers

Original `WeaponFX::CreateTracer` supplies the **impact-normal rotation** to `CTracerFX`.
Preserve that quirk: cast from source + normal × 50 along the normal, not along the
bullet. The checked primary-world ray must hit; distance from source minus 50 must be
at least 50. Place the line centre at ray origin + normal × (50 + random offset), with
offset in 0..distance. Do not clamp it to the impact or invent a streak on every shot.

The three hardware lines have endpoints at ±50 and local X offsets 0, +.05 and -.05.
Their endpoint colours/alphas are (1,.8,0,.1) and (1,0,0,.9). They remain stationary
(the original never applies its velocity) until the strict `now > start + .1` expiry.
Publication substitutes the original first client update; cosmetic randomness is owned
separately from damage. The bounded pool keeps 64 tracers, evicting the oldest. Unload,
load and world reset clear it; no object pointer or saved record is retained.

The existing XR line renderer now lives in `engine/line_renderer.*` and serves both
callers. XR overlay defaults are unchanged. Ballistic lines use alpha blending, scene
depth testing without depth writes, desktop letterbox/FOV or each eye's asymmetric
projection, and render before view models. They follow scene light-scale/fade. The two
new auxiliary fragment variants attenuate background glow and the AO mask like sprites,
with depth-copy/reflection writes masked. MSAA follows the scene target. All 72 shader
artifacts were regenerated together; the previous 68 remain byte-identical.

## Boundaries

The lot-2 fixed marks without polygon clipping, primary-world fragment/casing collision,
separate bounded pools and camera-origin player firing remain explicit adaptations.
Live realtime lighting is not supplied by a sprite or line. Shader/dependency pins,
commercial assets and the original reference sources remain unchanged. M2 stays closed
with M2-GPU-001; this does not certify M3 traversal or pixel-identical retail presentation.
