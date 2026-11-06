# Global illumination contracts

Baked GI for the M10 [GI plan](../implementation/gi_plan.md). Each lot adds its section.
GI is not rendered yet: nothing in this contract changes a frame, a save or the simulation.

## World queries (lot G1)

`engine/gi_world.h` holds read-only queries over the DAT. The baker and the presentation
mesh share them; they create no object, touch no GPU state and keep no reference to the world.

### Lightmap texel frame

`lightmap_frame(model, polygon, count, frame)` describes one stored polygon lightmap. It
refuses a polygon in four cases:

- no stored texels (width or height is zero);
- no SURF_LIGHTMAP bit (bit 7) on its surface;
- the model has no validated lightmap block;
- the texel range, the plane, or a base-ring point index is out of range.

The layout:

- **Texel centre.** Texel (sx, sy) is centred where `dot(lightmap_p, x) = origin[0] + 20 sx`
  and `dot(lightmap_q, x) = origin[1] + 20 sy` on the polygon plane `dot(normal, x) = distance`.
  The axes are not unit length.
- **Origin.** `origin` is chosen per axis by `lightmap_origin` over the first `count` points
  (the base ring), exactly as the mesh did before; `unfitted` counts the axes where no
  candidate fitted.
- **`coordinate(x)`.** Returns continuous texel coordinates, where texel centres sit at i + 0.5.
  The arithmetic is the mesh's (a float dot product and a double offset).
- **`position(sx, sy, x)`.** Solves the 3×3 system for a texel centre, fractional texels
  included. It fails when the axes and the normal do not span space.

`WorldMesh::build` now takes its origin and its vertex lightmap coordinates from this frame.
This is a refactor only:

- Vertex and lightmap-page digests are byte-identical on all 58 retail worlds, with and
  without lightmaps.
- `gpu.surface`, `gpu.post`, `gpu.lightmaps` and the `gpu.world*` tests pass unchanged.

### Point contents

`point_contents(model, point)` runs the original root-BSP descent: the back side is taken when
the signed distance is below −0.0001, as in `WorldVisibility::find_leaf`. It returns:

| Result | Meaning |
| --- | --- |
| `Empty` | The descent ends on `BspNode::inside` (−1). |
| `Solid` | The descent ends on `BspNode::outside` (−2), which is solid or void. |
| `Unknown` | The model has no tree, an index is invalid or the descent cycles. |

Evidence: 295 of 297 retail GameStartPoints are Empty. The other two are in 02_CityHub and
09_Abandoned_Buildings.

**World models are not carved into the root BSP.** A probe point therefore also needs both of
these:

- `point_clear(collision, point, half, clear)`: a zero-length `CollisionWorld` sweep of a cube
  against the primary solid faces, with `StartOverlap::Block`. The call fails on an invalid query.
- `world_model_containing(bounds, roles, point, margin)`: the first non-primary, non-sky model
  whose point bounds, grown by `margin`, contain the point. The bounds come from
  `world_model_bounds`, the authored-pose AABB of each model's points. The test is
  conservative, because a point in a concave model's hull counts as inside.

### World-model roles

`world_model_roles(world)` classifies every model from the DAT objects that bind it:

- Only classes that create OT_WORLDMODEL or OT_CONTAINER objects bind a model.
- They bind through `Name`, compared case-insensitively.
- The first model in file order with that name wins, like object creation.

| Role | Binding classes |
| --- | --- |
| Primary | Model 0. |
| Sky | DemoSkyWorldModel |
| Container | VolumeBrush, Water, Blood, Acid, Ladder, Conveyor, Damage, FreeFall, Minefield, Container |
| Mover | Door, RotatingDoor, RotatingBrush |
| Destructible | CDestructableBrush, WorldModelDebris |
| Unbound | Every other model. |

Notes on these rules:

- Objects of other classes never bind a model, even when their `Name` matches. Eleven retail
  Light, DirLight, ClientLightFX, Sound and GameStartPoint names coincide with BSP names.
- WorldModelDebris fragments beyond the exact `Name` stay Unbound.
- Bake policy per role is decision D8 of the plan: everything is baked at the authored pose.

### Ambient fill

`world_info_ambient_any_case` reads `AmbientLight r g b` from the world info string with a
case-insensitive key, under the record and number rules of `world_info_ambient`. The original
renderer's case-sensitive reader is unchanged (decision D15).

`audit_ambient(world)` measures, over lit faces (not invisible, sky or SURF_BRIGHT):

- the darkest stored texel, expanded from RGB565 like the mesh;
- the darkest baked vertex colour;
- the samples clearly below the ambient value. A texel counts when it is below by more than its
  RGB565 step (8/4/8); a vertex counts when it is below by more than 0.5.

Retail result, which settles the plan's ambient-floor rule:

- **The fill is a floor at ×1 in both data paths.** No lightmap texel or vertex in any of the 58
  worlds is below it.
- **Vertices.** The darkest vertex equals AmbientLight wherever the floor is reached.
- **Texels.** The darkest texel equals AmbientLight truncated to RGB565: for example,
  17 23 23 becomes 16 20 16.
- **The preprocessor read the lowercase key.** 24_AncientCity and 24b_Arena spell it
  `ambientlight`, and their data still carries the 15 10 5 floor.
- **Consequence.** The direct term of the plan, `D = O − min(O, A)`, uses A at ×1 for both
  texels and vertices.

### DAT identity

`world_dat_digest(resources, path, digest)` is the SHA-256 of the world DAT bytes, the same
value the save fingerprint stores. It is the bake identity key of lot G2.

### Tests

**`gi_world.frame`**
- Unit and non-unit axes on a tilted plane round-trip through `position`/`coordinate`.
- Refusals: no texels, no SURF_LIGHTMAP bit, out-of-range block, singular axes.

**`gi_world.contents`**
- Empty and Solid on each side of a plane, and the −0.0001 bias.
- Unknown for a missing tree, a cycle or an invalid child.
- Clearance above and through a solid floor.

**`gi_world.roles`**
- Each role, case-insensitive binding and duplicate-name shadowing.
- Non-binding classes and non-string names are ignored.
- Bounds containment skips the primary and sky models and honours the margin.

**`gi_world.ambient`**
- Parser cases, including the lowercase key.
- Audit counts, darkest values, the SURF_BRIGHT exclusion and the case mismatch.

**`gi_world.retail`** (private data)
- Every lightmapped polygon of the 58 worlds gets a frame.
- Corner texel centres round-trip within 1e-2 texel; the measured maximum is 2.4e-5.
- A majority of start points are Empty.
- Prints the per-world table of the [G1 report](../implementation/reports/gi_g1_prerequisites.md).

**`gi_world_tests digests <data>`**
- Prints the mesh digests that proved the refactor byte-identical.
