# Tester feedback, 2026-10-27 (0.5.0)

A Windows 0.5.0 log, on top of `f28e7766`. No commercial assets or reference-source
dependencies are added. M2's reservation and milestone acceptance are unchanged.

## 02_CityHubB ends after 100 seconds (fixed)

The tester reached 02_CityHubB from 03_Museum; the session ended 100 s after the level
started:

```
Simulation stage failed: physics
Object: Physics MoveObject failed: CDestructableBrush / startcrate at 640.000000,-9998441.000000,2624.000000 result=60
```

`startcrate` is a gravity BoxPhysics brush authored at (640,−96,2624), next to the
`imatrigstart` trigger, where no world model has a single polygon underneath. It
falls on the first frame. The original engine has no velocity cap and keeps
integrating it out of sight; after ~100 s at gravity 2000 it reaches y ≈ −10⁷, the
coordinate range `physics_vector` admits, and `MoveObject` returned
`DE_INVALIDPARAMS` (60), which failed the step.

It is not alone. CityHubB's Root geometry spans x −896…3904, yet three
CDestructableModels and five pickups (SubMachineGunPU, three BulletAmmoPU,
FlashlightPU) are authored beyond it or over holes, and two range crates fall onto
the sky clouds. The same leftovers exist in 02_CityHubC; 09_Abandoned_Buildings
drops two TimeBombPU and 12_Train_Station one ShellAmmoPU. Every session that stayed
about 100 s in one of these four worlds failed the same way.

- Generic physics now implements the original `FLAG_REMOVEIFOUTSIDE`: an object
  whose physics or projectile move leaves it outside the world is removed (deferred),
  counted in `ServerAdapter::outside_world_removals`. "Outside" is later LithTech's
  `CWorldServerBSP::IsOutsideWorld`: beyond the DAT footer extents padded by 100.
  An unmoving object is never removed, as in `UpdateObjectPhysics`.
- Replacement policy: a body without that flag that is already outside the padded box
  is held — not integrated — while neither its velocity nor its acceleration
  (gravity included) points back toward the world and no container affects it,
  counted in `outside_world_holds`. Its flags, velocity and saved state are untouched;
  a teleport back inside resumes ordinary physics. The original kept it falling where
  nothing can touch or see it.
- `containers.outside_world` (headless) covers the hold, two minutes past the former
  failure, the return/teleport cases and both removal rules.

A temporary audit ran every mounted world for 110 s at medium difficulty: all
single-player worlds and every multiplayer map with a start marker complete without a
failure; holds occur only in the four worlds above. The tester's exact Museum-to-
CityHubB entry (`03_egress`) was not replayed.
