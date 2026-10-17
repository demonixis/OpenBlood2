# Original vector impacts and material audio

Selected `CWeapon::FireVector`, `AddImpact` and `CWeaponFX::GetImpactSound` /
`PlayImpactSound` now execute for Beretta/knife. See [source ledger](../implementation/sources/m2_weapon_impact_sources.json)
and [validation](../../../Documentation/Implementation/m2_weapon_impacts.md). This extends
[weapon](weapon_contracts.md), [fire audio](weapon_audio_contracts.md) and
[AI hearing](ai_trigger_contracts.md) contracts. **M2 remains incomplete.**

## Source ordering and surfaces

An accepted shot uses one damage/spread sample and one ammunition deduction for
its entire vector. Each continued contact advances one unit along the normalized
shot direction, retaining the original endpoint/range. Source branch order matters:

| Contact | Damage / hearing | Continue |
| --- | --- | --- |
| Any VolumeBrush | No damage or impact alert; Water/Blood/Acid request impact effects | Always |
| Primary world except FIRETHROUGH | Impact effect then radius-200 impact alert | Glass and non-melee only |
| Destructible brush with mark-eligible ammo (bullet/shell/BMG) | FireThrough skips everything; otherwise effect, alert, damage | FireThrough or resolved glass |
| Door | FireThrough skips everything; otherwise effect, alert, damage | FireThrough only |
| Other | Effect, damage, then alert unless CBaseCharacter | Never |

This preserves two non-obvious source behaviors. Primary-world FIRETHROUGH falls
through into the general branch and **stops**; it is not an invisible surface.
Knife ammo is not mark-eligible, so a destructible brush takes the general branch
even when FireThrough is set. Door FireThrough applies to the knife as well.

Surface audio uses the raw object user-flag high byte or baked polygon texture
flags. The brush-specific `GetSurfaceType` determines continuation, but the source
already captured the effect surface before that fallback. Keep both values visible.
Default liquid brushes set no LIQUID surface byte: their impact request can correctly
produce no audio. Do not invent a water material. New CBaseCharacter initialization
publishes its original default FLESH high byte while preserving existing lower flags;
this does not implement the other original vision flags. Saved engine user flags
remain authoritative on restore. Primary world has no damage aggregate or original
actor sentinel in this port, so a general world contact records no damage delivery.

## Selected client impact sound

The original mapping covers stone/building, metal, wood, energy, glass, terrain,
cloth, plastic, flesh and liquid/water. Unknown/sky/FIRETHROUGH select no resource
and consume no random draw. The source's unconditional B2_LITE means `NRES(4)=3`:
variants are **impact1..3**, regardless of the supplied ammo type. An owned string
replaces mutation of an original string literal. Portable per-weapon client-effect
randomness is separate from shot/game RNG, seeded at one; CRT/global RNG parity is
not claimed. Missing visual branches' random draws are not reproduced.

`AddImpact` offsets playback by normal * 1.25. Existing typed WAV/SDL services play
one detached spatial voice with outer/inner radii 500/50, full volume and priority
MISC_LOW (0). Reverb remains requested but dry; priority arbitration is unavailable.
A missing resource records failure without suppressing damage, continuation or
hearing. Intentional silence has no voice and is not counted as playback. Detached
voices survive source/weapon removal, expire by sample completion and clear on unload.

## Engine and lifetime adaptations

Weapon rays include SOLID or RAYHIT objects. Eye alignment filters every VolumeBrush;
the final shot does not. The new typed `SegmentStart::SkipInterior` avoids repeatedly
hitting an enclosing volume or a thick brush after entry. Default query behavior for
other callers is unchanged; USE retains its existing entry-only policy. This bounded
replacement engine policy is informed by a later LithTech engine generation, not
verified retail physics. SegmentHit now retains primary-world texture flags.

At most 64 contacts are applied. Exactly 64 followed by a miss succeeds; a 65th
contact fails explicitly. Already applied effects/ammo cannot be rolled back.
Queries fail explicitly; forward progress and original range remain checked. Existing
collision preflight still runs before ammo use, and gunfire-hearing callbacks still
refresh the final ray. Impact callbacks recheck the shooter and target before damage;
removed shooters interrupt, removed targets are recorded without stale delivery.
General-case hearing runs after damage, so removal there must also be respected.
Deferred object destruction and existing weapon reentrancy guards remain mandatory.
All added state is owned by the weapon/adapter; no mutable game global is added.

## Inspection and remaining work

Player and AI weapon inspection shows ordered last-shot contacts, raw/stopping
material, branch, historical target, position, damage delivery, continuation, selected
resource, resolved voice and separate hearing/audio errors. Cumulative counts remain
visible for unselected player weapons. Historical handles/IDs are resolved each frame;
inspection never advances game/audio state. Last-shot collision now describes the
last query, which may be a miss after passing glass; the ordered contact list retains
prior hits, and `damage_delivered` means any delivery, not confirmed health loss.

This is selected impact **audio and gameplay**, not complete WeaponFX. Sparks, blood,
smoke, liquid colors/splashes, tracers, decals and their source random ordering remain
unavailable. Other weapons, retail parity, required world services, Gideon and safe
NEW_LEVEL still keep the actual M2 exit gate open.

Continuation: [weapon impact flash/smoke](weapon_sprite_contracts.md) supersedes the earlier blanket impact-visual omission for wood/stone bullet sprites only. Other composite components and M2 remain open.

The [blood-spurt continuation](blood_spurt_contracts.md) extends the selected original weapon/fragment/limb callers and impact splats. Earlier blanket omissions are historical; separate flesh sparks, generic debris and M2 remain incomplete.

The [original impact sparks](spark_contracts.md) continuation supersedes the flesh/metal spark omission only. Underwater bubbles, generic recipes and remaining M2 services stay unavailable.
