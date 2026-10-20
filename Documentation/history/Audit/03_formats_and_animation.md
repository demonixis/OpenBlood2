# Formats, Coordinates, and Animation

The prototypes contain substantial reusable format knowledge. That knowledge is more mature than the original-game API integration, but it is not yet a verified specification covering every retail asset and game behavior. No commercial archive was parsed during this audit. The execution evidence uses synthetic fixtures only.

## Reader Assessment

| Resource | Observed implementation | Audit result and limit |
| --- | --- | --- |
| REZ | C++ archive readers and managers; C# `RezArchive` and `GameArchiveSet` | C# synthetic archive check passes. Whole archives are retained in memory. Recursive directories lack a visited-offset/depth guard in the inspected implementations. Overlay priority and ambiguous basename resolution need explicit conformance cases |
| DAT | C++ and C# world/object readers; geometry and scene builders | C# synthetic object-scale and geometry-query checks pass. Both parsing families include heuristic object recovery. The C# `56/57/127` predicate identifies a branch; it does not prove complete support for these revisions |
| ABC | Nodes, geometry, bind transforms, animations; additional C# replacement vertices and sockets | C# synthetic parsing and CPU-animation checks pass. C++ malformed section traversal can fail to terminate. Game-event and API integration are incomplete |
| DTX | C++ heuristic format detection; broader explicit C# pixel-format decoding | C# paletted, 32-bit, and DXT1 synthetic tests pass. DXT3/5 branches exist but were not separately validated in this audit. Retail palette, transparency, mip, and material behavior remains unverified |
| PCX | Both language families contain readers | Synthetic C# paletted image test passes; not a complete image corpus |
| Configuration | CFG/JSON consumers and C# Bute parser | Typed Bute test passes. These readers do not execute original game rules by themselves |
| Fonts/video/music | Font and audio facilities exist in C++; SMK/STY research utilities exist | No full original presentation/media pipeline was exercised; no equivalent complete C# sound playback path was found in the inspected effect presenter |

Primary sources: C++ [format directory](../../../../OpenBlood2/OpenLithTech/src/formats), NoMadEngine [format directory](../../../../NoMadEngine/nomadengine/src/formats), C# [Nomad.Formats](../../../../nomad/src/Nomad.Formats), and [test program](../../../../nomad/tests/Nomad.Tests/Program.cs). Existing format notes remain useful hypotheses and historical evidence, not a replacement for parser validation.

## Confirmed Parser Termination Defect

[ABCParser::ParseFile](../../../../OpenBlood2/OpenLithTech/src/formats/abc_parser.cpp), lines 59–102, follows a signed next-section offset until it equals `-1`. It has no visited-offset check, iteration bound, or required forward progress. [BinaryReader](../../../../OpenBlood2/OpenLithTech/src/BinaryReader.cpp) clamps out-of-range seeks and returns zero on exhausted reads rather than carrying an error state. An outer exception handler therefore cannot reliably detect truncation or cyclic input.

The [ABC cycle probe](probes/abc_cycle_probe.cpp) constructs a 12-byte file with an unknown section whose next offset points to itself. The real parser does not return and is terminated after three seconds: exit **124**. [Output](evidence/abc_cycle.log). The source loop explains the result; it is not merely a slow parse of a large file.

The C# [AbcParser](../../../../nomad/src/Nomad.Formats/AbcParser.cs) has a 64-section loop guard and bounds-checked reads, which improve termination behavior. It can nevertheless stop at the guard without reporting a distinct cycle/unsupported-format result. A parser that terminates with a partial object is not equivalent to a parser that reports successful complete interpretation.

Other robustness findings are source-confirmed but were not fuzzed:

- C++ and C# REZ directory parsing recurse into referenced offsets without detecting cycles. Their range checks do not provide a graph traversal bound. See [C++ parser](../../../../OpenBlood2/OpenLithTech/src/formats/rez_archive_parser.cpp) and [C# parser](../../../../nomad/src/Nomad.Formats/RezArchive.cs).
- C++ ABC geometry subtracts `mainVertCount` from `totalVertCount` using unsigned values without first checking ordering. Combined with zero-returning reads, malformed counts can drive excessive loops. C# explicitly rejects this ordering error and retains replacement vertices.
- C# DAT parsing catches object-section failures and can return recovered/partial objects. Property recovery can advance one byte at a time when a candidate field is invalid. This supports exploration of imperfectly understood files, but complete gameplay loading needs to distinguish authoritative records from guessed or discarded data. See [DatParser.cs](../../../../nomad/src/Nomad.Formats/DatParser.cs).

The appropriate preservation question is whether all gameplay-relevant records were interpreted with correct values, not merely whether a mesh could be displayed.

## Archive Access and Resource Identity

[RezArchive.Open](../../../../nomad/src/Nomad.Formats/RezArchive.cs) uses `File.ReadAllBytes`; `Extract` allocates another array for each extracted resource. The C++ reader likewise retains its file buffer. This makes the current readers convenient to inspect, but memory use grows with archive sizes plus extracted and decoded copies. No peak-memory benchmark was taken.

C# `FindFile` performs a normalized full-path or suffix match through the entry list. [GameArchiveSet.FindFirst](../../../../nomad/src/Nomad.Runtime/GameArchiveSet.cs) scans archives in load order and returns the first match. This is a specific precedence policy, not evidence of compatibility with original patch/mod precedence. Files with the same basename in different directories and archive overlays need explicit identity rules. A resource cache must use the resolved resource identity, including its archive/override provenance, rather than assuming basenames are unique.

