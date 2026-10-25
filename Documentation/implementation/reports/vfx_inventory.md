# Remaining original VFX — lot 3 inventory

2026-10-25, OpenBlood2 baseline `42e737e9` (destruction lot 1 and shotgun lot 2 included).
This is a source audit, not a new visual playtest or an implementation lot.
The [roadmap](../../roadmap.md#original-vfx-completion-beta-feedback-m37--m4)
owns the proposed delivery order. M2 remains closed with M2-GPU-001; M3 campaign
acceptance and the unreproduced Museum crash are unchanged.

## Method and scope

Compared the original Blood2 server callers (`Weapon`, `GameWeapons`,
`GameProjectiles`, scene emitters) with `ClientShellDLL` recipes and the current
publishers/render consumers. The [source ledger](../sources/vfx_inventory_sources.json)
pins the original files. Current implementation paths below are relative to the
repository root; original paths are relative to `Blood2/`.

- **Missing** means an active original caller/recipe has no corresponding ported
  component or is explicitly recorded as unavailable. Working damage or sound
  does not establish that its visual effect exists.
- **Partial** means some components render; the row identifies what is missing.
- **Present** means a connected implementation exists, not pixel-identical retail
  certification. Existing contracts retain their collision, capacity, timing and
  rendering adaptations.
- **Conditional** means the original contains the feature, but campaign usage or
  the relevant variant has not been established. An SFX ID or an absent server
  class alone does not justify a campaign defect.

The explosion comparison uses the original high-detail recipes. Low/medium-detail
recipe parity, all authored placements, external mod content, multiplayer-only
presentation and `_ADD_ON` effects are not certified here. Asset existence for
unported recipes must be checked when implementing them; this audit does not
reclassify an unsupported recipe as a missing commercial resource.

## Weapons: confirmed gaps and existing coverage

| Family | Current coverage and remaining original effect | Priority / evidence |
| --- | --- | --- |
| Shared first-person muzzle flashes | **Missing outside shotgun.** `CViewWeapon` loads and shows flash sprites only for `WEAP_SHOTGUN`, although other weapon definitions name primary/alternate flash assets. Port each authored branch, including dual hands, rather than attach a generic flash to every weapon. | **P1.** Original `ClientShellDLL/ViewWeapon.cpp` / current `src/game/original/ViewWeapon.cpp`, `WeaponDefs.cpp`. |
| Beretta, MAC-10, sniper rifle, M16 primary and other ordinary bullet callers | **Partial.** Material impact audio, wood/stone flash/smoke, selected sparks and blood effects exist. Marks, material fragments, casings, muzzle smoke, bullet tracers and liquid splash/ripple/bubble components are not connected as a shared path. Lot 2 implements the shell-specific subset only. Ammo/material/hand flags and first-contact suppression must remain original. | **P1.** Original `ObjectDLL/Weapon.cpp::UpdateWeaponFX/Fire`, `ClientShellDLL/WeaponFX.cpp`; current `WeaponVector.cpp`, `WeaponFX.cpp`, `src/game/shotgun_effects.cpp`. |
| Sawed-off shotgun, both modes | **Present with explicit limits.** Lot 2 has flash, barrel/reload animation, secondary-only two casings and muzzle smoke, marks, wood/stone fragments and liquid effects. Marks are not polygon-clipped; fragment/casing collision and pools retain replacement boundaries. General light illumination is still missing. | Preserve the [lot 2 contract](../../contracts/shotgun_contracts.md#original-vfx-completion-lot-2-2026-10-25); do not restart this lot. |
| Minigun | First-person flash remains in the shared flash gap. **Do not add ordinary impact/tracer effects:** original `CMiniGun::AddImpact` is empty and the port deliberately suppresses that path. | **P1** flash only; original `ObjectDLL/GameWeapons.cpp`, current `WeaponVector.cpp`. |
| Flare gun primary / alternate / burst children | **Partial.** Projectile sprites, tracking, secondary splitting and sound exist. `OBJFX_SPARKS_1` and `OBJFX_SPARKS_3`, impact/fizzle/burst recipes **13–15** and owner lights are missing. The alternate parent's `OBJFX_SPARKS_2` assignment is commented out in the original: preserve that absence. | **P1.** Original `GameProjectiles.cpp`, `ExplosionFX.cpp`, `ObjectFX.cpp`; current `FlareProjectile.cpp` and recipe publishers. |
| Tesla primary / alternate | **Partial.** Flying projectile, sounds and alternate composite **8** exist. Primary impact **7**, alternate shot beam and ball-to-target arcs are missing; both `CTeslaCannon` and `CTeslaBallProjectile` count unavailable lightning. Projectile illumination is missing. | **P1.** Current `WeaponProjectile.cpp`, `TeslaProjectile.cpp`; original `GameWeapons.cpp`, `GameProjectiles.cpp`. Existing world lightning is a reusable backend, not proof these callers are connected. |
| M16 grenade, proximity/remote/time bombs | Default/grenade composites **0–3** have connected components, including grenade fragments/trails. Time bomb uses **1**. General light illumination remains absent. | Preserve existing work; include these in regression checks when changing shared explosion services. Current `explosion_*.cpp`, `CampaignProjectiles.cpp`. |
| Napalm primary / alternate | **Partial.** Primary **4** has its three model layers, sprites, ring and evaluated lights; fireball **6** has its authored sprites/light states. Alternate impact **5** is missing despite working spawned fireballs. `OBJFX_SMOKETRAIL_2` and projectile illumination remain unavailable. | **P2.** `NapalmProjectile.*`, `CampaignProjectiles.cpp`, `ExplosionModels::publish` (its special type-4 branch is outside the single-model selector). |
| Howitzer primary / alternate / mini-fragments | **Partial.** Projectile behavior, secondary child creation and sounds exist, but composites **9–11** are unsupported. The alternate fragments explicitly lack their `OBJFX_SMOKETRAIL_1` connection even though that recipe exists for other callers; projectile lights remain missing. | **P2.** `CampaignProjectiles.*`, original `GameProjectiles.cpp` and `ExplosionFX.cpp`. |
| Singularity | **Partial.** The blue firing beam and attraction/damage run. Impact/hole composite **18** and `OBJFX_SINGULARITY_1` on swallowed objects are missing. Recipes 2/3 belong to the separate original alternate projectile path; do not count their absence as a second confirmed primary defect. | **P2.** `WeaponProjectile.cpp`, `SingularityProjectile.cpp`, original `GameProjectiles.cpp`. |
| Death Ray | **Partial.** Primary green beam and reflections exist; impact **25** does not. Alternate rotating rings `OBJFX_DEATHRAY_RING_1/2` and target `OBJFX_ELECTRIC_1` are unavailable while area damage runs. | **P2.** `CampaignWeapons.cpp`, `CampaignProjectiles.cpp`, `ObjectParticleFX.cpp`. |
| Voodoo, Orb, Life Leech, BugSpray | Connected Voodoo **12–17** particles; Orb **33/34** trails and **21** break fragments; Leech fire trail and **16/17** impact components; BugSpray **31/32**, **26/27** particles/rings. These are **present**, with the common illumination and existing contract limitations. Do not port them again based on old progress reports. | Visual regression coverage, not a new missing-family lot. `ObjectParticleFX.cpp`, `OrbProjectile.cpp`, `LeechProjectile.cpp`, `BugSprayProjectile.cpp`. |
| Melee | Shared material sounds and blood/sparks exist. No evidence here warrants inventing a muzzle sprite or projectile trail. | Preserve original ammo/surface selection; general view-weapon animation fidelity is a separate audit. |

Reload/model-key audio outside the shotgun is still indexed/request-only in
`ViewWeapon`; hide/show-node keys and weapon motion are adjacent presentation work,
not counted as completed VFX by this inventory. Likewise, missing local/NPC muzzle
illumination belongs to the renderer item below, independently of visible sprites.

## Enemies and environment

| Family | Current coverage and remaining original effect | Priority / evidence |
| --- | --- | --- |
| Drudge lightning | Damage runs, but the boxed/thin/high-segment beam is explicitly unavailable. Can share the Tesla lightning connection work while preserving its own parameters and damage ray. | **P1** with Tesla. `CampaignWeapons.cpp::CDrudgeLightning::FireVector`; original `GameWeapons.cpp`. |
| Enemy energy / ground attacks | `CEnergyBlastProjectile` lacks `OBJFX_FIRETRAIL_2`; `CGroundStrikeProjectile` lacks `OBJFX_GROUNDFLAME_2`; the scaled BugSpray trail used by `CShikariLoogieProjectile` is also unconnected. **7/12/19** enemy impact recipes are missing. Existing Divine **22** and large shockball **20** components must be retained. | **P2.** `Projectile.cpp`, `CampaignProjectiles.cpp`, `TeslaProjectile.h`, original `GameProjectiles.cpp`, `ExplosionFX.cpp`. |
| Ancient One / Undead Gideon | Ancient One's red beam and primary napalm impact **4**, and Undead Gideon's vomit fire trail **3**, are connected. The vomit intentionally has no explosion effect. | Preserve the [Ancient One](../../contracts/ancient_one_contracts.md) and [Gideon weapons](../../contracts/gideon_weapons_contracts.md) contracts. `AncientWeapons.cpp`, `CampaignProjectiles.cpp`. |
| Naga | Eye laser renders but endpoint composite **28** does not; spike **29**, falling-rock **30** and ground-pound **31** composites are missing. Falling rock models/damage already exist. The original debris screen-shake request is recorded unavailable. | **P2.** `NagaEyes.cpp`, `CampaignProjectiles.cpp`; original `GameWeapons.cpp`, `NagaCeilingDebris.cpp`. Screen shake needs a separate desktop/VR comfort decision. |
| Spawner rift | **Missing:** `CreateRiftEffect` increments `rift_unavailable` instead of showing **EXP_RIFT_1 (23)**. The actual spawn is independent and already works. | **P2.** `Spawner.cpp::SpawnObject` and original `ObjectDLL/Spawner.cpp`; distinct from a working WarpGate. |
| Destruction debris | Wood/metal/glass now render, including custom model/skin pairs. **Stone, plastic and terrain** original CGibFX branches remain unsupported; the CDebris flesh route is also outside this pool despite existing flesh effects on character callers. Do not fabricate debris for surface kinds without an original branch. | **P2.** `src/game/flesh_gibs.cpp`, `original/Debris.cpp`; original `ClientShellDLL/gibfx.cpp`. Keep [lot 1 boundaries](../../contracts/destruction_debris_contracts.md). |
| Scene explosions | `CClientExplosionSFX` calls all six component publishers. Default **0–3** are connected; unsupported authored types remain missing according to the recipe table below. A generic unsupported component diagnostic can also describe a component the original recipe never creates; audit by recipe, not by error count alone. | **P2**, shared with explosive weapons. `ClientExplosionSFX.cpp`, `src/game/explosion_*.cpp`. |
| Sparks, ParticleSystem, ParticleStream, WarpGate, TripLaser, PolyGrid and world lightning | Connected systems already exist. WarpGate is not the missing Spawner rift. World lightning particles work, although its lights do not illuminate geometry. Existing smoke/burning-corpse, blood spurt/splat and flesh-gib paths must be preserved. | Present with existing contracts; future comparisons should verify authored placement and appearance rather than label the whole subsystem missing. |
| Rain and generic legacy emitters | **Conditional.** Original `Rain.cpp` publishes `SFX_RAIN_ID`; no Rain class/client rain renderer is ported. Generic `CClientSplashSFX`, `CClientTracer`, `CClientCastLineSFX` also lack registrations. The current class inventory reports these; an exercised mounted-campaign instance has not been established here. Selected replacement publishers do not implement every generic class. | **P3**, promote with a concrete level/caller. [Class coverage](../../contracts/class_coverage.md). |
| Pickup rotation | Original `SFX_POWERUP_ID` drives `CPickupObjectFX` rotation when `USRFLG_PICKUPOBJ_ROTATE` is set. That presentation path is absent; stock single-player usage is not established by this audit and current save support refuses rotation state. | **P3 / conditional.** Original `PickupObject.cpp`, `PickupObjectFX.cpp`; current `PickupSave.cpp`. This is not a missing universal pickup particle halo. |
| Realtime lights, underwater fog, translucent ordering, clipped marks | **Partial rendering infrastructure.** Explosion/world-lightning/ClientLightFX light states are evaluated, but are not general scene/model illumination. Flashlight radial illumination and static model lighting already exist. Underwater fog, global translucent ordering and polygon-clipped marks remain known gaps. | Dedicated **M4** work, larger than adding recipes. `scene_presenter.cpp`, [lighting](../../contracts/lightning_contracts.md), [shotgun](../../contracts/shotgun_contracts.md). Preserve the original reference and coordinate renderer changes with its owner. |

## Explosion recipe cross-check

The IDs are from original `Shared/SFXMsgIds.h`; expected components come from
`ClientShellDLL/ExplosionFX.cpp`. This is component presence, not a percentage of
visual parity. **L** means evaluated light state only, never working illumination.
**M/S/R/F/P** mean model / sprite / particle ring / model fragments / flame particles.
All current selectors **and** publisher special cases were checked.

| IDs / original names (prefix EXP_) | Expected components | Connected recipe components / gap |
| --- | --- | --- |
| 0–2 DEFAULT_SMALL/MEDIUM/LARGE | M S R L | Present; illumination absent. |
| 3 GRENADE | M S R F L | Present, including fragment trails; illumination absent. |
| 4 NAPALM_PRIMARY | M S R L | Present, including all three model layers; illumination absent. |
| 5 NAPALM_ALT | M S L | Missing recipe. |
| 6 NAPALM_FIREBALL | S L | Present; no original M/R component to add. |
| 7 TESLA_PRIMARY | M S R L | Missing recipe. |
| 8 TESLA_ALT | M S P L | Present; no original R component to add. |
| 9 HOWITZER_PRIMARY | M S R F L | Missing recipe. |
| 10 HOWITZER_ALT | M S L | Missing recipe; gameplay children are separate. |
| 11 HOWITZER_MINI | M S R L | Missing recipe. |
| 12 FLAME_SMALL | P L | Missing recipe. |
| 13 FLARE_FIZZLE | S R L | Missing recipe. |
| 14–15 FLARE_BURST/FRAG | S L | Missing recipes; projectile fragments are separate. |
| 16 LIFELEECH_PRIMARY | S L | Present. |
| 17 LIFELEECH_ALT | M S R L | Present. |
| 18 SINGULARITY_PRIMARY | M R L | Missing recipe. |
| 19 SHOCKBALL | M R L | Missing recipe. |
| 20 SHOCKBALL_LARGE | M L | Present. |
| 21 ORB_BREAK | F | Present, including trails; no original light required. |
| 22 DIVINE_SHOCKBALL | M R L | Present. |
| 23 RIFT_1 | M R L | Missing recipe and Spawner connection. |
| 24 RIFT_2 | M R L | Unsupported recipe; specific live caller not established here. |
| 25 DEATHRAY_PRIMARY | M L | Missing recipe. |
| 26 BUGSPRAY_PRIMARY | P | Present. |
| 27 BUGSPRAY_ALT | R L | Present. |
| 28 NAGA_EYE_BEAM | M P L | Missing recipe and endpoint publisher. |
| 29 NAGA_SPIKE | S R F L | Missing recipe. |
| 30 NAGA_STONE_CHUNK | S F | Missing recipe. |
| 31 NAGA_POUND_GROUND | S R | Missing recipe. |

Object-particle recipes currently accepted by `CObjectParticleFX::Init` are
**1, 3, 12–19, 21, 24, 31–34 and 36**. This whitelist alone is not coverage:
Howitzer still fails to publish recipe 1, and enemy callers need their authored
scale flags. Conversely, unused/commented recipes are not automatic defects.
Tesla and Singularity first-person power-up sprite creation is commented out in
the original `ClientShellDLL/GameWeapons.cpp`; do not implement it as a “repair”.
`CGroundStrikeProjectile::AddExplosion` also intentionally emits nothing.

## Resources and verification boundary

Known absent authored audio remains a separate category: Power Station CVBC's
`Sounds/lightpop2.wav`, KABOOM's `Sounds/Thunder.wav` and the five Ancient One
Lightning1–5 Thunder references are recorded in the existing
[lightning contract](../../contracts/lightning_contracts.md#exact-missing-authored-audio).
Their particles can work while sound fails. This is inherited resource evidence,
not a fresh archive sweep. Do not alias paths, synthesize assets or silently claim
successful playback. Missing NPC/world artwork from older loading reports is not
evidence that an unimplemented VFX recipe's resources are absent.

No runtime, assets, shaders, save format or gameplay changed in lot 3. Checks are
source-call/recipe review, source hashes, local documentation links and diff
whitespace. No build, GPU run, headset run or original-executable A/B capture was
performed for this documentation-only lot.

For each subsequently authorized lot, verify primary/alternate and both hands,
dry/wet impacts, relevant materials and suppressed/missed contacts, resource
failures, source removal, unload/load and bounded lifetime. Then capture the same
scenario in the original and port on desktop; exercise the existing XR path
without introducing screen-space camera shake. Gameplay damage and cosmetic RNG
must remain independent. Low/medium detail and renderer limits must be explicitly
accepted or left open rather than hidden by a “complete effects” label.
