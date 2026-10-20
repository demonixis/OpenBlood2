# Character node visibility and selected player Load tail

> Continued by [player restore contracts](player_restore_contracts.md): supported
> engine state and original restored-client association now execute in the same
> loaded world. Earlier blanket omissions below are historical. Full client/world
> state and safe NEW_LEVEL transfer remain incomplete.

The original named-node reset now has real engine state consumed by model rendering
and NPC hit localization. `CPlayerObj::RestoreModelState` applies the supported
model/flags/velocity portion of Load to an unassociated savegame owner; it does not
complete a player checkpoint or level transfer. See [source ledger](../implementation/sources/m2_character_nodes_sources.json)
and [validation](../implementation/reports/m2_character_nodes.md).

## Source rules and engine boundary

`AI_Shared::HideNodeAndSubNodes` addresses a standard name from the existing 21-name
table, then `_extra1`, `_extra2`, etc. A missing base still allows its extras; the
first missing numbered name stops traversal. This is **not skeletal descendant
recursion**. ResetNodes applies that sequence to all 21 names. Unrelated names and
extras after a numbering gap retain their state. The port selects these functions
as `hide_character_node/reset_character_nodes` without importing the unrelated
AI_Shared class, limb spawning or FX. Lookup preflight, negative-index checks,
a 257-extra lookup bound and explicit non-missing failures are safety adaptations.
The decoded model limit is 256 nodes. Missing names are normal; an invalid model
or stale owner never becomes successful reset.

The typed original-named Get/SetModelNodeHideStatus services return DE_NOTFOUND
for missing names and DE_INVALIDPARAMS for invalid owners/arguments/masks. A duplicate
name resolves to the first node in model order, as later LithTech `Model::FindNode`
does ([MadScientistAI](madscientist_contracts.md): `madscientist.abc` has two `torso`
nodes); this supersedes the earlier DE_ERROR for ambiguous names. Query failure leaves output unchanged. Names match ASCII
case-insensitively, consistent with existing pose queries. Assignments are idempotent
DE_OK, allowing consecutive extras to be visited even when already in the desired
state. All nodes start visible. An empty per-object byte vector represents that
state; first hiding allocates one byte per node, bounded to 256. It does not use
OpenBlood2's 64-bit mask or truncate later nodes. Inactive live objects are valid;
pending/stale objects are not. The mask belongs to the instance, never its shared
model. A successful different-model assignment clears it along with the animation
track; same-resource assignment preserves both. Failed replacement preserves both.

SDL_GPU geometry omits a triangle when **any of its vertices belongs to a hidden
node**. Vertex node IDs are mapped to node preorder slots, including sparse IDs;
they are not assumed equal to array offsets. This conservative triangle rule is
replacement rendering policy, not verified retail rasterization parity. No parent
inheritance, attachment hiding, physics-body resize or collision holes are implied.
Hidden triangles/nodes remain listed in ModelView; an entirely hidden model has
an inspection entry and no draw. Dynamic hide/unhide does not reload skin textures
or advance animation, and works without ImGui.

Original CDestructable localization skips hidden standard nodes before querying
their poses. A missing hide-status name retains the previous status, as in the
source; a missing later visible pose retains the previous available pose. If no
prior visible pose exists, a missing pose fails instead of using uninitialized
memory. Unavailable/ambiguous queries fail before damage mutation. All-hidden
models still reach the source's normal-damage random fallback; hiding is not
immunity. The existing body ray can still hit the object. Node transforms and
attachments remain queryable regardless of node visibility.

## Player model restoration

RestoreModelState requires live initialized savegame creation, successfully loaded
selected fields, no associated client, no active unsupported powers or damage/fire
callback. It calls original SetCharacter, named ResetNodes, writes the exact Load
flags (VISIBLE/MODELKEYS/YROTATION/ANIMTRANSITION/TOUCH_NOTIFY/SOLID/STAIRSTEP, **no
SHADOW**) and sets velocity to zero. Fresh/live Respawn now also calls ResetNodes
at the original location, while retaining its distinct SHADOW flag and reset rules.
The existing SetCharacter animation-table/sound-root/sphere measurement behavior
is unchanged; missing first hit nodes remain diagnostic rather than fatal there.

This operation does not call Respawn, ValidateAttributes, GoToStartPoint or a fresh
client profile, and grants no HP, armor or ammunition. It does not change raw
attributes, air, acceleration, dimensions, scheduling or an unchanged model's
animation clock. A different model still resets its track through the existing
SetModelFilenames service. Source Load's zoom reset is not claimed because zoom
is unported. Failures clear the completion flag and expose the error. This selected
tail is **not transactional across model setup and subsequent operations**; a future
whole-player restore must discard the failed staged owner. Do not use it as rollback
for an active player. LoadFields invalidates its preceding completion record.

The Skeleton panel shows actual hidden-node counts/status and selected tail success
or failure. Gray diagnostic nodes remain visible in the skeleton projection so
missing/hidden parts can be inspected. Rendering statistics expose removed triangles.
GPU smoke checks hide/reset while the inspector is hidden and after unload/re-entry;
separate SDL_GPU pixel assertions check two visible groups, one hidden, all hidden
and restored. Private restore checks use a separate unassociated player, not a
completed destination session.

## Remaining gate

Engine body/animation/schedule capture, remaining player fields, client saved data
and restored association, and failure-safe named destination entry still block
KeepAliveLoad and NEW_LEVEL. The three existing exit captures remain separate partial
records. General saves, dismemberment/limb spawning, gibs, corpse visibility transfer,
required FX and Gideon remain unavailable. M2 stays incomplete; source-class counts
and normal runtime-instance accounting are unchanged.

The [burning death/dismemberment continuation](death_effect_contracts.md) supersedes
blanket fire/gib/corpse-damage omissions for its selected original paths. Real
flame/smoke particles, burned corpses, runtime limbs and flesh models now run with
inspection. Blood-spurt/splat FX, generic gib/save services and full M2 remain open.
