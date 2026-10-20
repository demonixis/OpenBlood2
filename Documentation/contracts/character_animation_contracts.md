# Original character animations and node queries

> Extended by [node visibility and player model restoration](character_node_contracts.md):
> original named resets now affect rendering/hit candidates, and the selected Load
> model/flags/velocity tail is available before client association. Earlier blanket
> node-hide/Load-tail omissions are historical. Full engine/client restore, limb/gib
> behavior and the M2 transfer gate remain incomplete.

> Extended by [NPC hit/recoil contracts](ai_hit_contracts.md): original visible-node
> localization, region damage and nonlethal reactions now execute for initialized
> Cultist/Civilian NPCs. Earlier blanket localization/recoil omissions are historical;
> lethal death/corpse/drop services remain unavailable and M2 is incomplete.

> Extended by [AI state/action contracts](ai_state_contracts.md): selected original
> state decisions, transitions and stationary animation commands now execute.
> Full AI_Update and production NPCs remain unavailable; M2 is still open.

> Extended by [character audio contracts](character_audio_contracts.md): original
> voice helpers, player sound keys and material/liquid footsteps now execute.
> Earlier blanket character-audio omissions are historical; AI hearing and M2 remain open.

This extends [locomotion](locomotion_contracts.md) and [model services](model_and_collision_contracts.md).
M2 remains incomplete. CAnim_Sound imports animation discovery, CheckAnims,
GenerateHitSpheres and ForceAnimation from Blood II; its voice helpers, damage
localization callers, AI states and save/load are not imported. No AI class is
registered by this increment. See [source selections](../implementation/sources/m2_character_animation_sources.json)
and [execution evidence](../implementation/reports/m2_character_animation.md).

## Source behavior

All 219 original animation lookup assignments, including fallback lookups, retain
their order and literal names. Animation index lookup remains exact and returns
UINT32_MAX on failure. CheckAnims starts at index 1, replaces values above 999
with the preceding slot, and leaves slot 0 alone. The original unsigned `< 0`
comparison is unreachable and removed. A real clip with index 1000 also triggers
this original threshold. Only the original sixteen arrays receive this pass;
Talk, Special, Strafe, Humiliation and the single fields do not.

The left strafe aliases still refer to right strafe names. The humiliation path
tries death 04, then 05, then the original misspelled `humilation` variants; the
missing second enemy humiliation falls back to correctly spelled 01. Strafe
columns 5–8 and Humiliation columns 4–9 retain their constructor zero values.
The inspector exposes every stored slot, including those defaults and missing
indices; it does not claim every slot was matched to a named clip.

CPlayerObj::SetCharacter calls original animation discovery and sphere generation
after its actual model assignment, as the source does. Healthy locomotion now
uses these stored indices rather than reproducing CheckAnims through repeated
name searches. Requested and actual names remain separately inspectable. The
new owned GetModelAnimationName query supplies the actual name without inventing
an old ABI function. Each character owns its table; callers replacing a model
must rebuild it, as SetCharacter does. The existing synthetic missing-clip test
now explicitly rebuilds its table after directly replacing the test model.

ForceAnimation retains the intermediate clip zero followed by the requested
index. Repeating a nonzero clip resets through zero; requesting zero while
already on zero does not restart it. Invalid targets are checked before either
assignment, avoiding partial mutation. It is not ResetModelAnimation.

## Current node transforms

GetModelNodeTransform returns the current world-space position and quaternion
of a live model node, including initial-update calls. It samples the actual track
at its current time using the same interpolation helper as the full skeleton
and vertex sampler, composes ancestors, applies positive object scale to model
position, then object rotation and translation. Rotation composes object and
node quaternions; scale does not alter orientation. There is no attachment,
blend, skinning-normal or hidden-node service implied by this API.

