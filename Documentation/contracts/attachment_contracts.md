# Held models and attachments

> Extended by [NPC weapon-drop contracts](weapon_drop_contracts.md): the selected
> Cabal drop, independent falling hand and Beretta collection now work. Corpse/gib
> and remaining death services still fail explicitly; M2 is incomplete.

Later [production NPC lifecycle contracts](ai_runtime_contracts.md) supersede
this snapshot's absence of Cultist/Civilian initialization, AI_Update and activation.
The existing action/service limitations remain unless explicitly superseded.

This M2 increment connects the original CWeapon ShowHandModel/CreateHandModel,
CInventoryMgr current-hand branch and held CHandWeaponModel lifecycle. It adds
real attachment transforms through typed SDK methods. See the
[source ledger](../implementation/sources/m2_attachment_sources.json) and
[execution evidence](../implementation/reports/m2_attachments.md).
M2 remains incomplete; dropping weapons, client hand-visibility SFX and production
NPCs are not supplied by this increment.

## Original rules and adaptations

The current living player's inventory creates both occupied hand slots. Selection
removes both old hand models; death hides/removes the current models. Each weapon
owns its child and removes it in its destructor. The engine relation owns neither
endpoint. Opaque checked object handles replace the original borrowed CWeapon /
CHandWeaponModel pointers; independently removing a child or detaching its relation is safe and the next
inventory update recreates it. No callbacks dereference a destroyed weapon.

CreateHandModel uses the unchanged WeaponData model/skin paths, visible and
force-client-update flags, the optional `handheld` animation, zero position offset
and identity rotation offset. Right and left use `r_gun` and `l_gun`. A missing gun
node attaches at the owner's origin and requests hidden visibility, as the source
branch specifies. It does not silently select another node. Missing model data
fails and removes the partially created child. Client weapon synchronization
preflights both hand ABC and indexed skin before entry/selection succeeds. Skins
are still indexed only, not decoded or rendered.

The original CHandWeaponModel dimension of 0.1 and first-update visibility branch
are retained. Its accidental MID_PRECREATE fallthrough into Update is excluded:
precreate has no committed engine state in this boundary. The first scheduled
update applies the visibility request and then disables updates for the held
model. Client IDs/SFX, dropped-body physics, powerup spawning and save/restore
are not imported. A readable diagnostic model is not a working first-person
visibility effect. No extra hand-model animation keys or gameplay rules are added.

The selected inventory branch runs once after the local player's movement/client
weapon update, outside callbacks; it uses the current selected slot and propagates
failure. The broader original inventory Update and server view-model objects are
still selective ports. Changing a slot removes old held models even when later
client preflight fails; the selected slot rolls back and recreates its old hand
models on the next update. Existing client model rollback remains atomic.

## Engine relation contract

CreateAttachment/RemoveAttachment/FindAttachment/GetAttachedModelNodeTransform
use compiler-checked declarations. HATTACHMENT is a monotonically allocated
64-bit integer, zero invalid, with 65,536 allocations per Objects lifetime. IDs
are never reused on unload. Creation validates all inputs before changing either
child transform or relation output. Failed creation clears the output ID.

This supports one parent and one level of **visual** children: normal, model or
sprite. Duplicate parenting, chains/cycles, self-parenting, container/world-model
children, client-controlled children, physical flags and nonzero velocity or
acceleration reject. Independent movement/teleport/rotation/velocity/keyframe
updates and adding physical flags to an attached child reject. No joint solver,
attachment collision or parent scale inheritance for child size is claimed.

The relation stores copied node names and local offsets. It reuses the existing
[current node sampler](character_animation_contracts.md), including parent scale
for the node's location. Position is base-position plus base-rotation times offset;
rotation is base-rotation times offset-rotation. Offsets and child scale are not
multiplied by parent scale. The later LithTech transform implementation was read
as supporting reference, not imported or claimed generation-identical.

Transforms refresh after session simulation, before attached object/node
position queries and before segment intersection. All-object rays use the actual
child bounds; solid-only rays exclude these nonsolid children. They do not advance clocks, emit keys, integrate physics or
rely on visibility/activity or ImGui. Attached children skip generic physics.
When model replacement removes a required node, the relation stays inspectable
as unresolved, retains the last valid child transform and fails the session
refresh. There is no success-shaped fallback to the origin for an existing named
attachment. Restoring a readable node allows resolution again.

Removing either endpoint immediately invalidates its relations, before deferred
callbacks/destruction. This deliberately makes the original undefined child
removal and stale attachment cases safe. It does not remove the other endpoint.
Explicit detach retains the last resolved child pose. Invalid/stale IDs return
errors and cannot alias a new relation after reload. Original owner destructors
remain responsible for their children.

## Inspection and limits

The Skeleton panel lists every live relation, parent/child identity, chosen node
or explicit origin fallback, resolution, actual child position, hidden/inactive
state and model/skin indexing. Its normal model list includes held models and
missing models. The existing skeleton GPU scenario checks the real Caleb Beretta
attachment while the UI is hidden, then fresh attachment identity after reload.
Queries and inspection have no model-clock or lifetime side effects.

SubwayA's source accounting remains 187 created, four transferred sounds, 372
editor-only, 17 unsupported and zero failed, plus seven SelfTriggers. A default
living player adds one runtime Beretta child on its first inventory update;
dual Berettas add two. These are owned children, not newly supported source entries.
AI states/classes, ExitTrigger, required client FX and the remaining M2 services
keep the playable-session gate open. Linux software/hardware graphics checks do
not establish retail attachment timing, other-platform or XR parity.

The [animated model continuation](model_view_contracts.md) supersedes earlier model/skin rendering omissions for existing ABC/held/view models. It also corrects the absent-TransformInfo quaternion default at the reader boundary, shared by all pose/node/attachment queries. Explicit zero remains direct and nonzero flags remain unsupported. Close-view projection is replacement presentation; required gameplay/FX and M2 remain open.
