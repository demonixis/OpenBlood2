# CityHub entry and missing named world models

`fallengrills` is CityHub source object 413, a CDestructableBrush with Visible/Solid
set, Destructable/Pushable/Gravity clear, and Pos (192, 160, -1632). Its original
PostPropRead copies Name to the requested world-model filename. The mounted DAT
contains 100 models and no matching name; `boilergrill` is a different model.
No geometry is synthesized, renamed or borrowed.

## Source basis and limits

Blood II's original `de_codes.h` defines DE_MISSINGWORLDMODEL (23). The available
later LithTech engine has the corresponding LT_MISSINGWORLDMODEL (23):

1. `FindWorldModel` searches case-insensitively; no match returns null.
2. `se_InitWorldModel` returns that error, without dimensions/geometry setup.
3. `sm_AddObjectToWorld` cleans the engine object before initial update.
4. `LoadObjects` frees the original class instance and continues its object loop;
   an absent named BSP is not a failed world-file read.

This is corroborating engine-generation evidence, not a recovered 1998 executable
trace. The later preprocessor only emits a model when it gathers brushes; this
explains how an object row can exist without a model, but the original CityHub
editor source is unavailable, so the specific authoring cause is unknown. See the
[source ledger](../implementation/sources/m2_cityhub_entry_sources.json).

## Typed engine boundary

`Objects::create` still returns null when named geometry is absent. It records
`ObjectError::MissingWorldModel` only with an attached nonempty world, a nonempty
requested name, no case-insensitive match and no other validation error. Both
OT_WORLDMODEL and OT_CONTAINER share this engine rule. Original precreate runs;
initial update never runs. The rejected instance becomes pending and is destroyed
at the normal safe boundary. Runtime/string/restore callers still get failure.

Only the world-file loader classifies this typed failure as
`EntryState::MissingWorldModel`, displayed as **Omitted: missing world model**.
The source row and diagnostic remain; object and sound identities are empty.
This is neither a created object, difficulty filtering, nor a successful service.
No class/name/world exception or general 'ignore errors' policy is introduced.

Ambiguous names, invalid/unsupported geometry, absent world context, invalid
properties, rejected precreate, missing models/images/audio, unknown classes and
unavailable required services retain their existing failures. The original later
engine takes the first ambiguous match. This project first kept a stricter
rejection; the later [duplicate-name rule](object_contracts.md#duplicate-bsp-names) supersedes it and binds that first match. Binary/parser corruption still rejects preparation.

## Animated world materials revealed by destination preflight

After resolving the omitted object, strict presentation preflight exposed
`sprites/motelhell.spr` being passed to the DTX decoder. World-material lookup now
recognizes normalized `.spr` paths, uses the existing bounded SPR reader, decodes
all referenced DTX images and deduplicates them with ordinary world textures.
Manifest entries retain frame indices/fps and dimensions, without duplicating
pixel buffers. No manifest is treated as an image or uploaded as a fake texture.

All frames must have equal dimensions so the existing authored UV/Pan mapping
remains valid. Missing, malformed, oversized and incompatible frames retain explicit
resource errors and block NEW_LEVEL. Existing 4,096-material/128 MiB pixel limits
include frame images; frame references additionally stop at 65,536 per mesh. The
SPR reader's own 1 MiB/4,096-frame/1..1,000-fps bounds remain unchanged.

Rendering resolves the frame before the pass and binds its already uploaded DTX.
Opaque and translucent world passes use the same read-only world-time phase, with
integer `1000/fps` frame periods, wrapping and world-clock reset. Repeated draws at
the same time cannot advance animation. Visibility/ImGui do not own this clock.
The later renderer establishes SPR world-material support and tracker selection;
this common simulation-time phase is an explicit replacement timing policy, not
retail per-section rendering-clock parity. Manifest alpha/key flags do not override
the existing authored world-surface alpha rules. No shader artifacts changed.

The material inspector shows manifest path, fps, dimensions, frame count and current
DTX path, alongside each decoded/failed frame resource. Presentation preflight counts
decoded DTX images separately from manifests.

## Transition and inspection

The existing [NEW_LEVEL transaction](level_change_contracts.md) admits the explicit
omission while retaining all other preparation and rollback checks. Reports and
rehearsals count omissions separately and retain every source row/detail.
Session's world table and Session/Objectives transition reports expose the outcome;
drawing never triggers creation or transfer.

CityHub accounts for all 1,026 source entries: 453 created, 31 transferred world
sounds, 541 editor-only, one omitted named world model, zero unknown and zero
failed. These are initial source dispositions, not later runtime-object totals.
Supported KeepAlive player state enters at `01_egressa`, with a new checked client
association and the existing original start-trigger behavior. Resource paths,
content bytes and dependency pins are unchanged.

Initialization success does not certify later gameplay, every future resource
request, full campaign progression, autosave or M2 completion. See
[implementation evidence](../implementation/reports/m2_cityhub_entry.md).
