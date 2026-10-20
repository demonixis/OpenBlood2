# M2 — CityHub entry and the absent fallengrills world model

The CityHub transition now treats the absent `fallengrills` BSP as an explicit
engine-rejected object, following the available LithTech loader behavior. It does
not create an empty brush or replace its geometry. NEW_LEVEL can proceed through
normal destination initialization, original player KeepAlive entry and checked
client reassociation. **M2 remains incomplete.**

## Investigation

The mounted CityHub DAT comes from BLOOD2.REZ with zero patch overrides, at byte
144209905 (3,899,649 bytes). It has 1,026 object rows and 100 world models. Source row 413
is CDestructableBrush `fallengrills`, at (192, 160, -1632), with Visible/Solid enabled
and Destructable/Pushable/Gravity disabled. Original
[PostPropRead](../../../../sources/blood2/ObjectDLL/DestructableBrush.cpp) copies the
object name to its world-model filename. No model matches `fallengrills`;
`boilergrill` is unrelated. A case-insensitive scan of the raw DAT finds exactly
one `fallengrills` occurrence, consistent with the object Name and no BSP binding. Changing the filename, collision flags or commercial
DAT would invent behavior.

Blood II's [SDK codes](../../../../sources/blood2/AppHeaders/de_codes.h) include
DE_MISSINGWORLDMODEL (23). The available later engine has the same error number and
an explicit control flow:

| Reference | Observed behavior |
| --- | --- |
| [world_shared_bsp.cpp](../../../../sources/lithtech/runtime/world/src/world_shared_bsp.cpp) | Case-insensitive name lookup returns null when no model exists. |
| [server_extradata.cpp](../../../../sources/lithtech/runtime/server/src/server_extradata.cpp) | World models and containers return LT_MISSINGWORLDMODEL before geometry/dimensions setup. |
| [servermgr.cpp](../../../../sources/lithtech/runtime/server/src/servermgr.cpp) | Extra-data failure destroys the engine object before its initial callback. |
| [s_object.cpp](../../../../sources/lithtech/runtime/server/src/s_object.cpp) | World loading frees the rejected original class and continues to the next source object. |

The later [preprocessor](../../../../sources/lithtech/tools/PreProcessor/FindWorldModel.cpp)
only emits a world model when brushes are gathered. This makes an orphan object
row plausible, but does not establish why CityHub's authoring left this particular
row. These references belong to a later engine generation; this is an informed
compatibility adaptation, not a captured execution trace of the 1998 binary.

## Implementation

The existing typed object creator distinguishes missing named geometry from
ambiguous or invalid geometry. It still returns null, records the diagnostic,
skips initial update and defers destruction to the normal safe boundary. Runtime
creation callers receive a real failure.

World-file loading retains the source row with **Omitted: missing world model**,
no object/sound identity and a concrete reason. Both destination rehearsal and
NEW_LEVEL inspection count this separately. There is no `fallengrills`, CityHub
or CDestructableBrush special case. Missing context, empty names, invalid properties,
unknown classes, ambiguous bindings, unsupported shapes, malformed DAT and other
missing resources retain their existing failure behavior. The project intentionally
keeps stricter ambiguity rejection than the later engine's first-match lookup.

CityHub now accounts for **453 created / 31 transferred sounds / 541 editor-only /
one omitted / zero unsupported / zero failed** source entries. Omission is not
counted as a created object or successful engine service. The existing safe
transaction still validates resources, preserves supported player state, selects
`01_egressa`, transfers audio output and changes world revision at the frame boundary.

Read the [contracts](../../contracts/cityhub_entry_contracts.md) and
[source hashes](../sources/m2_cityhub_entry_sources.json). Commercial assets,
reference repositories and dependency revisions remain external and unchanged.

## Animated CityHub world textures

The first corrected transition passed object initialization, then correctly refused
presentation: `sprites/motelhell.spr` had been handed to the DTX decoder. The old
SubwayA session survived that refusal. This exposed a separate required material
service rather than a reason to bypass destination validation.