Names match ASCII case-insensitively; this choice follows the later LithTech
Model::FindNode reference, not an observed Blood II retail binary. Multiple
matching names fail rather than choosing arbitrarily. Missing/invalid nodes,
non-models, pending/stale objects, null outputs, invalid clocks, nonpositive
scale and transformed coordinates beyond the existing ±10-million-unit bound
fail without changing outputs. Inactive live models remain queryable. Queries
never advance the track or dispatch callbacks. There is no retained node cache:
the sampler walks a bounded ancestor stack (64 entries), avoiding heap allocation
and stale results after movement, animation reset, model replacement or unload.

## ABC quaternion normalization

The private male cultist has an idle1/pelvis quaternion with squared norm
15227.486328. The female cultist has an idle2/r_gun quaternion with squared norm
about 0.000042. Both are finite nonzero rotations; the previous reader's arbitrary
[0.0001, 10000] squared-norm window rejected them. The reader now normalizes any
finite nonzero float quaternion using a double-precision squared norm and length,
which handles both subnormal and maximum finite float components without overflow
or underflow. Runtime interpolation still uses the existing normalized float math.
Zero and nonfinite rotations, malformed sections and all existing resource budgets
still reject atomically. Synthetic tests assert normalized output at both numeric
extremes and continued zero/NaN/infinity rejection. Failure diagnostics identify
the animation, node, frame, squared norm and position. No replacement identity
rotation or skipped frame is introduced. NoMadEngine's parser was consulted for
its existing non-unit quaternion handling; its implementation was not imported.

## Original sphere measurements

GenerateHitSpheres reads the source's eighteen standard node names and computes
its pair distances multiplied by 0.66, including copied head/neck, torso/pelvis,
hand and foot radii. Missing later nodes copy the preceding position exactly as
the source does; their presence bits and a diagnostic expose that fallback.
If the first head node is missing, the source would use uninitialized data.
The port instead clears the radii, reports failure, and marks them unavailable.
A malformed model therefore cannot manufacture hit geometry. Unavailable
spheres do not turn an otherwise inspectable player model into a failed entry.

These radii are measured at SetCharacter's current pose, not recomputed every
animation frame. The Skeleton panel draws those stored radii at the live node
positions and labels them as measurements. They are **not used as collision
shapes or damage localization yet**. Existing hitscan/body damage remains the
previous tested implementation. The panel includes hidden/inactive/missing
models, live world transforms and all original animation slots. Reading it has
no gameplay side effects; Release has no ImGui dependency.

## Verification boundaries

Synthetic tests cover mapping/fallback quirks, invalid/missing clips, forced
restart semantics, hierarchical interpolation plus object transforms/scale,
missing nodes, first-match duplicate names ([MadScientistAI](madscientist_contracts.md)),
unchanged failure outputs, inactive and stale lifetime,
all original radius formulas and missing-node handling. Private checks inspect
all clips/nodes of Caleb, male/female civilians, labtech, male/female cultists and
Gideon, and repeat real SubwayA shell/player entry, movement and cleanup.
The GPU scenario exercises SDL movement/crouch with the inspector hidden,
queries the actual player head and stored radius, then unloads/re-enters;
its visible capture uses the Skeleton panel. No retail executable or non-Linux
platform validation is claimed. Class accounting and the M2 startup blocker
remain unchanged.

For the graphical lifecycle check and capture, run from Blood2:

```sh
./build/debug/blood2 --skeleton-smoke --capture build/skeleton.png
```

The installed corpus covers 326 clips and 6,410 node queries across the seven
models above. All lack the two standard ankle names; Gideon also lacks the two
upper-arm names. Their original predecessor fallback is explicit in inspection.
The table contains 227 stored slots (219 lookups include retry assignments).

The [held-model/attachment continuation](attachment_contracts.md) adds original
Beretta/knife held children and typed visual attachments. Earlier blanket hand-model
and attachment omissions are superseded for this bounded subset. Dropping, physical
attachments, client visibility SFX and complete M2 remain unavailable.
