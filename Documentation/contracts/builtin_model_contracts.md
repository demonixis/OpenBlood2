# Original built-in Model

The registered `Model` class now runs the original `mdl_MessageFn` from
`sources/blood2/AppHeaders/engineobjects.c`. `BuiltinModel` is only its C++ adapter
name, avoiding a collision with the engine's decoded ABC `Model` structure.
This is distinct from `CModelObject` and `CDestructableModel`; neither is imported
or replaced by this class. No new mutable game global or dependency is added.

## Source behavior

PRECREATE unconditionally assigns OT_MODEL before delegating to the original
`bc_EngineMessageFn`. World and string creation read inherited Name, Pos, Rotation,
Flags and optional boolean flags in their existing order, then Filename and Skin.
The registration defaults are empty Filename/Skin and FLAG_VISIBLE. Runtime and
restore creation keep their supplied structure fields except the forced type.
The parent is BaseClass, so there are no B2 difficulty filters, damage aggregate,
initial-update actions, scripted updates or light emission. Aggregate engine and
object messages still receive one dispatch. Static registration supplies the
source CF_ALWAYSLOAD intent without retaining a DLL loader.

The source callback does not select a clip, infer collision dimensions or start a
physics body. Existing engine defaults provide clip zero, looping, identity scale
and zero collision half dimensions. The verified CityHub subset contains 47 visible,
non-solid models with Flags=1. Preserving other authored flag bits does not implement
missing shadow, lighting or physical-body behavior. Do not infer a collider from
visible geometry or claim physical Model compatibility from these objects.

## Resource, rendering and lifetime

The existing resource path loads bounded ABC data after precreate, before initial
update. Immutable ABC objects share a weak cache by resolved archive/range. Each
instance owns its transform and animation track. Missing/malformed ABC files retain
an inspectable instance with `model_error`, no usable animation and no model draw;
this is the existing inspection policy, not a successful resource load or a ready
level. An empty authored Filename requests no asset and remains visible as such.

The existing ModelView resolves exact DTX paths, reports missing/malformed skins,
and uses its explicit magenta fallback. Rendering snapshots do not advance clocks.
Hidden instances remain listed and their active animation continues without UI.
Pending instances stop drawing immediately; deferred destruction releases their
model ownership. World unload also clears caches; old IDs remain stale after reload.
See [model/resource contracts](model_and_collision_contracts.md) and
[presentation contracts](model_view_contracts.md).

Objects identifies built-in Model semantics, current half dimensions, resource
status, original properties and animation. Skeleton and 3D view retain normal
pose/skin/visibility inspection. CityHub's 35 light bulbs and 12 red lamps are actual
textured geometry, not simulated light emitters. No lighting or retail-renderer
parity is claimed.

## Destination evidence and limits

Private CityHub creation changes from 244 to 291 created entries and 206 to 159
unsupported entries; its 541 editor-only entries and 35 failures remain. Structural
preparation has 295 schema-checked, 159 unknown and 31 invalid entries. Rehearsal
still creates/destroys its temporary session and resumes the unchanged source; the
new class owns no global state requiring additional exchange. It never promotes the
destination. SubwayA accounting is unchanged.

Tests cover forced type, runtime/default/property ordering, schema refusal,
aggregate dispatch, missing ABC, malformed ABC/DTX, hidden clocks, removal, shared
resources, actual CityHub properties/poses/textures and two unload/reload cycles.
GPU tests isolate one authored instance of each asset using a diagnostic camera,
read pixels, verify texture success, omit/restore submissions and repeat after reload.
This is asset rendering evidence, not a full CityHub playthrough. Existing destination
isolation and objectives GPU smoke check the increased coverage without accepting
remaining failures. [Implementation evidence](../implementation/reports/m2_builtin_models.md)
and [source ledger](../implementation/sources/m2_builtin_model_sources.json) record verification.

CDestructableModel and the other unknown/failed destination classes, safe automatic
NEW_LEVEL, required services/FX and the actual M2 acceptance gate remain incomplete.
