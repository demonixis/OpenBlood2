# Original AI visibility queries

> Extended by [AI perception contracts](AI_PERCEPTION_CONTRACTS.md): original
> candidate lists and stimulus/target selection now execute. Automatic NPC states
> and complete M2 remain unavailable; earlier acquisition omissions are historical.

M2 remains incomplete. This ports AI_Mgr::CanSee and the two GlobalFilterFn flags
it requests. See [source selections](M2_AI_SIGHT_SOURCES.json),
[execution evidence](../../Documentation/Implementation/M2_AI_SIGHT.md),
[node contracts](CHARACTER_ANIMATION_CONTRACTS.md), [object contracts](OBJECT_CONTRACTS.md)
and [weapon rays](WEAPON_CONTRACTS.md). No production NPC is registered by this
increment. Visibility is a query, not target acquisition or an AI state machine.

## Preserved branch order

The origin is the observer's current animated head node. If that query fails,
CanSee uses its object position. The source used the MoveObj position snapshot;
this port reads the current checked object position because that aggregate's
update scheduling is not integrated. The destination of the first ray is the
target's object position. The observer is excluded and nonsolid objects are
ignored unless FLAG_RAYHIT is set. A hit on the target returns true immediately.

Only a hit on a different object or the primary world triggers a target-head
retry. A complete miss returns false without querying the head. If the target
has a readable head node, a second ray starts at the same observer origin and
ends at that current head position. It must also hit the target to return true.
An unobstructed segment without a target impact is not visibility. Missing target
heads leave the first obstruction in place. Rays are bounded to two per call.

CanSee itself does not check range, facing angle, visibility flags, activity,
player invisibility, hearing, factions or state. Those checks belong to other
original methods and remain unported. Inactive live solid targets remain ray
candidates. The add-on Nightmare always-visible branch is not imported.

## Original object filter

The typed SegmentObjects::SolidOrRayHit admits solid or explicitly ray-hittable
objects. Blood II declares FLAG_RAYHIT for raycasts; the later engine intersection
callback confirms its combination with IGNORE_NONSOLID. This narrow reference is
adopted for CanSee without importing that engine or claiming binary equivalence.
Existing weapon/USE selection modes stay unchanged.

The typed SegmentFilter::AIVisibility selects only the source's IGNORE_LIQUID and
IGNORE_GLASS behavior. Every VolumeBrush descendant is excluded, including
non-liquid ladders and conveyors. A CDestructableBrush descendant is excluded
only when its original GetSurfaceType returns SURFTYPE_GLASS and Destructable is
true. CanDamage and FireThrough do not control this decision. An indestructible
glass brush, a wooden FireThrough brush and a Door still obstruct the query.
The existing GetSurfaceType material/debris fallback is retained.

This is an object filter. Glass material on a primary-world polygon does not
make that polygon transparent. Existing primary-world triangles, transformed
world-model geometry and object AABBs still determine intersections. Skeletal
hit spheres are not collision bodies. Retail ray edge/interior/tie behavior has
not been verified; this is the existing replacement collision engine. Weapon
and USE rays default to SegmentFilter::None and retain their previous behavior.
Unknown filter values fail before collision traversal.

## Lifetime and inspection

Observer and target must be live checked identities. Pending, stale, null or
foreign handles fail before any ray. Zero-length/invalid segments and missing
collision indices return false with separate Invalid or Unavailable inspection
outcomes; a missing service is never represented as clear sight. Node-query
failures retain the original position/no-head fallback. No new string, message,
object link or target ownership is acquired. CanSee does not change m_hTarget,
move either object, advance animation or dispatch model keys.

The bounded last-query record stores checked identities, copied endpoints,
body/head collision results, ray count and outcome. A removed target or hit object
remains historical data and is labelled as removed. The Skeleton panel reads
this record without issuing gameplay queries. It shows origin fallback, head
retry, hit object/world polygon and fraction. The existing diagnostic AI model-key
probe also queries the actual player while the inspector is hidden and after
reload; no authored NPC is replaced or counted as implemented.

Synthetic checks cover direct target hits, behind/distant/inactive targets,
nonsolid misses, head-only visibility, animated-head changes, missing heads,
source material/class filtering, primary-world obstructions, invalid handles,
unavailable collision and repeated removal/unload. Private checks sample six
actual NPC models and two SubwayA cycles. Their controlled probes are explicitly
diagnostic; they do not establish retail enemy behavior.

SubwayA remains 187 live-created entries, four transferred world sounds, 372
editor-only, 17 unsupported and zero failed, plus seven SelfTriggers. CivilianAI,
CultistAI, Gideon, ExitTrigger, target selection, navigation/actions, attachments
and required FX/services keep the M2 exit gate open and --start-game at exit 6.

The [explicit target/facing continuation](AI_TARGET_CONTRACTS.md) adds original
AI_Mgr named target/link commands and deferred target clearing. GetObjectPos now
returns a typed status; target sampling and CanSee reject unresolved poses.
Autonomous acquisition/retention and full AI behavior remain unavailable.
