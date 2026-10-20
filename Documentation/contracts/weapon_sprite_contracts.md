# Original weapon impact flash and smoke

Selected original `CWeapon::UpdateWeaponFX`, `CWeaponFX::CreateFlash/CreateSmoke`
and `CSmokeImpactFX` lifecycle now execute. See [source ledger](../implementation/sources/m2_weapon_sprite_sources.json)
and [evidence](../implementation/reports/m2_weapon_sprites.md). This extends
[vector impacts](weapon_impact_contracts.md), [sprite presentation](sprite_view_contracts.md)
and [explosion sprites](explosion_sprite_contracts.md). **M2 remains incomplete.**

## Original selection and recipe

Only bullet/shell/BMG impacts whose raw surface is WOOD or STONE request these
sprites. BUILDING shares stone audio but not this visual branch. Knife, metal,
liquid and unknown surfaces do not acquire invented smoke/flash. Selection runs
before the existing material sound, using the existing owned per-weapon client RNG.
Missing marks and other original visual branches still consume no draws; complete
retail RNG ordering is not claimed. Gameplay shot/damage randomness remains separate.

The original client normal is normalized after the server AddImpact offset of
normal * 1.25. The destination query selects the first overlapping container and
only uses its code if USRFLG_VISIBLE is present. A hidden first result masks later
visible overlaps. Stable slot order is the existing replacement for unavailable
retail BSP ordering. The default footstep query continues to include hidden volumes.
In liquid (including original FREEFALL classification), the flash is suppressed
and smoke requests bubbles. That SparkFX service remains explicitly unavailable:
record a failed report and create neither a flash nor an incorrect underwater smoke.

On dry wood/stone, B2_LITE restricts variants to 1..3. CreateFlash selects
`Sprites/impact01..03.spr`, adds one normal unit, uses constant XY scale .1,
alpha 1, duration .15 s, fade/waveform/alignment zero and the already ported
CExplosionSpriteFX. CreateSmoke adds three normal units, starts at XY scale .1,
grows to .35 in one second, uses initial alpha .65 and linear fade, and selects
`Sprites/smokepuff1..3.spr`. It draws vertical gravity .05.. .1 before selecting
the smoke resource. Wood RGB is (1,.8,.6); stone keeps the created white color.
Motion is enabled only for normalized normal.y > -.3. Zero/invalid normals fail.

## CSmokeImpactFX lifecycle

Owned typed settings replace legacy client objects and string pointers. Preserve:

- Creation sets position/scale only; actual color is white/opaque until Update.
- The first active update starts the lifetime, even when creation happened earlier.
- Start-delay zero is the original sentinel: an update at time zero can rearm it.
  The tick that finishes a delay still returns hidden; the next callback starts.
- Scale change tests only X/Y, ignoring the supplied update-scale flag. The rotate
  flag is stored but unused. Componentwise upper clamps preserve shrinking quirks.
- Each eligible callback adds deceleration to velocity, then velocity and gravity
  to position. These are per-callback additions, never multiplied by frame seconds.
- Fade uses the initial alpha, lower-clamped to zero. Final motion/scale/fade run
  before the strict lifetime return removes the effect.

The existing 1 ms CSpecialFX scheduling policy wraps those original updates;
SPR animation advances independently with elapsed time. Nonfinite/reversed clocks
fail. Positions/velocities/scales obey engine finite bounds; scales are nonnegative,
color/alpha are [0,1], duration is (0,600], delay [0,600] and resource length <=1000
without embedded NUL. Nonpositive/infinite original effects are outside this bounded
subset. Null sprite name uses smokepuff1; an authored empty name fails explicitly.

## Ownership, renderer and inspection

The adapter owns up to 256 combined flashes/smoke and the last 64 reports. Effects
and reports use non-reused IDs across unload. An event ID means a retained report,
not successful creation. A failed flash preserves a successfully created smoke and
vice versa; failures never suppress original weapon damage, audio or hearing.
Source/weapon removal does not kill detached visuals. Session steps advance them
outside UI; unload clears them before resource stores. Update failures remain
inspectable and return failure from the session. No mutable original global is
introduced; destination rehearsals have their own effect store.

The existing depth-tested straight-alpha sprite pass now accepts per-instance RGB,
using its existing vertex tint path. It multiplies decoded DTX color and alpha by
client effect state. No new shader or dependency is needed. Snapshots only read
clocks and transforms. Missing current DTX frames remain explicit presentation errors,
even if SPR creation succeeded. Flashes/smoke have their own snapshot ID namespace.
The Sprites inspector shows current positions, scales, tint/alpha, callback counts,
first-update/delay state, resource/decoded frame, prepared quads and retained failures.
Player/AI impact inspection links its report ID to those details.

Marks/decals, sparks, wood fragments, muzzle smoke, bubbles and remaining composite
components are still unavailable. Current fullbright rasterization is replacement
presentation, not retail parity. User playtest issues (mouse look/fire, NPC attack
activation, missing train glass) take priority over additional FX work. Safe
NEW_LEVEL, Gideon and other required services still keep the M2 gate open.
