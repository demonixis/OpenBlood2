# Original server-class coverage

Reviewed 2026-10-25 against the runtime registry and the optional LithTech reference tree.
This is a class-name inventory, **not** a claim that every registered class has complete retail
behavior. It is separate from save-payload support and from missing artwork/audio.

The original `Blood2/ObjectDLL/ObjectDLL.dsp` sources contain 224 unique active
`END_CLASS_DEFAULT*` names, including conditional add-on names. The current runtime registers
192 names: 184 of those ObjectDLL classes, plus BaseClass, six native classes and StartPoint.
The following **40 names are not registered**. Source comments must be stripped lexically
before counting (a block-comment marker inside a line comment must not hide live declarations).
Runtime names are returned by `Objects::classes()` after `GameSession::initialize()` and are
registered in `src/game/registration.cpp`; the reference tree is not a build dependency.

The historical [class catalog](../implementation/save_load_audit/class_catalog.md#3d-original-compiled-classes-not-registered-in-openblood2)
contains source locations and measured world usage. Its 40-name difference still matches this
registry. None of those names was placed in the 58 shipped DATs in that audit. This does not
exclude a missing dynamically spawned class or a failure later in a level.

## Absent object registrations with replacement services

A replacement covers the listed paths, not every possible original caller or mod.

| Name | Current coverage |
| --- | --- |
| `CClientSmokeTrail` | Particle services cover selected trail callers; no server class. |
| `CClientSparksSFX` | Typed sparks service. |
| `CClientWeaponSFX` | WeaponFX dispatch to the typed effect services. |
| `CImpact` | Explosion/weapon sprites. |
| `CModelObject` | Explosion model service. |
| `CViewWeaponModel` | Client CViewWeapon instances. |
| `Explosion` | Typed explosion services; no generic Explosion object. |

`CProjectile` is the implemented C++ projectile base, without a directly constructible class registration.

## Unported add-on registrations

`CFlayerChain`, `CFlayerPortal`, `CGasGrenade`, `CNightmareFireball`, `CRockProjectile`, `CombatShotgunPU`, `FlayerPU`, `GremlinAI`, `Nightmare`.

## Other absent registrations

`CBulletImpactSFX`, `CClientBloodTrail`, `CClientCastLineSFX`, `CClientDebugLine`, `CClientLaserBeamSFX`, `CClientLaserFX`, `CClientMarkSFX`, `CClientSplashSFX`, `CClientTracer`, `CFireballProjectile`, `CGooProjectile`, `CImpactRicochet`, `CShellCasing`, `CSingularityAltProjectile`, `CraneControl`, `FireFX`, `LightFX`, `Rain`, `RotatingSprite`, `Script`, `SoccerBall`, `SoccerGoal`, `WreckingBall`.

`CClientLaserBeamSFX` is not a placeable server class; the existing weapon beam service is not
an implementation of that class. `LightFX` is distinct from the registered `ClientLightFX`
family. Absence here must not be hidden by an alias or a placeholder object.

## Names absent from the original as well

These are not missing ports: authored `Gremlin` and `RocketAmmoPU`, and the original dynamic
lookup spellings `DieBugDieAmmo`, `FareAmmoPU`, `LifeEssencePU`, `BatterAmmoPU`, `TheAngerPU`.
They remain explicit absent-original-class reports. In particular, do not alias `TheAngerPU`
to `AngerPU` or `Gremlin` to the add-on `GremlinAI`.

Original files not included in ObjectDLL.dsp (BirdAI, BugAI, RatAI, WurmAI, DefenseGlobeAI,
DefenseGlobeLegs and SpellPickup), commented registrations and duplicate variants are excluded
from the 224-name compiled-source set.

## Museum report

The 2026-10-25 report has no crash location, action, build identification or crash log.
`03_museum` passes current world inspection, 30 seconds of entry simulation, the duct regression,
and the difficulty-1 whole-world save audit (entry and after combat, restoration and subsequent
simulation). That audit saves 444 records / 478303 bytes. This is **not a reproduction or fix of
the reported crash**, and gives no evidence that one of the absent names above caused it.
A failing route/action and build, or a save immediately before the failure, is still needed.
