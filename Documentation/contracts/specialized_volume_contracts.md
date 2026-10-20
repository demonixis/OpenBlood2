# Conveyor, Damage and FreeFall boundary

This extends [container_contracts.md](container_contracts.md). The new classes
are selective ports of original VolumeBrushTypes, using the existing checked
volume geometry, object callbacks and shared player body. See
[m2_specialized_volume_sources.json](../implementation/sources/m2_specialized_volume_sources.json) and
[execution evidence](../implementation/reports/m2_specialized_volumes.md).
M2's playable-session gate remains open.

## Original rules and defaults

| Class | Code | Damage / second | Damage type | Liquid gravity | Surface |
| --- | ---: | ---: | ---: | ---: | --- |
| Conveyor | 6 | 0 | Normal (0) | Unused by conveyor motion | Hidden |
| Damage | 8 | 100 | Normal (0) | Unused by damage motion | Hidden |
| FreeFall | 9 | 9,999,999 | FreeFall (8) | 0 | Hidden |

Registered defaults retain zero viscosity, zero current, hidden surface colors,
10-second surface scale periods and the original FreeFall constructor's zero
gravity. FreeFall's repeated SurfaceHeight registration resolves to its final
value, 5. Typed authored properties override defaults, including Code and
DamageType. None of these classes is replaced by a synthetic box or mover.

Conveyor's original server override applies `current * step_time` only to a
supported character. Ordinary props and airborne characters receive no conveyor
current through this override. The source's missing Hidden check is retained:
a hidden conveyor can still affect a supported non-player character. It does
not call base volume viscosity/damage, even if those properties are nonzero.
The new service uses fresh checked support instead of a fabricated world handle.
AI behavior beyond this original character branch is not implemented.

The local player skips this server integration and uses original MoveMgr rules:

- The first conveyor body record consumes that category even if hidden. Each
  active tracked volume still adds its own current; the four-record bound and
  stable slot order remain unchanged.
- An active conveyor brakes the full velocity linearly at ladder speed / 0.2
  seconds, and applies the ladder 3D speed cap (200 walking, 400 running).
  Source update ordering uses the previous step's ladder speed for braking.
- Gravity and ordinary jump controls remain active. Conveyor membership alone
  does not enable pitch-relative free movement or halve movement acceleration.
  Ordinary dry-land drag is suppressed while the conveyor flag is set.
- These client rules apply whenever the body overlaps, including in the air.
  After a ground jump, the following step can cap the upward velocity. They are
  intentionally not rewritten as a ground-only platform controller.
- Force-position, removal, leaving the volume and teardown cannot leave a stale
  conveyor state. ON/OFF is refreshed on the next movement substep.

FreeFall follows the existing source liquid classification, swim controls and
braking, but uses zero liquid gravity and does not consume air. Its default
large damage still kills an ordinary player. A zero-damage authored override
can exercise zero-gravity motion independently; it is not the default behavior.

## Damage and level time

The base volume's original 0.1-second damage dose/cadence is retained. Accepted
Damage values now span 0–10,000,000 so the original FreeFall default fits;
the resulting dose remains within the existing 1,000,000 typed-message limit.
Nonfinite, negative and larger property values still reject creation.

Retail FreeFall entries in this installation store DamageType **10**, whereas
the source registration defaults to **8**. Some Damage metadata stores **9**.
Values 9 and 10 are preserved as the source enum's Blind and Slow identifiers;
they use the ordinary Destructable resistance/armor/health branch. They are not
remapped based on the class name. Original GameWeapons applies the voodoo
special actions separately (for example `Slow(5.0f)`); a volume damage identifier
alone does not add those weapon actions. Voodoo, vision effects and full death
reactions remain unported. Retail executable equivalence is unverified.

The original CPlayerObj damage-message branch ignores VolumeBrush senders for
the first two level seconds, before calling the aggregate chain. Other senders,
including self-suffocation and weapons, retain their normal damage path.
`GetWorldTime()` is an explicit typed adaptation of that source clock query:
it measures elapsed simulation time since successful world replacement, while
`GetTime()` and object schedules keep the existing monotonic session clock.
Failed load validation preserves the old level clock. Delayed player entry and
respawn do not restart grace; unloading/reloading does. Diagnostics count
protected volume messages separately from successful message delivery.

## Content, inspection and limits

The Containers tab includes all eight supported volume classes, source numeric
codes, current/gravity, damage/type, server physics and damage-message counters,
player conveyor state, health/armor and level protection count. Hidden volumes
remain listed; rejected source objects remain visible in World. Geometry is
still the actual closed shell. Changes are independent of Dear ImGui.

The supplied base/multiplayer archives create and remove 103 additional volume
instances: 38 Conveyor, 47 Damage, 18 FreeFall. Four entries in
`worlds/14_subwayc.dat` (two Damage, two Conveyor) bind the ambiguous name
`noname`; `worlds/20_powerstation.dat` Damage `kkk` has no model. The four
`noname` entries now bind the first model in file order ([duplicate-name rule](object_contracts.md#duplicate-bsp-names), 40
Conveyor / 49 Damage created); the missing model remains an explicit failure. `worlds/16_bridge.dat` remains unreadable at
the previously recorded typed property. The binding is deterministic and inspected, not arbitrary.

SubwayA now has 23 created / 372 preprocessor-only / 185 unported source entries.
CityHub has 11 supported volume instances, including its six conveyor escalators.
Creating a shape is not proof that every map's scripted gameplay works. The
real Subway movement scenario is a short airborne overlap below the train;
standing transport/jumps are verified with synthetic collision fixtures.
Moving train/platform collision is still absent.

CraneControl, Minefield, key handshakes, save/restore, AI container reactions,
liquid surfaces and underwater effects remain pending. Movers/pushers, full
combat and the remaining required classes still prevent playable startup.

The separately requested [Minefield continuation](minefield_contracts.md) supersedes
the Minefield omission above; other historical omissions keep their stated scope.
