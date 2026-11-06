# Global illumination contracts

Baked GI for the M10 [GI plan](../implementation/gi_plan.md). Each lot adds its section.
GI is not rendered yet: nothing in this contract changes a frame, a save or the simulation.

- [World queries (lot G1)](#world-queries-lot-g1)
- [Baked file and keys (lot G2)](#baked-file-and-keys-lot-g2)
- [F8 overlay (lot G3)](#f8-overlay-lot-g3)
- [Probe placement (lot G4)](#probe-placement-lot-g4)
- [Bake scene and tracer (lot G5)](#bake-scene-and-tracer-lot-g5)

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
content, the freshness rules and the search order. No bake writes one yet (G5); nothing reads
one at runtime yet (G8).

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
Status works since G3 and the two probe panels since [G4](#f8-placement-editing); Runtime and Bake
name the lot that brings them (G8, G10).

**Status** shows, truthfully:
- the summary: no world, error, no baked file, a stale file with its reasons, a refused/foreign
  file, or "ready file, not applied (GI rendering: lot G8)";
- that GI rendering is disabled until G8 and the original lightmaps are in use;
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
and the world, deterministically, so the file stays small and reviewable. Nothing is baked or
rendered from it yet (G5, G8).

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
world and its placement into a `.ob2gi` file through a deterministic CPU tracer. Lot G5 traces a
probe survey only: the file holds INFO and PLAC, and the lighting sections arrive with G7 (LMAP,
`VTX `) and G9 (PROB). Nothing here changes a frame, a save or the simulation.

### Bake scene

`GiBakeScene::build(mesh, uses)` takes a `WorldMesh` built with resources and lightmaps on (the
faces the renderer draws) and one use per range:

- `gi_range_uses(world, excluded)` gives Main to the Primary, Mover and Destructible models, Sky
  to DemoSkyWorldModel models and Skip to containers and unbound models (`world_model_roles`).
  Every name in the placement's `excluded_models` must match at least one non-primary model
  (case-insensitive; all matches are skipped) or the bake fails with exit 4.
- Positions are local + pivot with no rotation: the authored pose (decision D8). Sky ranges go
  to a separate tree (`trace_sky`) for the G7 sky pass (decision D9); they never block bake rays.
- Each triangle keeps its face record: range, material, lightmap page, flags, plane normal, UVs,
  lightmap coordinates and vertex colors. Flags: **Translucent** (a SURF_TRANSPARENT batch with
  a material, chroma-keyed copies included), **SkyPortal** (a sky face; rays end there with
  `Sky`), **Lightmapped** and **Unresolved** (no material: opaque, albedo 0.5 grey).
- **Albedo** (`albedo(face, u, v)`): raw stored RGB (0–1; G7 fixes the colour space), nearest
  texel, repeating, from mip `round(log2(20 × texels per world unit))` clamped to the image's
  levels (`gi_albedo_mip`; texels per unit is the square root of the texel area over the world
  area). An SPR uses its first frame, the presentation's initial texture. Alpha is the mip-0
  texel alpha the tracer used, 1 for opaque faces.
- **Baked light** (`baked_light(face, u, v)`): WorldShade's rule, bilinear page sample ×
  `world_saturate_scale` for lightmapped faces, else the interpolated vertex colors.
- Images are checked once (dimensions, RGBA size; mips used only when `mip_rgba` has exactly
  `texture_mip_bytes`); at most 4 Mi triangles.

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

The INFO text is `openblood2-gi-settings 1`, then `quality`, `bounces` and `rays` lines; the
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
4. Scene (exit 5 on a mesh or scene failure, 4 on an unmatched exclusion), then the survey.
5. The file: key from the expectation, `key.settings` = SHA-256 of the INFO text, `key.placement`
   = `gi_placement_hash`, sections INFO and PLAC. It is written atomically to
   `--gi-out` (default `user_directory()/gi`) under `gi_file_name`, creating folders.

Cancellation is checked before the scene, during the survey and before writing: a cancelled bake
exits 12 and writes nothing, so an existing file is kept.

`--bake-gi WORLD` (a name or resource path) runs it with `--gi-quality`, `--gi-bounces`,
`--gi-rays` (custom only), `--gi-jobs` 1–256, `--gi-out DIR`, `--gi-if-stale` and the placement
directory of lot G4 (`--gi-placement-dir`, the plan's `--gi-placement`). SIGINT and SIGTERM set the
cancel flag. It prints `GI_PROGRESS stage=survey done=N total=M` lines and one line
`GI_RESULT status=baked|current|failed|cancelled exit=N world=… quality=… bounces=… rays_per_probe=…
jobs=…`, then the error or file and, for a bake, the placement status, probe and face counts, BVH
size, threads, rays, `rays_per_second`, `rays_per_second_per_thread`, the survey counts, `digest`,
`scene_ms`, `survey_ms`, `bytes` and `ms`. Exit 0, 2 (arguments or data), 4 (placement, bake or
write), 5 (world load), 12 (cancelled). It is a separate headless tool, like `--gi-place`.

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
