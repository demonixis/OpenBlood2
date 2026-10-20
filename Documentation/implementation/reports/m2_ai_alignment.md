# M2 — Original parallel-axis alignment for AI navigation

Two Cultists in CityHub could permanently stop updating when their detour query hit
a horizontal surface. The port rejected an obstacle normal parallel to its cached
up vector, then latched `AI_Update: Obstacle normal/up is degenerate`. CMovement now
honors the original SDK's fallback for this valid alignment request. **M2 remains
incomplete.**

## Investigation and source basis

A diagnostic loads the real CityHub, makes the player invulnerable, snapshots the
27 production NPC positions and relocates the player near each. Three positions
fail a stationary primary-world clearance check and are skipped; the remaining
24 each advance 300 ticks at 60 Hz, totaling 120 simulated seconds. This deliberately
isolates nearby AI activity; it is not ordinary walking, a combat playthrough or
proof of complete CityHub traversal.

Against Blood2 baseline `b7313c194fb1d3ba2406dd19b1c0fa11bfce8ccb`, two different
Cultist instances named CultistAI8 suspend with the same alignment failure. Their
recorded cached positions are (1312, 613.818, -2544) and
(884.889, 676.914, -2394.71). The same diagnostic after the correction reports
**zero suspended NPCs at every sampled stop**, with the same 24 accepted placements.

Original [movement.cpp](../../../../sources/blood2/ObjectDLL/movement.cpp) calls
AlignRotation for clearance direction and obstacle normals. The Blood II
[server SDK](../../../../sources/blood2/AppHeaders/server_de.h) documents selection of
an alternative up vector for a parallel reference. The available later
[math service](../../../../sources/lithtech/runtime/shared/src/mathlt_impl.cpp) and
[frame builder](../../../../sources/lithtech/runtime/shared/src/geomroutines.cpp)
provide the threshold and bounded reference perturbations used here. This is an
original-compatible adaptation supported by source evidence, not verified retail
1998 execution parity.

## Change and inspection

CMovement keeps its existing right-axis convention and uses the alternative
reference only for valid parallel/near-parallel inputs. It preserves source query
order, collision, partial routes and work bounds. Zero/nonfinite vectors, missing
collision and invalid object identities still fail explicitly.

The two recorded requests do **not** find a traversable route: they now return the
ordinary Blocked result (14 and 13 rays respectively), which the existing original
navigation state handles by returning to Idle. The correction removes a false
engine-service failure; it does not manufacture a path through geometry.

The Skeleton inspector now shows the count of successful fallback alignments and
the last direction, cached reference up and resolved right. This is per-object
history, separate from the current plan result; it runs independently of the UI.
Read the [contracts](../../contracts/ai_alignment_contracts.md) and
[source ledger](../sources/m2_ai_alignment_sources.json).

## Validation

Synthetic coverage includes up/down/near-parallel direction, rotated local up,
ordinary horizontal behavior, invalid input, unavailable collision, no owner motion
and deferred removal. The private CityHub regression repeats both recorded requests
through two load/unload cycles and requires Blocked, bounded work, finite partial
points and zero remaining objects/clients/messages/strings. The existing GPU character
smoke also checks a parallel request outside the inspector, while hidden and after
reentry. Debug and inspector-free Release are built separately.

| Validation | Passed | Private-data tests included |
| --- | ---: | ---: |
| Debug targeted headless | 100 / 100 | 17 |
| Release targeted headless | 100 / 100 | 17 |
| Clang ASan/UBSan/leak checks | 54 / 54 | 8 |
| Debug NVIDIA | 6 / 6 | 5 |
| Release NVIDIA | 6 / 6 | 5 |

A final stronger assertion requires the two private queries to return Blocked. It
was rebuilt and passed in all three configurations; production code did not change
after the suites above. All 10,923 reference files still match the inventory and
the six reference repositories are clean.

Validation totals and exact commands are retained in the
[evidence ledger](evidence/m2-ai-alignment.json), alongside the before/after diagnostic
source and logs. The tested scope is navigation/recovery/following/locomotion/escape,
production AI, object lifetime, player controls and safe level transitions. GPU
checks use the NVIDIA Vulkan ICD and verify the actual device name in their logs.
Sanitizer checks cover navigation, recovery, production AI and object lifetime.

No shaders, dependency pins, reference repositories or commercial assets changed.
The new tests do not certify native macOS execution or all NPC encounters. Missing
AI actions/services, broader rendering fidelity, full-world restore and the other
observable M2 acceptance requirements remain open; the heartbeat remains active.

Implementation commit: `6aeb63750107c064c1b612d9be1399f0e19a8e17` in the independent Blood2 repository.
