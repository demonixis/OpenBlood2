# CityHub museum approach: jump diagnosis

18 October 2026. Investigation of the user's report that Caleb cannot finish
level 2 because of the jumps before the museum. **The production defect remains
open.** The experiment below is not an integrated or regression-tested fix.
It does not reopen M2, resolve M2-GPU-001, or accept M3.

## Reproduction

The installed `worlds/02_cityhub.dat` supplies gravity `(0,-2000,0)`.
Caleb's original standing impulse is 600, giving an unobstructed rise of 90.
The installed idle/jump animation half dimensions are `(12,42,12)`.

The museum approach contains a concrete planter rim at Y=672 and a dumpster
top at Y=768: a **96-unit rise**. Its authored `hahahint` ObjectivesTrigger
explicitly describes reaching the museum through dumpsters. Using these as
supports is part of the authored route, rather than evidence by itself of a bug.
The following museum fence reaches Y=832, 64 units above the dumpster.

A headless diagnostic loads the actual world and player, then places Caleb at
each selected start. It calls the normal MoveMgr/server/collision path at 60 Hz
while leaving other object updates frozen. This isolates movement; it is not a
full interactive playthrough or a retail-executable comparison.

| Case | Starting player center | Observed result |
| --- | --- | --- |
| Flat reference | `(1100,682.002,-2860)` | Feet rise 90 units and return to the floor. |
| Planter rim to dumpster | `(1000,714.002,-2688)` | Feet rise 90 units, stop six units below the top, and fall. Forward movement remains blocked at Z approximately -2708. |
| Dumpster to museum fence | `(1060,810.002,-2750)` | Running jump crosses the fence at Z=-2800..-2808. No assertion of reaching the museum exit trigger. |

An earlier attempted start at Z=-2680 intersected the original tree model's
collision box and was rejected. The recorded rim start at -2688 is clear and
settles successfully before the jump.

## Isolated experiment

`ServerAdapter::MoveObject` requires walkable support and no upward motion
before attempting its three-sweep stair service. Consequently the existing
21-unit standing step allowance cannot help with the six-unit shortfall in air.

The [experimental patch](evidence/cityhub-jump-experimental.patch) removes that
support/upward-motion restriction, retaining the flag, solid-body, bounded rise,
clearance sweeps and normal landing checks. A separately compiled object is
linked into the probe only; project sources and the game executable are unchanged.

With this experiment, the rim-to-dumpster case reaches the dumpster and settles
at center `(1000,810.001,-2772)`, feet Y=768.001. Its maximum rise including the
step is 107.736. The unobstructed reference remains exactly 90, and the separate
dumpster-to-fence case still crosses. Jump impulse and gravity are unchanged.

This confirms the grounded-only step restriction blocks this route and that
airborne stepping can remove that particular blocker. It does **not** establish
the exact original engine policy. Later LithTech's
`runtime/shared/src/collision.cpp` invokes `StairStep` from `FLAG_STAIRSTEP`
without the added grounded-only gate, but is a different engine generation.
Before integration, the candidate needs bounded airborne behavior and regression
coverage for ceilings, walls, descending motion, contacts, NPCs and moving bodies.
The user's other two locations have not been identified or reproduced.

## Evidence and reproduction commands

Examined Blood2 revision: `17758caf31d2abc933c8b029ca7f1764db0b702c`.
Existing Linux Debug libraries were used; no production rebuild, GPU test,
sanitizer run, other-platform validation or retail playthrough was performed here.
Both final diagnostic executables exited zero; the blocked production jump is
a recorded failure of traversal, not a passing gameplay acceptance test.

- [Probe source](evidence/cityhub-jump-probe.cpp)
- [Current solver trace](evidence/cityhub-jump-current.log)
- [Experimental solver trace](evidence/cityhub-jump-experimental.log)
- [Candidate diff, not applied](evidence/cityhub-jump-experimental.patch)

From `Blood2/`, with existing Debug libraries and an external installation:

```sh
c++ -std=c++20 -I src -I src/game/legacy -I build/debug/_deps/sdl3-src/include \
  ../Documentation/Implementation/evidence/cityhub-jump-probe.cpp \
  -o /tmp/cityhub-jump-probe build/debug/libblood2_game.a build/debug/libengine.a \
  build/debug/_deps/sdl3-build/libSDL3.a build/debug/_deps/spdlog-build/libspdlogd.a \
  -pthread -lm
/tmp/cityhub-jump-probe "/path/to/Blood II"
```

For the experiment, copy `src/game/server_physics.cpp` outside the checkout,
apply the documented diff to that copy, compile with the same include arguments
and `-c`, and add its object before `libblood2_game.a` when linking the probe.
No commercial resources are copied into this repository.
