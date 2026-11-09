# Original server-class coverage

This inventory tracks direct server class registrations in `src/game/registration.cpp`.
Registration, active behavior, edition availability, and save-payload coverage are tracked separately.
The optional reference tree and the original ObjectDLL binaries are audit inputs. The runtime uses its own factories.

The October source audit found 40 names without a factory. `CRockProjectile` and `GremlinAI`
were subsequently registered. The binary audit added the Demo-specific `CClientGibFX`, bringing the
follow-up list to 39. The historical [class catalog](../implementation/save_load_audit/class_catalog.md#3d-original-compiled-classes-not-registered-in-openblood2)
records source locations and world usage for the earlier audit.

## Follow-up registrations

| Family | Class names |
| --- | --- |
| Projectile combat | `CProjectile`, `CFireballProjectile`, `CGooProjectile`, `CSingularityAltProjectile` |
| Nightmares | `CFlayerChain`, `CFlayerPortal`, `CGasGrenade`, `CNightmareFireball`, `CombatShotgunPU`, `FlayerPU`, `Nightmare` |
| Scene and scripting | `CraneControl`, `Explosion`, `FireFX`, `LightFX`, `Rain`, `RotatingSprite`, `Script`, `WreckingBall` |
| Soccer | `SoccerBall`, `SoccerGoal` |
| Effect carriers | `CBulletImpactSFX`, `CClientBloodTrail`, `CClientCastLineSFX`, `CClientDebugLine`, `CClientLaserBeamSFX`, `CClientLaserFX`, `CClientMarkSFX`, `CClientSmokeTrail`, `CClientSparksSFX`, `CClientSplashSFX`, `CClientTracer`, `CClientWeaponSFX`, `CImpact`, `CImpactRicochet`, `CModelObject`, `CShellCasing` |
| View model | `CViewWeaponModel` |
| Demo | `CClientGibFX` |

The `CProjectile` factory creates the legacy base defaults and its registered subclasses inherit
the original class relationship. Fireball and goo retain their separate collision and particle
behavior. Gideon's goo weapon uses the original ID. The alternate Singularity is constructible and
has its own attraction behavior. Stock Singularity firing retains the source projectile selection.
The new active projectile and add-on pickup payloads remain unsupported for saving.

The blood-trail carrier places splats on nearby surfaces and follows its linked parent. The source
airborne payload used the particle-explosion message ID with a different layout. The port covers
the surface-splat behavior. The Demo debris carrier uses the same typed debris service as existing
destructible objects. The Demo binary's wider model and sound ranges are selected in that edition.
`B2Demo.rez` omits stone model/skin pairs 1, 4 and 5, so those draws advance to the next mounted
pair. Standalone marks, tracers and lasers use the local presentation pools.

The [Nightmares](nightmares_edition_contracts.md), [scene classes](legacy_scene_classes_contracts.md),
[crane](crane_objects_contracts.md), [soccer](soccer_objects_contracts.md),
[model effects](legacy_model_effect_contracts.md), [weapon effects](shotgun_contracts.md) and
[particle stream](particle_stream_contracts.md) contracts describe their behavior and boundaries.
The dedicated tests exercise stock maps or small fixtures, without requiring campaign traversal.

## Targeted verification

- `legacy_classes.registry` checks all 39 factory names independently of mounted resources.
- `spawner.cathedral`, `spawner.ancient_city` and `spawner.arena` trigger the authored unlocks.
- `nightmares.installed` inspects 13 original expansion maps. `nightmare_boss.world` exercises
  the boss death relay on the original map. The two `soccer` cases kick, score and respawn the
  ball on their original maps.
- `legacy_classes`, `fire_effects`, `rain`, `script`, `crane`, `client_splash`,
  `client_weapon_sfx`, `legacy_smoke`, `weapons.legacy_view_model` and `nightmares_weapons`
  cover setup, live behavior, effect publication and cleanup on focused scenes. The destruction
  and death suites check existing brush, crate, glass and gib callers.

## Names absent from the original as well

The authored `Gremlin` and `RocketAmmoPU` names and the dynamic lookup spellings `DieBugDieAmmo`,
`FareAmmoPU`, `LifeEssencePU`, `BatterAmmoPU`, and `TheAngerPU` remain explicit absent-original-class
reports. In particular, `TheAngerPU` and `Gremlin` retain their original identities.

Original files outside the compiled project, including BirdAI, BugAI, RatAI, WurmAI,
DefenseGlobeAI, DefenseGlobeLegs, and SpellPickup, were excluded from the inventory along with
commented registrations and duplicate variants.

## Museum report

The 2026-10-25 report lacks a crash location, action, build identification and log.
The earlier `03_museum` audit covered world inspection, 30 seconds of entry simulation, the duct
regression and difficulty-1 world saves at entry and after combat. It did not reproduce the crash.
