# Global illumination contracts

Baked GI for the M10 [GI plan](../implementation/gi_plan.md). Each lot adds its section.
Lot G7 bakes the GI lightmaps and lot G8 composes them into the world when Options > Display
GLOBAL ILLUMINATION is on (off by default, decision D11). With it off, or without a Ready file,
every frame is unchanged; nothing in this contract changes a save or the simulation.

- [World queries (lot G1)](#world-queries-lot-g1)
- [Baked file and keys (lot G2)](#baked-file-and-keys-lot-g2)
- [F8 overlay (lot G3)](#f8-overlay-lot-g3)
- [Probe placement (lot G4)](#probe-placement-lot-g4)
- [Bake scene and tracer (lot G5)](#bake-scene-and-tracer-lot-g5)
- [Bake-all script and packaging (lot G6)](#bake-all-script-and-packaging-lot-g6)
- [GI lightmap core (lot G7)](#gi-lightmap-core-lot-g7)
- [World override (lot G8)](#world-override-lot-g8)

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

## Baked file and keys (lot G2)

`engine/gi_file.h` holds the baked GI container (`.ob2gi`), the keys that tie it to the mounted
content, the freshness rules and the search order. Lot G5 writes it and lot G8 reads it at
runtime ([world override](#world-override-lot-g8)).

### Container

One file per world, little-endian, deterministic: the same `GiFile` always gives the same
bytes (no time, host name or padding garbage).

| Field | Bytes | Rule |
| --- | --- | --- |
| Magic | 8 | `OB2GI` 0x1A 0 0 |
| Version | 4 | 1 |
| Endian marker | 4 | 0x01020304 |
| File size | 8 | The whole file, trailer included; at most 256 MiB |
| World digest | 32 | SHA-256 of the world DAT (`world_dat_digest`) |
| Albedo key | 32 | See below |
| Settings hash | 32 | SHA-256 of the INFO settings text |
| Placement hash | 32 | Placement the bake used (G4); zero until then |
| Algorithm | 4 | `gi_algorithm_version`, bumped when a bake would change numbers |
| Edition | 1 + 3 | 0 retail, 1 Nightmares, 2 demo; three zero bytes |
| World path length | 2 + 2 | At most 128; two zero bytes |
| World path | n | Normalized resource path ending in `.dat` |
| Section count | 4 | 1 to 6 |
| Sections | | Tag (4), flags (4, zero), size (8), CRC-32 (4, `SDL_crc32`), payload |
| Trailer | 32 | SHA-256 of every preceding byte |

Sections appear in one order only, each at most once: `INFO` (mandatory, first), then the
optional `PLAC` (G4), `LMAP` and `VTX ` (G7), `PROB` (G9), `REFL` (G11). The INFO payload is a
u16 length and the canonical settings text (UTF-8, no NUL, at most 4,096 bytes). Every other
payload is opaque here; its owning lot decodes it and applies its own limits (texels, probes,
reflection resolution).

### What refuses a file

`read_gi_file` validates everything before returning and leaves its output untouched on
failure. Each refusal names its reason ("Invalid GI file: …"):

- wrong magic or a file shorter than the fixed header;
- a file over 256 MiB, a declared size different from the actual size;
- an unsupported version, a wrong endian marker, an unknown edition, nonzero reserved bytes;
- a world path that is too long, truncated or not a normalized `.dat` resource path;
- a SHA-256 trailer mismatch (checked before any section);
- a section count outside 1–6, an unknown tag, a duplicate or out-of-order section, INFO not
  first, nonzero flags, a truncated section header or payload, a CRC-32 mismatch;
- INFO with a bad length, invalid UTF-8, or a text whose SHA-256 differs from the header;
- bytes left between the last section and the trailer.

`write_gi_file` refuses the matching invalid inputs (settings hash, text, world path, edition,
section order, size) and never returns partial bytes. `save_gi_file` writes atomically: a
refused or failed write keeps the previous file.

### What only marks it stale

A valid file is compared with the mounted content by `gi_status`:

| Status | When | Applied? |
| --- | --- | --- |
| Missing | No file under that name | No |
| Failed | Unreadable or refused (above) | No |
| Foreign | Another world digest, or a file baked for another world path | No; not a candidate |
| Stale | Same world DAT, but a compared key differs | No; reported with its reasons |
| Ready | Every compared key matches | Yes, the only applied status |

Stale reasons are `albedo`, `algorithm`, `edition`, `settings` and `placement`. Settings and
placement are compared only when the caller asks for them: the runtime accepts any bake
settings, while a bake with `--gi-if-stale` (G5) compares its own. The F8 states Dirty, Baking
and Disabled are runtime states of G3/G10, not file statuses.

### Keys

- **World:** the DAT SHA-256, the same value as the save fingerprint.
- **Albedo:** SHA-256 over the sorted set of normalized texture paths of every world model, plus
  the DTX frames of SPR textures. Per texture: the path, a present flag, then for a present one
  the providing archive's directory digest, entry offset and size. Missing textures are keyed
  as absent, so installing one marks the bake stale. An invalid texture path fails the key.
- **Edition:** from the mounted set (`--demo`, `--nightmares`, otherwise retail).
- `gi_expectation(resources, world_path)` computes world, albedo, edition and algorithm.

### Names and search order

`gi_file_name` maps a world to `<normalized path without .dat>.<first 12 hex digits of the DAT
digest>.ob2gi`, for example `worlds/01_subwaya.91bc95a10ed8.ob2gi`. A new DAT gets a new name,
so an old bake never shadows it.

`gi_search_roots(configured)` returns, in order:

1. `--gi-dir DIR`, else `gi_dir = DIR` from the configuration file (relative to that file);
2. `user_directory()/gi`;
3. `/sdcard/openblood2/gi` (Android only);
4. `SDL_GetBasePath()/gi`, the packaged copy (decision 8).

`find_gi_file` reads at most one file per root through SDL I/O (Android assets work), never
writes or deletes, records every candidate with its status and detail, and chooses the first
Ready one. Later roots are still reported, so a shadowed or stale copy is visible.

### Configuration and tools

- The configuration file accepts `gi_dir` beside `data_dir`, each at most once; keys stay
  case-sensitive and anything else is still refused.
- `--list-worlds` prints every mounted world DAT: single-player (`worlds/<name>.dat`), then
  `worlds/multi/`, then other `worlds/` subdirectories, then `worlds_ao/`, in path order within
  each group, with its kind, DAT digest and GI file name. `--json` prints one JSON object
  (`edition`, `worlds[]` with `path`, `kind`, `digest`, `gi_file`) and lowers the console log
  level to `error` unless `--log-level` is given. Exit 0, 2 (data), 5 (a DAT cannot be read).
- `--gi-verify FILE` validates a file, prints its header and sections, then its status against
  the mounted world and the expected file name. Exit 0 (ready), 2 (arguments, data or an
  unreadable file), 4 (refused, stale or foreign), 5 (its world is not mounted or unreadable).
- Both are separate headless tools: they refuse `--world`, `--frames`, `--capture`, the other
  tool and every other execution mode.

### Tests

**`gi_file.roundtrip`** Byte-identical rewrites, INFO-only files and the largest settings text.

**`gi_file.write_refusals`** Every writer refusal, with the output left unchanged.

**`gi_file.read_refusals`** Every reader refusal above, each reached past the earlier checks by
resealing the size and trailer; the output is left unchanged.

**`gi_file.fuzz`** 4,000 deterministic mutations (with and without resealing, with truncation):
each is refused or round-trips byte for byte.

**`gi_file.status`** Ready, each stale reason, combined reasons and Foreign.

**`gi_file.naming`** Normalization, the digest suffix and refused paths.

**`gi_file.search`** Root order with the isolated user directory; Missing, Failed, Stale, Ready
and shadowed candidates; a renamed foreign file; atomic save keeping the previous file.

**`gi_file.albedo`** A synthetic REZ: order-free, case-normalized, sensitive to entry size,
to an absent texture appearing and to the texture set; invalid paths fail.

**`gi_file.retail`** (private data) Deterministic expectations and names for all 58 worlds; a
saved file is Ready, then Stale (albedo) after its key moves.

**`config.gi`** `gi_dir` precedence and refusals, tool option conflicts, the JSON log level.

## F8 overlay (lot G3)

The GI tools overlay (`app/gi_overlay.{h,cpp}`, class `GiTools`) is the shell the later lots
fill. It reads the session and the GI folders only: no simulation, save or rendered-frame change
while it is closed, and no GI is applied by it.

### Builds

- `BLOOD2_GI_TOOLS` defaults to ON on desktop (macOS, Linux, Windows), Release included
  (decision D4), and OFF on iOS and Android (Quest included); turning it on there is a
  configuration error. The dedicated server builds no client.
- In XR sessions F8 is unavailable (`MenuInput::gi_available` is false) and the overlay never
  draws.
- F8 itself is reserved in every build, like F10 (decision D5); without the overlay it does
  nothing.

### States and input

| State | Shown | Input |
| --- | --- | --- |
| Closed | Nothing | Unchanged |
| Open | The window (panels below) and the gizmos | Owned like the F10 inspector: play blocked, mouse released, keyboard and mouse go to ImGui, Esc does not open the pause menu |
| Pinned | A non-interactive status strip (top right) and the gizmos | Unchanged: play continues |

- **F8**, a fresh key down: Closed or Pinned → Open, and the inspector closes; Open → Pinned
  when the window's **Pin** switch is set, otherwise Closed. Repeats do nothing.
- The window's close button leaves Open the same way.
- **F10** opening the inspector leaves Open (to Pinned or Closed); the two never own input
  together.
- F8 is ignored, and the event continues unchanged, during text entry (the message editor or a
  focused ImGui field), while a Controls row tracks a device, and when the overlay is
  unavailable.
- `MenuInput` handles F8 before the inspector gate in the normal flow (title screen and
  promotions first, as for F10) and before `DemoFlow` in the menu-free demo, where F10 hands the
  overlay over to the inspector and a finished demo closes it. `--view-world` uses the normal
  flow.
- Ownership changes reset play input like the inspector (`MenuInput::reset`, `tools_open()`).

### Panels

Panels use CyberEngine's vocabulary: Status, Runtime, Light probes, Reflection probes, Bake.
Status works since G3, the two probe panels since [G4](#f8-placement-editing) and Runtime since
[G8](#f8-runtime-panel); Bake names the lot that brings it (G10).

**Status** shows, truthfully:
- the summary: no world, error, no baked file, a stale file with its reasons, a refused/foreign
  file, or a ready file with the runtime state since G8: "ready file, applied", "ready file, GI
  off (Options > Display)" or "ready file, not applied (<state>)" ("ready file" where no presenter
  is linked);
- the runtime line: whether GI is drawn, from which file, or why the original lightmaps are in
  use;
- the world path, DAT and albedo digests, edition, algorithm and expected file name;
- every search root with its status (the chosen Ready candidate marked), path, stale reasons
  or failure detail, exactly as `find_gi_file` reports them;
- the camera position and the root-BSP contents there;
- **Refresh**, and the **Show camera cell** switch.

The lookup is recomputed when the overlay leaves Closed, when the world path or revision
changes, and on Refresh; never per frame.

### Gizmos

- `ScenePresenter::tool_lines` is drawn after the ballistic tracers with the same line pipeline,
  depth test and pass. It is read when a frame is recorded and is empty unless a tool fills it,
  so frames with the overlay closed are unchanged. No shader artifact is added.
- G3's one gizmo is the 128-unit lattice cell containing the camera, coloured by the root-BSP
  contents at the camera: green Empty, red Solid, magenta Unknown (`GiTools::cell_gizmo`).

### Tests

**`menus.gi_tools`** Unavailable F8; Open blocking play, fire and Esc; repeats; Closed; Pin and
Pinned play; inspector exclusion both ways; message editor, ImGui text field and control
capture keeping F8; the demo flow (no pause while open, F10 hand-over, finished demo).

**`input_actions.table` / `input_actions.rebind`** F8 refused as a binding and for assignment.

**`input_actions.controls_file`** Version 3 writes; version 1/2 F8 lines dropped and reported;
version 3 F8 refused; an unknown version refused.

**`gi_overlay.cell`** Cell bounds and the Unknown colour.

**`gi_overlay.draw`** Headless ImGui frames: Closed draws nothing and clears gizmos; Open and
Pinned draw without a world.

**`gi_overlay.retail`** (private data) 01_SubwayA through `--gi-dir`: no baked file, stale
(albedo), then ready and chosen; the Open window draws its table.

## Probe placement (lot G4)

`engine/gi_placement.{h,cpp}` places the light and reflection probes a bake will use. A
placement file holds only the editable inputs; the light-probe lattice is derived from the file
and the world, deterministically, so the file stays small and reviewable. The bake reads it (G5);
the runtime never does.

### Placement file

One canonical JSON file per world, `<normalized world path without .dat>.json` (for example
`worlds/01_subwaya.json`), in the placement directory:

1. `--gi-placement-dir DIR`, else `gi_placement_dir = DIR` in the configuration file (relative to
   that file, at most once, like `gi_dir`);
2. in GI tool builds, the checkout's `data/gi/placement` when `data/gi` exists (decision 8; the
   path is compiled in as `BLOOD2_GI_PROJECT_DIR`);
3. `user_directory()/gi/placement`.

```json
{
  "format": "openblood2-gi-placement",
  "version": 2,
  "world": "worlds/01_subwaya.dat",
  "dat_sha256": "<64 lowercase hex digits>",
  "excluded_models": ["Door01"],
  "light_probes": {
    "spacing": 128,
    "budget": 16384,
    "clearance": 8,
    "volumes": [
      {"mode": "exclude", "min": [x, y, z], "max": [x, y, z]}
    ]
  },
  "reflection_probes": {
    "cap": 42,
    "probes": [
      {
        "auto": true,
        "capture": [x, y, z],
        "influence": {"min": [x, y, z], "max": [x, y, z]},
        "projection": {"min": [x, y, z], "max": [x, y, z]},
        "blend_distance": 32,
        "priority": 0,
        "box_projection": true,
        "resolution": 64
      }
    ]
  }
}
```

- **Writer** (`write_gi_placement`): fixed key order and layout, shortest round-trip floats,
  `-0` written as `0`. The same placement always gives the same bytes. Saves are atomic; the
  previous file survives any failure.
- **Reader** (`read_gi_placement`): at most 256 KiB, nesting 6, 8,192 values, strings of 256
  bytes (the shared bounded parser `engine/bounded_json.{h,cpp}`, also used by the material
  manifest). Every key exactly once, no unknown key, then `validate_gi_placement`. Output is
  untouched on failure.
- **Limits:** a normalized `.dat` world path without `"`, `\` or control characters (at most
  128 bytes); spacing a multiple of 8 in [32, 1024]; budget [1, 262144]; clearance [0, 64];
  at most 256 volumes; boxes with min < max on every axis and coordinates within 1e6; cap
  [0, 42] and no more probes than the cap; capture inside the projection box; blend [0, 1024];
  priority [−1000, 1000]; resolution a power of two in [16, 256]; at most 256 excluded models,
  each 1–128 bytes of printable ASCII without `"` or `\`, unique ignoring case.
- **Versions:** the writer emits version 2. The reader also accepts version 1, which has no
  `excluded_models` key (read as empty); a version 2 file must have it. Lot G5 added it
  (decision D8); the light-probe lattice ignores it, and its F8 editing arrives with the G10 Bake
  panel.
- **Statuses** (`load_gi_placement`): Missing (no file), Ready, Foreign (valid but made for
  another world path or DAT digest; the detail names it) and Failed (unreadable or invalid).
  A Foreign or Failed file is never replaced implicitly.
- **Placement hash** (`gi_placement_hash`): SHA-256 of the canonical text followed by
  `placement_algorithm 1\n`; the `.ob2gi` placement key. `gi_placement_algorithm` is bumped
  whenever the same file and world would resolve to different probes.

### Light probes

`GiPlacementScene::build` keeps references to the world and its primary collision (both must
outlive it and stay unchanged). `classify(point, clearance)` decides a probe point, in order:

1. the root BSP must say Empty: Solid (or void) gives **Solid**, an undecidable or invalid
   query **Unknown**;
2. a cube of half-size `clearance` must meet no primary collision face (`point_clear`), else
   **Clearance**;
3. Mover, Destructible and Unbound world models (doors, breakables, crates) whose bounds, grown
   by the clearance, contain the point: a model without a BSP tree, a point not Empty in the
   model's own tree, or a clearance-cube corner Solid there gives **WorldModel**. Containers
   and sky models never block.

`resolve_light_probes`:

- **Lattice:** nodes on multiples of the spacing over the primary model's point bounds:
  `origin = floor(min/s)·s`, `dims = floor((max − origin)/s) + 1` per axis, node index
  `x + nx·(y + ny·z)`, at most 16 M nodes.
- **Volumes:** a node or relocation target inside an exclude box, or outside every include box
  when any exists, is **Excluded**.
- **Relocation:** an invalid node tries the 26 neighbour offsets at s/4 then at 3s/8, nearest
  first (stable), and takes the first valid, non-excluded one. A node rejected for Clearance or
  WorldModel (empty space) must also see its target (no primary face on the segment); a Solid
  or Unknown node moves without that test. Relocated probes are flagged.
- **Budget:** over the budget, the spacing grows along ×1.25, ×1.5, ×1.75, ×2, ×2.5, ×3, ×4
  (rounded to multiples of 8; counting stops at budget + 1). Each tried spacing is reported
  with its count. Still over at ×4 is a failure.
- **Stats:** every node of the final lattice is exactly one of probe (kept or relocated),
  excluded, solid, unknown, clearance or world model. Unknown, Clearance and WorldModel nodes
  are also returned for the F8 markers.

Accepted risk: relocation can cross a wall thinner than s/4 from a solid node (no visibility
test there); wall nodes are the common case, so most relocated probes move into the room.

### Reflection probes

`fit_room(point)` casts six axis rays against the primary collision faces, limited to the
world bounds. A hit within 1e-3 of the bounds limit counts as the outer wall (enclosed); a miss
marks the room **open**. Closed doors (Mover models at the authored pose) crossing a ray
shorten it to their bounds. Faces are rounded to 1/16 unit.

`place_reflection_probes` replaces the automatic probes and keeps the manual ones first:

- candidates are rooms fitted around the lattice probes (a stride keeps at most 4,096);
- rooms narrower than 192 units (x or z) or lower than 96 units (y) are **small** and skipped;
- the rest are ranked enclosed first, then by volume (largest first), then by probe order;
- a room overlapping an accepted probe (manual, or of the same kind, enclosed or open) by more
  than half of the smaller volume is **overlapping**; beyond the cap it is **capped** but still
  counts against later duplicates;
- an accepted room becomes a probe whose influence and projection boxes are the room, with
  `blend_distance = min(32, smallest extent / 4)`, priority 0, resolution 64 and box projection
  only when enclosed. Its capture point is the room centre (rounded to 1/16) when that point
  passes `classify` with the placement clearance and is visible from the seed, else the seed.

`fit_reflection_probe(point)` builds one manual probe the same way around any point whose root
contents are Empty and refuses solid or void capture points.

### Placement section

`encode/decode_gi_placement_section` define the `.ob2gi` PLAC payload (version 1, little-endian)
that G5 will write: `u32 version, u32 spacing, f32 origin[3], u32 dims[3], u32 count`, then per
light probe `u32 node, f32 position[3], u32 flags` (bit 0 relocated), then `u32 count` and per
reflection probe 76 bytes: capture, influence min/max, projection min/max (f32×3 each), `f32
blend, i32 priority, u32 resolution, u32 flags` (bit 0 box projection, bit 1 automatic).
Decoding refuses descending or out-of-lattice nodes, a probe farther than 3s/8 from its node, an
unrelocated probe away from it, invalid reflection probes and trailing bytes.

### `--gi-place`

`--gi-place WORLD|single|all` (a name such as `01_SubwayA`, a resource path, the single-player
worlds or every mounted world, in `--list-worlds` order) loads each world, reads its placement
(Missing gives the defaults), resolves the light probes, re-places the automatic reflection
probes and writes the canonical file when its bytes change.

- `--gi-reset` ignores existing files (a Foreign or Failed file otherwise fails that world with
  "(--gi-reset replaces it)"); `--gi-dry-run` writes nothing; `--json` prints one object
  (`directory`, `worlds[]`) and lowers the console log level like `--list-worlds`.
- One `GI_PLACE <path> status=written|unchanged|dry-run|failed` line per world with the
  spacing, the lattice counts by reason, the reflection counts (`reflections`, `manual`,
  `open`, `rooms`, `small_rooms`, `overlapping`, `capped`), `light_ms`, `reflection_ms` and the
  file; a failure adds its detail.
- Exit 0, 2 (arguments, data, directory or an unmounted world), 4 (a placement failed), 5 (a DAT
  cannot be read). It is a separate headless tool: it refuses `--list-worlds`, `--gi-verify`,
  `--world`, `--frames`, `--capture`, `--bench` and every other execution mode.

### F8 placement editing

The overlay loads the current world's placement when the world path or revision changes (not
when it reopens) and resolves the lattice; unsaved edits survive closing the overlay and a
reload of the same DAT. Missing gives the defaults; Foreign and Failed leave nothing to edit
until **Start from defaults**, which replaces the file only when saved.

- **Shared header:** directory, status, Dirty ("unsaved edits"), the last message, a live
  validation line, **Save** (refused while the light probes do not resolve), **Revert** and
  **Reset to defaults**.
- **Light probes:** spacing, budget and clearance (applied on Enter or the step buttons,
  re-resolving the lattice), **Place light probes**, the counts, rejections by reason and
  over-budget spacings, the resolve time, marker switches and radius, and the volume list
  (**Add include/exclude at camera**: a 512-unit box; type switch, box drag, delete). Box drags
  re-resolve when released.
- **Reflection probes:** the cap (lowering it drops the last automatic probes), **Place
  automatically**, **Add at camera**, the last placement's counts, a selectable list and, for
  the selected probe, **Move to camera** and **Fit to room** (refit, keeping priority,
  resolution, box projection and blend), **Delete**, capture, influence and projection drags,
  blend, priority, box projection and the resolution (16–256). Any edit makes the probe manual
  and moves it among the manual probes.
- Every edit sets Dirty; Save writes the canonical file and sets Ready.
- **Markers** (Open and Pinned, in a world): light probes as 16-unit crosses (green kept, yellow
  relocated) and, when shown, rejected nodes (magenta), within the marker radius (default 1,536)
  and at most 4,096 together; volumes (green include, red exclude); reflection probes (orange
  influence, blue projection, white capture; the selected influence and capture yellow). The
  Pinned strip and the Status panel add the placement summary.

### Tests

**`gi_placement.lattice`** Synthetic 1024×256×1024 room: 9×3×9 nodes, 243 probes (194
relocated); exclude and include boxes; clearance 64 rejects wall nodes; budget 100 moves to
spacing 160 and reports `{128, 101}`; budget 1 fails.

**`gi_placement.world_models`** A door block inside the room: WorldModel (inside and within
the clearance), Clearance, Solid and Unknown classifications, one WorldModel and two Clearance
rejections, a probe moved off the door face; containers and sky never block, an unbound model
does; a world without a primary tree is refused.

**`gi_placement.reflections`** An exact enclosed room box; one probe at the centre with 242
overlapping candidates and its defaults; a manual probe kept first and suppressing the room;
cap 0 caps it; a door ends a room; a narrow room gets no probe.

**`gi_placement.json`** Canonical round trip, formatting-independent hash, every refusal named,
the writer refusing what the reader refuses, and 3,000 seeded mutations, each refused or
rewritten canonically.

**`gi_placement.files`** Names, directories, Missing/Ready/Foreign (DAT and world path)/Failed,
and a refused save keeping the previous file.

**`gi_placement.section`** PLAC round trip, sizes `36 + 20n + 4 + 76m` and refusals.

**`gi_placement.retail`** (private data) All 58 mounted worlds resolve within the default
budget; prints the audit lines.

**`gi_overlay.draw`** Every panel draws without a world; edits are refused without one.

**`gi_overlay.retail`** (private data) 01_SubwayA in a scratch placement directory: Missing
gives 1,031 probes; markers; a manual probe added at a probe, saved and refused in the void;
automatic placement keeps it first; delete; an exclude volume removes probes; Revert and
Reset; both panels draw; edits survive closing; a Foreign file waits for Start from defaults.

**`config.gi`** `gi_placement_dir=` (relative, empty and duplicate refused), the
`--gi-placement-dir` override, `--gi-place` with its modifiers and every refused combination.

## Bake scene and tracer (lot G5)

`engine/gi_bake_scene.{h,cpp}`, `engine/gi_jobs.{h,cpp}` and `engine/gi_bake.{h,cpp}` turn a
world and its placement into a `.ob2gi` file through a deterministic CPU tracer. Lot G5 traced a
probe survey and wrote INFO and PLAC; lot G7 adds the LMAP and `VTX ` sections
([below](#gi-lightmap-core-lot-g7)) and G9 will add PROB. Nothing here changes a frame, a save or
the simulation.

### Bake scene

`GiBakeScene::build(mesh, uses)` takes a `WorldMesh` built with resources and lightmaps on (the
faces the renderer draws) and one use per range:

- `gi_range_uses(world, excluded)` gives Sky to the models drawn as sky (FLAG_SKYOBJECT:
  DemoSkyWorldModel models and SkyPointer targets, as `gi_sky_setup` replays them, lot G7), Main
  to the other Primary, Mover and Destructible models and Skip to containers, unbound models and
  the models of refused DemoSkyWorldModel objects (`world_model_roles`).
  Every name in the placement's `excluded_models` must match at least one non-primary model
  (case-insensitive; all matches are skipped) or the bake fails with exit 4.
- Positions are local + pivot with no rotation: the authored pose (decision D8). Sky ranges go
  to a separate tree (`trace_sky`) and never block bake rays; the G7 sky cube paints their faces
  directly (decision D9).
- Each triangle keeps its face record: range, material, `WorldMesh` triangle index, lightmap
  page, flags, plane normal, UVs, lightmap coordinates and vertex colors. Flags: **Translucent**
  (a SURF_TRANSPARENT batch with a material, chroma-keyed copies included), **SkyPortal** (a sky
  face; rays end there with `Sky`), **Lightmapped** and **Unresolved** (no material: opaque,
  albedo 0.5 grey).
- **Albedo** (`albedo(face, u, v)`): raw stored RGB (0–1, display values: lot G7 keeps them raw,
  as the renderer multiplies them by the light), nearest texel, repeating, from mip
  `round(log2(20 × texels per world unit))` clamped to the image's levels (`gi_albedo_mip`;
  texels per unit is the square root of the texel area over the world area). An SPR uses its
  first frame, the presentation's initial texture. Alpha is the mip-0 texel alpha the tracer
  used, 1 for opaque faces.
- **Baked light** (`baked_light(face, u, v)`): WorldShade's rule, bilinear page sample ×
  `world_saturate_scale` for lightmapped faces, else the interpolated vertex colors.
- Images are checked once (dimensions, RGBA size; mips used only when `mip_rgba` has exactly
  `texture_mip_bytes`); batches hold whole triangles from a triangle boundary; at most 4 Mi
  triangles.

### BVH and tracing

`GiBvh` is a binned SAH tree (12 bins, leaves of at most 4 triangles, a traversal step costing one
triangle test) built single-threaded and deterministically: stable partitions, and below depth 40
or when no split helps, a median split with ties by input order; depth is at most 64. Nodes are 32
bytes; triangles store `v0`, `e1`, `e2` and their face.

`trace(origin, direction, tmax, hit)` (t in units of `direction`) is ordered (nearer child first by
the split axis sign) with a fixed per-call stack, so any number of threads trace one scene. The
triangle test is WorldShade's two-sided Möller–Trumbore (`|det| < 1e-12` rejected, `u ∈ [0, 1]`,
`v ≥ 0`, `u + v ≤ 1`, `0 ≤ t ≤ limit`); equal distances go to the lower face index.

- Opaque, sky-portal and unresolved faces stop the ray; an opaque face ignores texture alpha like
  the renderer.
- A translucent face reads its mip-0 alpha at the hit (nearest, repeating, as the alpha pass
  draws it): 0 is a cutout and is skipped, 255 stops the ray, anything between is a layer.
- The nearest nine layers are kept. Up to eight pass through and are returned nearest first with
  their alpha; a ninth stops the ray as `LayerLimit`. Layers beyond the opaque hit are dropped.
- `GiHit`: kind (Miss, Surface, Sky, LayerLimit), back (`dot(direction, normal) > 0`), t, u, v,
  face and the layers.

The tracer and bake sources compile with `-ffp-contract=off`, and ray directions use rejection
sampling (`gi_uniform_direction`: + − × ÷ and `sqrt` only, no library trigonometry), so results do
not depend on the optimization level: the Debug ASan and RelWithDebInfo builds give the same
survey digest.

### Jobs and randomness

`run_gi_jobs(count, threads, cancel, job)` runs jobs on SDL threads (the caller is thread 0; a
thread that fails to start leaves its share to the others). An atomic counter hands out indices;
the cancel flag is read before every job and no job starts once it is set. Results never depend
on the thread count: jobs write their own slots and draw from `GiRandom(stream, index)`, a
splitmix64 sequence seeded from their own indices. `gi_default_jobs()` is the logical core count.

### Settings and survey

| Preset | Bounces | Rays |
| --- | --- | --- |
| `preview` | 1 | 64 |
| `final` (default) | 2 | 256 |
| `custom` | `--gi-bounces` 1–3 (default 2) | `--gi-rays` 16–4096 (default 256) |

Lot G5's INFO text was `openblood2-gi-settings 1`, then `quality`, `bounces` and `rays` lines;
lot G7 makes it version 2 with a `sky` line ([settings](#settings-file-and---bake-gi)). The
thread count is never part of it. `survey_gi_probes` casts each light probe's rays (directions
from `(probe index, ray)`) to infinity and counts front, back, sky, miss, layer-limit hits and
layers; a probe is **inside** when more than half its rays hit back faces. Each probe hashes every
hit (kind, side, face, t bits, layer count); the survey digest is the SHA-256 of the per-probe
records in order. Progress is reported about every 5 % of the probes.

### `bake_gi_world` and `--bake-gi`

1. The world must be mounted (exit 2); the DAT is read and parsed and `gi_expectation` computed
   (exit 5).
2. Placement: a Ready file is used verbatim; Missing gives the defaults with automatic reflection
   probes; Foreign or Failed fails (exit 4). The light probes are resolved (exit 4).
3. `--gi-if-stale`: when the output file is Ready for this world, albedo, edition, algorithm,
   settings hash and placement key, the bake reports `current` and traces nothing. A committed
   placement is checked before any geometry work.
4. Scene (exit 5 on a mesh or scene failure, 4 on an unmatched exclusion), then the survey, then
   the lot G7 sky, receivers and passes (exit 4).
5. The file: key from the expectation, `key.settings` = SHA-256 of the INFO text, `key.placement`
   = `gi_placement_hash`, sections INFO, PLAC, LMAP and `VTX `. It is written atomically to
   `--gi-out` (default `user_directory()/gi`) under `gi_file_name`, creating folders.

Cancellation is checked before the scene, during the survey and the passes, and before writing:
a cancelled bake exits 12 and writes nothing, so an existing file is kept.

`--bake-gi WORLD` (a name or resource path) runs it with `--gi-quality`, `--gi-bounces`,
`--gi-rays` and `--gi-sky` (custom only, the last from lot G7), `--gi-jobs` 1–256, `--gi-out DIR`,
`--gi-if-stale` and the placement directory of lot G4 (`--gi-placement-dir`, the plan's
`--gi-placement`). SIGINT and SIGTERM set the cancel flag. It prints `GI_PROGRESS stage=survey
done=N total=M` lines and one line `GI_RESULT status=baked|current|failed|cancelled exit=N
world=… quality=… bounces=… rays_per_probe=… jobs=…`, then the error or file and, for a bake, the
placement status, probe and face counts, BVH size, threads, rays, `rays_per_second`,
`rays_per_second_per_thread`, the survey counts, `digest`, `scene_ms`, `survey_ms`, the lot G7
fields, `bytes` and `ms`. Exit 0, 2 (arguments or data), 4 (placement, bake or write), 5 (world
load), 12 (cancelled). It is a separate headless tool, like `--gi-place`.

### Tests

**`gi_bake.bvh`** 6,002 random triangles in two ranges (one offset by its pivot): 20,000 rays,
axis-aligned and bounded ones included, give the same face and t as brute force; equal faces tie
to the lower index; negative tmax and an empty scene miss.

**`gi_bake.alpha`** Cutout, partial layer, alpha-255 stop and an opaque face ignoring alpha;
bounded segments; 8, 9 and 12 layers (eight pass, the ninth stops); sky portal and unresolved
faces; Sky ranges only in the sky tree and Skip ranges nowhere; a use list of the wrong size.

**`gi_bake.albedo`** Mip choice (clamping, flat UVs), sampling of the chosen level, vertex light
and a lightmapped face's page sample × 2.

**`gi_bake.jobs`** Every slot written once; zero jobs; cancellation stops new jobs; a pre-set
flag runs none; `GiRandom` and `gi_uniform_direction` reproducible, unit length and centred.

**`gi_bake.settings`** Presets, custom defaults and ranges, refusals and the INFO text.

**`gi_bake.determinism`** A closed room with glass and a sky portal: the survey digest and
counts are identical for 1, 2 and 7 jobs, ordered progress, a different ray count changes the
digest, a pre-set cancel flag cancels.

**`gi_bake.retail`** (private data) 01_SubwayA: exclusions by case and an unknown name; 20,000
probe segments where the bake's opaque, unlayered hits equal WorldShade's (fraction and light);
Preview bakes with 1 and 4 jobs give identical bytes and digests and a Ready file; `--gi-if-stale`
reports current; a cancelled bake exits 12, keeps the previous file and writes nothing into an
empty folder; an unmounted world (2) and a Foreign placement (4); throughput is printed.

**`config.gi`** The `--bake-gi` options and every refused combination.

## Bake-all script and packaging (lot G6)

`tools/gi/bake_all.py` bakes a set of worlds with the game binary, and the build copies the
results where the runtime's packaged root looks (`SDL_GetBasePath()/gi`). Baked files are
derived from commercial data: they stay local (decision D3).

### `tools/gi/bake_all.py`

Standard library only, run as `python3 -I tools/gi/bake_all.py`. The binary owns every format
and staleness decision: the script never opens a `.ob2gi` file.

- **Worlds** come from `blood2 --list-worlds --json`. `--set single` (default) is the `single`
  and `addon` kinds, `multi` the `multi` kind, `all` everything (`other` included). `--worlds`
  names worlds by path, file name or stem, case-insensitive; an unknown name is a setup error.
- **Each world** runs `blood2 [--data-dir D] [--nightmares|--demo] --log-level error --bake-gi
  PATH --gi-quality Q --gi-jobs T --gi-out OUT --gi-placement-dir P [--gi-bounces B]
  [--gi-rays R] [--gi-if-stale]` from the repository root. `--gi-if-stale` is dropped by
  `--force`. `--bounces`/`--rays` need `--quality custom`.
- `--edition retail|nightmares|demo` adds `--nightmares` or `--demo`; `--data-dir` is passed
  through (absent: the game's own configuration). `--blood2` defaults to `build/release/blood2`,
  then `build/dev/blood2`.
- `--jobs N` (1–64, default 1) bakes N worlds at once; `--threads T` (0–256) is each bake's
  `--gi-jobs`, by default the processor count divided by the jobs.
- **Status** from the last `GI_RESULT` line and the exit code: `baked` + 0 is PASS, `current` + 0
  is SKIP, exit 12 or `cancelled` is CANCELLED, anything else is FAIL (the error, or the last
  output lines when there is none). A failed world never stops the run.
- **Output:** one `GI_BAKE <world> PASS|SKIP|FAIL|CANCELLED` line per world (probes, reflections,
  bytes, ms, the error), then `GI_BAKE_TOTAL worlds= pass= skip= fail= cancelled= not_run=
  seconds= report=`.
- **Report:** `--report` (default `data/gi/bake_report.json`, ignored by git), written
  atomically: schema 1, start time, duration, binary, edition, selection, quality, force, jobs,
  threads, output folder, cancelled, totals, pruned files and one entry per world (status, exit,
  seconds, error and the numeric `GI_RESULT` fields).
- **Cancellation:** SIGINT and SIGTERM send SIGINT to every running bake (terminate on Windows);
  bakes exit 12 and write nothing, worlds not yet started are NOT_RUN, the report records the
  cancel and the script exits 12.
- `--prune` removes, after a run that was not cancelled, every `.ob2gi` under the output folder
  whose relative path is no mounted world's `gi_file`, selected or not (`GI_PRUNE` lines, listed
  in the report).
- `--list` bakes nothing: `GI_LIST <world> READY|STALE|INVALID|FOREIGN|MISSING|ERROR` from the
  file's presence and `--gi-verify`'s exit code and `stale` line.
- `--dry-run` prints each bake command (and `--prune`'s candidates) and runs nothing.
- Exit 0 (every world PASS or SKIP), 1 (a FAIL), 2 (arguments, binary, listing, selection or
  report), 12 (cancelled).

### Packaging (`cmake/GiPackaging.cmake`)

- `BLOOD2_GI_DATA_DIR` (default `data/gi/baked`, the script's output) and `BLOOD2_PACKAGE_GI`
  (default ON; `ci-release` and the CI APK jobs set it OFF).
- A `CONFIGURE_DEPENDS` glob finds `**/*.ob2gi`; configure prints `Packaged GI files: N`. With
  none, nothing is added to the build. Otherwise each file is its own `copy_if_different`
  command and the `blood2_gi_data` target (in ALL, a dependency of the game) holds them, so a new
  bake is copied by the next build without relinking and an unchanged tree has no work. This
  replaces the plan's POST_BUILD `copy_directory_if_different`, which runs on every link and
  cannot leave the empty case free.
- Destinations keep the `worlds/` subfolders: `<executable folder>/gi` (`$<CONFIG>/gi` with a
  multi-config generator); Apple bundles take the files as resources at
  `MACOSX_PACKAGE_LOCATION Resources/gi/…` (`Contents/Resources/gi` on macOS, the bundle root's
  `gi` on iOS); Android stages them in `<build>/android/assets/gi`, which `blood2_apk` passes as
  `-Pblood2.assets` and Gradle adds as an optional `assets` source folder (`assets/gi` in the
  APK, read through SDL I/O's `assets://` base path).
- Copies are never removed by the build: a file deleted from the data folder stays in an existing
  build tree until it is cleaned. The runtime picks files by their expected name, so a leftover
  is reported by F8 but never used for another DAT.

### Decision D3 guard

- `.gitignore` ignores `/data/gi/baked/` and `/data/gi/bake_report.json`; placement and settings
  stay committed.
- `cmake/CheckGiPolicy.cmake` (the `gi_policy` test) fails when git tracks a `.ob2gi` file,
  anything under `data/gi/baked` or the report, or when `.gitignore` stops ignoring them; outside
  a git checkout it reports a skip.
- `cmake/PackageCi.cmake` refuses to package when `build/ci/game/gi` exists.

### Tests

**`gi_policy`** The repository check above (verified failing on a tracked `.ob2gi` and on a
missing ignore rule in a scratch repository).

**`gi_bake_all`** (Unix, Python 3) The script against a stub binary: PASS, FAIL with a quoted
error, the totals and report, `--gi-if-stale`/`--gi-jobs`/`--log-level` forwarding, a second run
skipping, `--force`, `--list` (ready, stale with reasons, missing), `--dry-run` running nothing,
`--prune` keeping unselected worlds' files, an unknown world (exit 2), and SIGINT to the script
reaching a running bake (exit 12, later worlds NOT_RUN, nothing baked after the cancel).

## GI lightmap core (lot G7)

`engine/gi_lightmap.{h,cpp}`, `engine/gi_sky.{h,cpp}` and `engine/gi_preview.{h,cpp}` compute the
indirect light of every stored lightmap texel and baked vertex colour, and the bake writes it into
the LMAP and `VTX ` sections after the survey. Lot G8 composes them into the world
([world override](#world-override-lot-g8)); nothing here changes a save or the simulation.

### Units (decision D1)

- **E** is the cosine-weighted mean of the display radiance arriving at a sample: raw albedo (0–1)
  times the light the renderer draws, per channel, unbounded. The sky adds its own radiance.
- **Pages draw as 2 × Saturate, vertex light does not.** A lightmapped face shows
  `albedo × 2 × min(page, 1)` (`world_saturate_scale`); a vertex-lit face shows
  `albedo × min(colour, 1)`. A texel therefore adds `E × 255 / 2` to its 8-bit value and a corner
  adds `E × 255`.
- **Composition** (`gi_compose`, one 8-bit channel O, AmbientLight A, intensity k, floor f):
  `A_eff = A > 0 ? min(O, A) : 0`, `D = O − A_eff`, indirect `= k × max(E, 0) × s` with s = 255/2
  for a texel and 255 for a corner, and `out = clamp(floor(D + max(indirect, f × A_eff) + 0.5),
  0, 255)`. A NaN E or indirect counts as 0. **E = 0 with f = 1 gives O exactly**, for every O,
  every A (negative, NaN and infinite included) and every k. A is `world_info_ambient_any_case`
  (lot G1), zero when the info has none.
- **Defaults** (Gate B, 2026-11-07): `gi_default_k` = 2 and `gi_default_f` = 1. They are
  composition parameters that G8 applies when it builds the world mesh; changing them needs no new
  bake.
- The ambient fill is never bounced: the light a hit re-emits is built from D, never from O.

### Receivers

`WorldMesh::build` reports, when asked, one `WorldTriangleSource` per emitted triangle: its model,
polygon, the three base-ring corners (0, i, i + 1) and how the mesh lights it (`WorldFaceLight`):
**Sky** (a sky portal), **Lightmap** (its stored page), **Vertex** (baked colours), **Bright**
(SURF_BRIGHT, or a textured face without baked colours) or **Untextured**. The mesh itself does not
change. `gi_receivers` builds, from the Main ranges only:

- **Blocks:** one per polygon the mesh lights with its lightmap, sorted by (model, polygon). The
  `lightmap_frame` must exist with the polygon's stored width and height, and its texels must lie
  in the model's block; otherwise the bake fails (exit 4). At most 4 Mi texels.
- **Sets:** one per vertex-lit polygon and per lightmapped polygon that also has a colour per
  point. The second kind is **DETAIL LOW**: its corners show only when lightmaps are off. Corners
  are the base ring (`base_points`, else every point). At most 4 Mi corners.
- **Elements:** every block's texels row-major, then every set's corners; element indices run over
  both. Each element keeps its owner, its stored light (RGB565 expanded like the mesh, or the
  colour) and its sample.
- **Texel samples:** the texel centre (`LightmapFrame::position(i, j)`) stays where it is when it
  lies inside the base ring and at least `edge_inset` (1 unit) from every edge. Otherwise it moves
  to the closest boundary point, then toward the centroid by min(1, half the distance), so a
  sample never sits on the plane of the wall it meets. Corners move toward the centroid the same
  way. Every sample is then raised 0.5 units along the unit plane normal.
- **Solid samples:** a sample whose root-BSP `point_contents` is `Solid` is never traced.
  `Unknown` counts as open. World models are not carved into the root BSP: the back-face rule
  below catches samples inside them.
- **Links:** each scene face gets the receiver that lights it (bilinear texel coordinates of its
  vertices for a block, set corners for a set), or Bright, Sky or Black (untextured faces, and
  lightmapped or vertex faces outside Main ranges).
- **Neighbours:** receivers sharing a base-ring edge (the same two point indices in one model) with
  a receiver of the same kind (block, vertex set or DETAIL LOW set) whose unit normal has a dot
  product of at least 0.9 with theirs.

### Gathering

- **Directions:** ray i of n follows the rank-1 lattice `((i + 0.5)/n, i/φ)`, shifted by
  (s1, s2) and wrapped, through the concentric disk map to the cosine-weighted hemisphere around
  the receiver's unit plane normal (the Duff et al. basis). Only `+ − × ÷` and `sqrt` are used:
  `gi_quarter_sin_cos` is a Taylor series on [−π/4, π/4]. The shift comes from
  `GiRandom(seed, element, pass)`, and the seed is the first eight bytes of the world DAT
  SHA-256, little-endian. Results never depend on the thread count.
- **Passes:** pass 1 traces every element that is not solid and not DETAIL LOW. A later pass
  traces the valid ones again. DETAIL LOW sets feed no lookup, so they are traced once, in the last
  pass. Pass b reads only the state left by pass b − 1: passes are separated by barriers.
- **Radiance at a hit** (decisions D9 and D10), from the current state:
  - lightmapped face: `albedo × 2 × bilinear(min(D + E/2, 1))` over its block;
  - vertex-lit face: `albedo × min(D + E, 1)`, interpolated over its set's corners;
  - SURF_BRIGHT face: its albedo (manifest emission is not baked);
  - untextured face: 0 (counted);
  - sky portal or miss: the sky radiance in the ray direction;
  - back face (`dot(direction, normal) > 0`): 0 (counted);
  - translucent layers composite in front of the hit, nearest first (`out += α T L`,
    `T ×= 1 − α`); a ninth layer (`LayerLimit`) adds only its own α share.
  D is `(O − A_eff) / 255`; before pass 1, E is 0, so the first bounce re-emits the original
  direct light.
- **Validity:** an element is valid when at most 10 % of its rays (`back_limit`) hit back faces on
  its first trace (pass 1, or the last pass for DETAIL LOW). `back_fraction` counts first traces
  by tenth of back-face rays.
- **E** of a traced element is the mean radiance of its rays. After each pass and its dilation,
  the state becomes `min(D + E/2, 1)` for texels and `min(D + E, 1)` for corners.

### Dilation

After every pass, elements that are not valid (or not traced) take values in three steps:

1. **Inside each block:** rounds over a snapshot. Each missing texel with at least one known
   8-neighbour takes their mean and becomes known for the next round, until the block is full.
   A block with no valid texel is left for step 3.
2. **Inside each set:** missing corners take the mean of the valid corners.
3. **Across shared edges:** a receiver with no valid element takes the mean of its neighbours'
   receiver means, in at most 8 rounds; a filled receiver can fill others in the next round.
   Receivers still empty are 0 and counted as `unfilled`.

Validity bits record each element's own sample, not whether it was filled.

### Sky (decision D9)

`gi_sky_setup(world)` replays what the DAT objects set up once creation, initial updates and first
updates have run:

- DemoSkyWorldModel (file order) adds its own model to slot `Index` at its initial update.
- SkyPointer (file order, at its first update, so after every initial update) adds the model of
  the first object whose `Name` equals `SkyObjectName` exactly (case-sensitive, like
  `FindNamedObjects`) to slot `Index`, replacing the slot's holder. A target already in the sky
  keeps its slot. A target that is missing or binds no world model is counted as unresolved.
- The last of either class with three nonzero `SkyDims` defines the sky box and the view box.
- Objects the original precreate refuses (`Index` outside 0–29, negative or non-finite `SkyDims`
  or `InnerPercent`) add nothing, and neither does an object whose definition is rejected.
- SkyFog comes from the last WorldProperties object and applies only when `FogEnable` gives a
  valid fog colour, as in the client's `ResetGlobalFog`.

The models of every registered object are drawn as sky (FLAG_SKYOBJECT): `gi_range_uses` gives
them the Sky use, SkyPointer targets (doors and brushes) included, so they never block bake rays.

`GiSky::build` paints a cube map (6 faces of 64², 2×2 samples per texel) from the sky eye of the
world centre (`sky_view_position`, the linear map from the world box to the view box), as the sky
pass draws:

- slots in order, front faces only, no depth test, each range's triangles in submission order;
  opaque faces replace the colour, translucent faces blend with their texel alpha;
- distances in (0.01, 65536], the sky pass projection planes;
- a texel holds albedo times baked light (sky-portal and untextured faces show their vertex
  colour, like the renderer), fogged by SkyFog over the radial distance (the sky pass uses view
  depth, which a cube shared by every view cannot follow);
- directions no model covers take the solid-angle average of the covered colour, or black when
  the sky draws nothing.

Like the sky pass, an invalid definition fails the bake only when a visible object holds a slot.
`radiance(direction)` is the bilinear texel within the direction's face (edges clamped) times the
**sky scale**; `displayed` omits the scale.

### Sections

Both payloads start with u32 format (1), u32 records and u32 values, then 8-byte records, the
u32 RGB9E5 values and the validity bits:

- **LMAP:** one record per block (u16 model, u16 polygon, u8 width, u8 height, u16 zero), then
  one E per texel;
- **`VTX `:** one record per set (u16 model, u16 polygon, u16 corners, u16 zero), then one E per
  corner.

- Records are sorted by (model, polygon) and unique. Values follow in record order (texels
  row-major). Validity bits are LSB-first with zero padding.
- **RGB9E5** is the `GL_EXT_texture_shared_exponent` rule (9 mantissa bits, bias 15): negative and
  NaN channels encode 0, values above 65,408 saturate, and only `frexp`/`ldexp` are used, so it is
  exact.
- The readers are strict about format, counts, sizes, order, reserved fields and padding.
  `check_gi_lightmaps`/`check_gi_vertex_light` also require the records to name polygons with a
  stored lightmap of that size, or with baked colours and that base ring.

### Settings, file and `--bake-gi`

| Preset | Bounces | Rays | Sky scale |
| --- | --- | --- | --- |
| `preview` | 1 | 64 | 1.00 |
| `final` (default) | 2 | 256 | 1.00 |
| `custom` | `--gi-bounces` 1–3 | `--gi-rays` 16–4096 | `--gi-sky` 0–4, at most two decimals |

- The INFO text is `openblood2-gi-settings 2`, then the `quality`, `bounces`, `rays` and
  `sky X.YY` lines, so the sky scale is part of the settings key. `gi_algorithm_version` is 2:
  files from lot G5 become stale.
- The bake runs the survey, then the sky, the receivers and the passes; exit 4 for a sky,
  receiver or section failure. Cancellation is also checked during every pass.
- `GI_PROGRESS stage=bounce b/B done=N total=M` lines report each pass. `GI_RESULT` adds
  `sky_scale`, the sky counts (`sky_slots`, `sky_triangles`, `sky_covered`, `sky_average`,
  `sky_pointers`, `sky_unresolved`, `sky_fog`, `sky_ms`) and the lightmap counts (`blocks`,
  `texels`, `sets`, `corners`, `low_corners`, `solid`, `back_invalid`, `filled`,
  `neighbour_filled`, `unfilled`, `lightmap_rays`, `lightmap_back`, `lightmap_sky`,
  `lightmap_untextured`, `lightmap_layer_limit`, `back_fraction` as ten counts, `pass_ms` per pass
  and `lightmap_ms`).

### `--gi-preview` (diagnostic)

`--gi-preview WORLD` reads the Ready baked file of a mounted world from `--gi-out` (any settings
and placement), rebuilds the bake scene, sky and receivers, and writes one PNG per view under
`--gi-preview-dir` (default `user_directory()/gi/preview`): three 480×360 panels, **Original |
GI | Indirect**, CPU-rendered with a 90° horizontal field and 2×2 samples per pixel.

- Views: the first GameStartPoint at its yaw, then turned by 120° and 240°, then the other start
  points, six at most; eyes 36 units above `Pos`. A world without one uses its centre.
- Faces show albedo times their drawn light (pages doubled): the stored light, then the D1
  composition with `--gi-k` and `--gi-f` (0–4, default `gi_default_k` and `gi_default_f`). The
  indirect panel shows
  `albedo × k E × 4`. The sky shows as drawn, unscaled.
- `GI_PREVIEW_RESULT` reports the mean E of valid texels and corners, the fraction of 8-bit
  channels the composition changes, the mean change and the file's settings. Exit 2 (arguments or
  data), 4 (no current file, sections that do not match the world), 5 (world load).

The game never uses this tool; it exists to choose the k, f, bounce and sky defaults (Gate B).

### Known limits

- **Coplanar twins.** The tracer's equal-distance rule (the lower face index wins) means that of
  two opaque faces in one plane facing opposite ways, a ray reaching them from one side can hit
  the back of the other. Retail data has 2 drawn opaque twin pairs, both in
  09_Abandoned_Buildings; the leak test shows the effect on a synthetic zero-thickness wall.
- **13_Downtown's sky** covers 0.2 % of the cube: two DemoSkyWorldModel boxes bind the same
  model name, the first match wins and the last definition is kept, so the bake sees what the
  runtime draws.
- Model shadows and dynamic lights are not part of the bake; neither is manifest emission (D10).

### Tests

**`gi_lightmap.identity`** E of 0, −0, a negative value or NaN with f = 1 returns O for every O,
texel or corner, on a grid of 232,960 cases (negative, NaN and infinite A; infinite and NaN k);
worked values of the formula.

**`gi_lightmap.rgb9e5`** Exact round trips, saturation, NaN and negative channels, rounding into
the next exponent, the smallest exponent, and 100,000 random values within half a step.

**`gi_lightmap.sections`** LMAP/VTX round trips; refusals of every count, size, order, reserved
field and padding byte; `check_gi_*` against a synthetic world.

**`gi_lightmap.directions`** The quarter sine/cosine against the library (< 1e-13), unit length,
hemisphere, measured cosine moments and mean direction per ray count, shift wrapping.

**`gi_lightmap.placement`** A synthetic world with a triangle (DETAIL LOW), two vertex squares, a
perpendicular wall, a bright quad, a half-solid BSP and AmbientLight: layout, every sample against
a restated rule, the edge corrections, owners, neighbours, links, bilinear lookup, section checks
and identical samples for 1 and 7 jobs.

**`gi_lightmap.furnace`** A closed box of uniform albedo and light: bounces 1–3 match
`E_b = 2aD(1 + a + … + a^(b−1))` (pages) and `aD(1 + …)` (vertex) to 2e-6 relative; exact ray
counts.

**`gi_lightmap.box`** A floor under a grey bright ceiling and black bright walls, against the
analytic form factor: worst error below 2e-3 (lightmap and vertex floors, 4,096 rays).

**`gi_lightmap.planes`** A white emitter above a receiver, with a uniform sky: worst error below
3e-3.

**`gi_lightmap.leak`** Two rooms sharing a wall: the dark room's E is exactly 0, its samples stay
on its side, the lit room's floor is fully lit.

**`gi_lightmap.dilation`** Blocks with solid texels, a solid block filled from its neighbour, an
isolated block and set left at 0, and the report counts, against a restated dilation.

**`gi_lightmap.back`** A sample under a large overhanging back face is invalid and unfilled; a
small one stays valid with the expected energy.

**`gi_lightmap.determinism`** Identical E and validity for 1, 2 and 7 jobs; another seed differs.

**`gi_lightmap.sky`** Setup rules (refusals, last definition, pointer replacement, case-sensitive
targets, unresolved targets, fog), a six-colour sky box seen from the mapped eye, texel-centre
mapping, the scale, the hidden-holder rule, partial coverage and fog.

**`gi_lightmap.preview`** Views, panel values, the section round trip, a mismatch refusal and a
uniform render.

**`gi_lightmap.retail`** (private data) 01_SubwayA at Preview: the file's sections pass the
checks, match the receivers and the report, every traced element is counted, and the
composition is the identity at E = 0 on real data.

## World override (lot G8)

`engine/gi_runtime.{h,cpp}` loads the current world's Ready file, and `WorldMesh::build` composes
its LMAP and `VTX ` sections (decision D1) into the lightmap pages and vertex colours it emits.
Nothing is baked or written at load, and nothing outside those page texels and vertex colours
changes: geometry, UVs, materials, batches, collision, saves and the simulation are untouched.

### Switch, preferences and command line

- Options > Display ends with `GLOBAL ILLUMINATION [OFF/ON]` (setting 31, the 16th row), off by
  default until Gate C and the defaults of G13 (decision D11). It serves both renderers, every
  detail level and XR (the composition is CPU-side; no shader changes).
- `preferences.cfg` **version 13** adds `global_illumination` (0 or 1, default 0). Versions 1–12
  load it off; a version 12 file carrying the key, a version 13 file without it and any other
  value are refused.
- The F8 Runtime checkbox is the same setting, so it is saved like a Display change.
- `--gi` applies the baked GI to `--bench` and the smoke runs (`--gpu-smoke`, world smokes),
  composed with `--gi-k`, `--gi-f` (0–4, defaults 2 and 1) and `--gi-view final|original|direct|
  indirect|lighting|validity`. It is refused with the GI tools, network sessions and the game
  itself, where the Display row and F8 own it; `--gi-view` requires `--gi`.

### Lookup

`ScenePresenter` looks the file up when GI is on and the world changes, when GI is turned on, and
when F8 asks again (`gi_reload`); never per frame. `load_gi_world_light`:

1. computes the content keys (`gi_expectation`: DAT and albedo digests);
2. runs `find_gi_file` over `gi_search_roots(config.gi_dir)` (the G2 order: `--gi-dir`, user,
   sdcard, packaged); the first Ready candidate wins and any bake settings are accepted;
3. decodes LMAP and `VTX ` (both required: an older file fails) and checks them against the
   world (`check_gi_lightmaps`, `check_gi_vertex_light`).

States: **Off** (nothing looked up, decoded data dropped), **NoWorld**, **Missing** (no Ready
candidate; the detail names each Stale, Foreign or Failed candidate with its reason), **Failed**
(keys, lookup or sections; the detail names it) and **Applied**. A file is applied whole or not at
all, and its payload is released once decoded. The game logs one line per lookup: what was
applied with its counts and load cost (keys, lookup, sections in ms), or why nothing was; and the
world mesh build time of each rebuild that composed GI.

### Composition

- **Pages:** each texel of a listed block (LMAP record of the same model and polygon) is expanded
  from RGB565 as before, then replaced by `gi_compose(O, A, E, k, f, texel)` per channel (s =
  255/2). The replicated borders copy the composed texels. Page alpha stays 255.
- **Corners:** a listed set (`VTX ` record) whose ring matches the polygon's composes each
  corner's colour (s = 255): vertex-lit polygons at every detail level, and lightmapped polygons
  with baked colours at DETAIL LOW.
- **A** is `world_info_ambient_any_case` (zero without AmbientLight).
- Unlisted polygons, fully bright faces, sky portals, untextured faces, FX geometry and models
  keep their stored or original light.
- **E = 0 with f = 1 is the exact identity** for every k: GI on with a zero bake, like GI off, a
  missing file or a stale one, presents byte-identical frames in both renderers (`gpu.gi`).
- The presenter rebuilds the world mesh when the world, the detail level, the applied file
  (lookup count), k, f or the view changes; the rebuild clears the presentation textures as every
  world rebuild does. k and f outside [0, 4] fall back to the defaults.

### Views

Every view recomposes the pages and corners from the decoded file; none needs a bake.

| View | Pages and corners |
|---|---|
| Final | `gi_compose(O, A, E, k, f)` |
| Original | The stored light, byte for byte |
| Direct only | `gi_compose(O, A, 0, 0, 0)`: the stored light without its ambient fill |
| Indirect only | `gi_compose(0, 0, E, k, 0)` |
| Lighting only | Final, with every decoded world texture drawn white (texture alpha kept) |
| Texel validity | Green where the element's own sample was valid, red where it was filled or left |

### F8 Runtime panel

- **Global illumination** checkbox (the Display setting) and the runtime state line.
- **Look up again** (after a bake, for example).
- When applied: the bake settings, the file's blocks, texels, corner sets and corners, the load
  cost, and what the current mesh composed with the world mesh build time.
- **Intensity k** and **Ambient floor f** sliders (0–4), **Defaults** (k 2, f 1); they last for
  the session and are not saved.
- **View** buttons for the six views. The Pinned strip shows the view, k and f while GI is on.

### Network and server

A network client loads GI from its own content (its mounted data and its own `gi_dir` roots),
never from the server; nothing about GI goes over the wire. The dedicated server has no presenter
and never looks a file up.

### Interim effects (until G9 and G12)

- **Models** sample the face light below them from the world mesh (`ModelLighting`, `WorldShade`),
  so with GI on they see the composed light until the light probes (G9) replace model lighting.
- **Model shadows** remove 60 % of the composed light below a caster, GI included.
- **Enhanced renderer:** the static-light specular occlusion (`saturate(baked / predicted)`) and
  the normal-map relief read the total, GI-composed luminance, so GI raises grid specular where it
  adds light until the reflection probes (G12) separate the terms.

### Cost

`--bench --gi` on Apple M5 (960×540): see the
[G8 report](../implementation/reports/gi_g8_world_override.md). Composition runs only when the
world mesh is rebuilt (SubwayA 5.6 ms with GI); frames draw the same pages and vertex colours as
before, so the frame cost does not change. The lookup costs about 13 ms on SubwayA (content keys
6 ms, file read and validation 6 ms, sections 0.2 ms).

### Tests

**`gi_runtime.override`** A synthetic box of lightmapped, vertex-lit and DETAIL LOW faces with
AmbientLight: E = 0 at Final is byte-identical to no GI for k 0–4 at both detail levels, with the
composed counts; a uniform E per block and varying corners give `gi_compose` pages and colours at
Final (two k/f pairs), Original (identical), Direct, Indirect, Validity and Lighting (white
textures, alpha kept, pages unchanged); an unlisted polygon keeps its stored light; files without
`VTX `, with a block or a set that does not fit the world are refused untouched; composition
bounds and record lookup.

**`gi_runtime.retail`** (private data) 01_SubwayA through the runtime lookup: NoWorld, Missing
with no file, Missing naming a stale (albedo) file, then a Ready zero bake applied with its counts
and byte-identical meshes at high and low detail, then a nonzero bake changing the pages.

**`gpu.gi`** (private data, GPU) 02_CityHub and 01_SubwayA in the original and the enhanced
renderer at high and low detail: GI off is the reference; GI on with no file, a stale file and a
zero Ready file are byte-identical; a nonzero file changes the frame, the Original view and GI off
restore it, and Lighting only differs.

**`menus.state`** The 16th Display row toggles setting 31. **`menus.preferences`** Version 13
bytes, version 12 loading off, and the refused key/value combinations. **`config.gi`** `--gi`,
`--gi-view` and their refusals. **`gi_overlay.draw` / `gi_overlay.retail`** The Runtime panel with
and without the presenter link, and the summaries per runtime state.