These are reuse constraints, not a requirement to replace the parsers. The format logic can be separated from whole-file storage and search policy.

## Coordinate and Unit Contracts

There are at least three distinct spaces to reconcile: original game units and orientations, parsed world/model coordinates, and rendered/physics coordinates. Positions, normals, winding, rotations, dimensions, velocities, animation translations, and socket transforms must agree across the boundary.

The C# [LithTechCoordinate](../../../../nomad/src/Nomad.Core/LithTechCoordinate.cs) helper mirrors X without scaling. [GodotLevelMeshBuilder](../../../../nomad/src/Nomad.Runtime/GodotLevelMeshBuilder.cs) mirrors X and applies `WorldScale = 0.035`, with an inverse conversion. [GodotAbcMeshBuilder](../../../../nomad/src/Nomad.Runtime/GodotAbcMeshBuilder.cs) and [AbcSkeletonModelNode](../../../../nomad/src/Nomad.Runtime/AbcSkeletonModelNode.cs) also handle object rotations, animation flips, bind transforms, scale, and winding.

These transformations are not automatically contradictory: they serve different intermediate spaces. The architectural concern is that their domains are implicit and spread across builders. The passing mirror-X test covers one helper, not the complete world/physics/model conversion pipeline. The C++ [level manager](../../../../NoMadEngine/nomadengine/src/core/level_manager.cpp) similarly applies `WORLD_SCALE` while translating DAT properties into component state.

For preservation, the original gameplay can retain its units while the rendering/physics boundary performs explicit conversions. The audit does not choose a final unit scale. Round-trip transforms, normal/winding orientation, rotated brush collision, viewmodel handedness, and bone/socket alignment remain unverified in a running game.

## Animation Is Part of Gameplay

The original [AI_Mgr.cpp](../../../../sources/blood2/ObjectDLL/AI_Mgr.cpp) handles `MID_MODELSTRINGKEY` and repeatedly queries `MS_PLAYDONE`. [Weapon.cpp](../../../../sources/blood2/ObjectDLL/Weapon.cpp) also branches on model playback state. Animation therefore supplies events and state transitions, not just moving vertices.

The C++ [AnimationController](../../../../OpenBlood2/OpenLithTech/src/core/animation_controller.cpp) provides interpolation, hierarchy traversal, looping, and frame lookup. ABC key strings are retained in parsed data, but the inspected controller does not deliver them as original engine model-string messages. More decisively, the original game's model methods still reach the stubs described in [the contract audit](02_original_code_and_contracts.md).

Two additional timing concerns are visible in the renderer:

- [Renderer3D::UpdateAnimations](../../../../OpenBlood2/OpenLithTech/src/renderer/renderer_3d.cpp), lines 336–341, truncates each frame's seconds to integer milliseconds. There is no fractional remainder in this conversion. Repeated 1/60-second frames become 16 ms each, a potential 4% animation-clock shortfall if driven this way continuously. This is arithmetic from the source, not a measured game slowdown.
- [ResourceManager::UpdateAnimations](../../../../OpenBlood2/OpenLithTech/src/renderer/resource_manager.cpp), lines 1301–1348, skips the controller update for distant or offscreen objects. That can save visual work, but it also freezes this controller's time. Connecting original gameplay events to this culled clock would make behavior depend on visibility. GPU skinning exists alongside a CPU fallback, so it would also be incorrect to describe all animation as CPU-only.

The Godot prototype has two paths. [GodotAbcMeshBuilder](../../../../nomad/src/Nomad.Runtime/GodotAbcMeshBuilder.cs) selects `AbcSkeletonModelNode` for world models, constructs Godot skeletons and animation tracks, and selects a default animation. Its viewmodel path builds meshes separately. The older [AbcAnimatedModelNode](../../../../nomad/src/Nomad.Runtime/AbcAnimatedModelNode.cs) uses the standalone CPU controller, but its presence does not make it the active default world-model path.

The console tests execute [AbcAnimationController](../../../../nomad/src/Nomad.Runtime/AbcAnimationController.cs) and mesh assembly with synthetic data. They do **not** execute Godot's native Skeleton3D/AnimationPlayer path. Socket nodes in the inspected skeleton implementation are positioned from rest transforms and added as child nodes; no animated bone-following mechanism was identified in that class. Original animation events, completion timing, gameplay-selected animations, animated attachments, and viewmodel state transitions still require runtime validation.

## How the Larger Engine Tree Helps

The later LithTech tree is valuable for studying object lifetime, world queries, model transforms, and animation trackers. It is not established as the exact Blood 2 runtime. Its [README](../../../../sources/lithtech/README.md) describes a Jupiter Ex source lineage and explicitly notes differences among game generations. Its [model loader](../../../../sources/lithtech/runtime/model/src/model_load.cpp) reads LTB/D3D model structures, whereas the prototypes focus on Blood 2 ABC files.

This is a concrete format/API mismatch. Algorithms and behavioral patterns can inform implementation, but transplanting an entire later subsystem requires an explicit compatibility analysis. Successful parsing of an ABC model does not validate later LTB loading, and availability of a later engine source tree does not prove native Blood 2 compatibility.