World materials now use the existing SPR parser and decode every referenced DTX.
A shared frame-image cache and read-only world-time selection animate the surface
through the existing SDL_GPU opaque/alpha passes. All frame pixels upload with the
world revision; drawing does not reload them or advance game clocks. Inspection
shows fps, frame count and the current frame resource. The actual Motel Hell
manifest has 11 frames at 15 fps, each 128 by 64 pixels. Missing/bad frame data and
inconsistent dimensions still reject preparation. No shaders or dependency pins
changed. The later LithTech renderer supports SPR textures; the common simulation
clock is a documented replacement for its per-section render-time tracker.

## Validation

- Object/lifetime probes reject absent world models and containers before initial
  update, reclaim pending instances, and keep ambiguous names and invalid shapes
  as failures. Synthetic NEW_LEVEL admits a generically named omitted brush while
  still rejecting unknown classes, bad resources, invalid geometry and recursive
  entry exits.
- The private transition test performs three CityHub/SubwayA round trips, preserves
  37 HP, 23 armor and 1,234 bullets, rebinds clients, advances revisions and releases
  all resources. Malformed Bridge requests preserve source identities, intrusive
  lists, voice cursors and clocks. CityHub selects `01_egressa`; SubwayA's only
  start is `noname`, so the original fallback is retained on return.
- A separate read-only probe runs 600 post-transition updates (ten simulated
  seconds): 100 HP remains, no generic unavailable service is called, and teardown
  releases clients/strings. This is a stationary entry probe, not a full playthrough.
- World-material tests exercise exact-path/case deduplication, every frame's DTX
  validation, missing/malformed/unequal frames, invalid time/recursive indices and
  the 65,536-reference aggregate bound. GPU pixels cover opaque/alpha animation,
  fixed-time redraw, wrap/reset and unload/reload with no per-frame texture upload.
- GPU transition tests render both destinations, reject an independent missing
  destination, preserve player stats and transfer real SDL audio output. The
  application objectives smoke repeats the authored delayed SubwayA ExitTrigger
  and original relay while the inspector is hidden, then unloads and explicitly
  reloads SubwayA to repeat it. Source paths are checked before the second exit;
  raw handles are never compared across independent session namespaces.

Debug and inspector-free Release builds pass. Tests ran against the external
installation; graphics used the NVIDIA Vulkan ICD and logged the **GeForce RTX
3060 Laptop GPU** at runtime.

| Validation | Passed | Private-data tests included |
| --- | ---: | ---: |
| Debug headless suite | 783 / 783 | 128 |
| Release headless suite | 783 / 783 | 128 |
| Debug NVIDIA suite | 23 / 23 | 14 |
| Release NVIDIA suite | 23 / 23 | 14 |
| Clang ASan/UBSan lifetime, world, texture and player checks | 47 / 47 | 7 |
| Focused transition/object checks | 17 / 17 | 2 |

The graphics suites include real mouse combat/corpse clearance, world/model depth,
alpha, sky, sprites, surface animation, flashlight, proximity/remote bombs and the
application's hidden-inspector objectives flow. CityHub's station-arrival capture
was also visually inspected. A final strengthened synthetic refusal assertion was
rebuilt and checked in Debug, Release and the sanitizer configuration. No production
behavior changed after the full suites.

Reproduction commands, test totals, source/content hashes, the exact implementation
patch, private probe source and logs are retained in the
[evidence ledger](evidence/m2-cityhub-entry.json). The
[initial SPR refusal](evidence/m2-cityhub-entry-initial-spr-refusal.log) and initial
GPU fixture failures remain recorded alongside successful final results. Formatting
and local documentation links pass; all 10,923 reference files match the inventory
and the six reference repositories remain clean.

Implementation commit: `b7313c194fb1d3ba2406dd19b1c0fa11bfce8ccb` in the independent Blood2 repository.

## Remaining scope

This removes the missing-name blocker for entering CityHub; it does not establish
complete CityHub traversal or campaign correctness. Later AI/action services, full
lighting/FX fidelity, autosave/whole-world restoration and other M2 acceptance
requirements remain separate. The shader artifacts were not changed; Linux NVIDIA
validation does not substitute for native macOS testing.
