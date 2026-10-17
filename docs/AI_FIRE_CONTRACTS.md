# Original AI weapon choice and standing fire

> The [animation-clearance correction](AI_ANIMATION_CLEARANCE_CONTRACTS.md) supersedes
> earlier blocked-resize failures/CrawlUnder fallback claims: original SetAnimation
> continues a valid clip while physical bounds stay unchanged on collision refusal.
> The AI caches retained physical dimensions; the request remains inspectable.
> Invalid/unavailable services still fail; the replacement resize solver is unchanged.

The [Flare Gun continuation](FLARE_CONTRACTS.md) adds original ID 3, primary sticking/burn ticks, alternate fragments, pickup/drop and sprite tint/alpha. Earlier blanket omissions are historical for this subset; remaining effects, death services and M2 stay open.

The [Mac-10 continuation](MAC10_CONTRACTS.md) adds original weapon ID 2, its pickup/drop, NPC use and both-hand view animation fallback. Earlier Beretta-only statements below are historical for this supported subset.

> [Destructible model contracts](DESTRUCTABLE_MODEL_CONTRACTS.md) now implement
> the CDestructableModel-only IGNORE_DESTRUCTABLE branch in CheckClearShot.
> Earlier absence statements below are historical; CanSee keeps its own filter.

This selective source port connects `FindBestWeapon`, type-based `ChangeWeapon`,
`MC_BestWeapon`, the Beretta/knife default `Fire` aiming branch, `CheckClearShot`,
`MC_Fire_Stand` and AI `fire_key` dispatch. [Source ranges](M2_AI_FIRE_SOURCES.json)
and [execution evidence](../../Documentation/Implementation/M2_AI_FIRE.md) record
its boundary. Read [weapons](WEAPON_CONTRACTS.md), [state actions](AI_STATE_CONTRACTS.md),
[visibility](AI_SIGHT_CONTRACTS.md) and [object lifetime](OBJECT_CONTRACTS.md).

These are reusable original methods, not a complete NPC. No production NPC is
registered; authored CivilianAI/CultistAI/Gideon entries remain unsupported.
Full initialization/AI_Update, attack-state sequencing, navigation, dying/recoil,
scripts, other weapons, required client FX and ExitTrigger still block M2.

## Weapon choice

FindBestWeapon examines right-hand slots in source order, starting with the current
weapon. Candidates require strictly `distance < range`, `radius < distance`,
nonzero raw ammo and either greater integer maximum damage or an empty current
ammo pool. Ties retain the current candidate. No suitable alternative returns
null without discarding an unsuitable current weapon. Invalid owner/distance
returns false separately from a successful no-change result.

The source reads the raw AMMO_NONE pool, which is zero: it does not select the
knife through this heuristic, although the knife can fire without ammunition.
MC_BestWeapon may consequently choose FindAmmo with a current knife. This quirk
is preserved. Choice uses the current checked owner position and cached target
position instead of MoveObj's old position cache.

MC_BestWeapon preserves the two animation phases and the completion tick that
clears `m_bAnimating` without advancing Metacmd. Idle/Passive Cabal branches now
call it. SWITCH_WEAPON has three columns; TYPE_MELEE is four. The source's invalid
index is rejected explicitly, not mapped to an invented clip. Destination clips
are checked/selected before committing the inventory change. ChangeWeapon reuses
the supported slot/owned-hand lifetime; legacy view Holster/muzzle-FX setup are
not implemented. No null current weapon is dereferenced.

## Aiming, clearance and delivery

The supported default Fire branch samples current `r_gun` position and aims at
the cached target position. If that node is unavailable it uses body position
and current forward, not the target direction. Selected directions are normalized
and checked. Missing weapons, pending/stale endpoints, degenerate vectors and
required service failures remain observable errors.

CheckClearShot uses normal/alternate weapon range and the existing typed
SolidOrRayHit/AIVisibility ray path. All VolumeBrush subclasses and destructible
glass brushes are ignored for clearance. Original IGNORE_DESTRUCTABLE refers to
CDestructableModel, which remains unported and unregistered; it is not a blanket
filter for all destructible objects. Actual weapon rays retain their own existing
filter, so a clear aim through glass may still hit/damage that glass.

An initial miss is clear, unlike CanSee. A target hit is clear. An obstruction
allows a current target-head retry; that retry must hit the target (a miss fails).
The source updates the cached target position to the sampled head even when the
retry fails. Only a successful retry changes the firing direction. Missing head
poses do not manufacture a second ray. Queries advance neither animation nor
physics. Collision remains the existing bounded primary/world-model/AABB service,
not retail hit-limb physics.

The typed inventory shot uses a session RNG byte to seed the existing portable
weapon generator. This adapts the original server RNG to local owned transport;
retail CRT bit parity is not claimed. Player eye alignment is now correctly
restricted to CPlayerObj, as in original FireSpread; NPCs use the actual gun ray.
A pending weapon owner cannot fire. Damage/ammo/owned-hand cleanup and deferred
callbacks use the existing weapon/object services.

Original AI Fire discards the inventory return: a dry attempt still returns true.
Inspection separates attempts, fired shots, dry attempts, blocked shots and service
failures. Unlike that unchecked source return, an invalid/unavailable shot service
fails explicitly. Damage delivery is not by itself proof of a health change.

## Commands and model keys

MC_Fire_Stand selects the original fire-type clip, disables looping, stores primary
reload time and resets load/time starts to zero. Its first update is 0.1 seconds.
An actual animation-resize block selects CrawlUnder; missing clip/model/collision
services remain failures. Resize is the already documented atomic foot-anchored
replacement, not reconstructed retail SetObjectDims2.

While playing, time must be strictly greater than last-load + reload before Fire.
A source-accepted attempt (including dry) updates last-load time. A blocked shot
or no weapon clears animation and advances Metacmd; completed playback independently
advances it again. Both increments can happen in the same tick. Invalid clocks,
overflow and missing playback reject instead of wrapping or silently waiting.
No extra cooldown or physics integration is introduced.

The model-key dispatcher retains fire-first priority, no-target early return,
pending-target failure, and base motion forwarding after an attempted shot. A
fire_key and timed MC_Fire_Stand call can both fire: the source does not deduplicate
them. Cultist pistol clips have no fire_key and use the timed path; the catalog
reports this rather than pretending those shots came from markers. show_weapon's
uninitialized source parameter and MC_Extra remain explicit unsupported branches.

The inspector reads live state and errors, including hidden/inactive/missing
models, without advancing actions. Diagnostic drivers in tests and the GPU smoke
call this command explicitly; they are not AI_Update. Removal in a damage callback
is deferred through the animation batch; no later marker uses the pending actor.

The [autonomous locomotion continuation](AI_LOCOMOTION_CONTRACTS.md) now supplies
walk/run/moving fire, obstruction/ledge probes and Cultist close/far attack sequencing.
Earlier blanket omissions are superseded only for this subset. Full initialization,
AI_Update/deactivation, remaining states and shade recovery stay unavailable.

The [Naga weapon continuation](NAGA_WEAPON_CONTRACTS.md) extends the supported NPC weapon subset with original eye-beam damage, head_gun aiming and red-small beam presentation. Gideon and the eye impact composite remain unavailable; earlier blanket weapon omissions are superseded only for this subset.

The [shield continuation](SHIELD_WEAPON_CONTRACTS.md) also permits original GIDEON_SHIELD selection and generic r_gun/body aiming through FIRE_KEY. This supplies the visual-only weapon, not Gideon's own fire-node override or boss state loop.

The [Gideon combat continuation](GIDEON_COMBAT_CONTRACTS.md) supersedes the selected-method omissions for boss decisions, shield armor, node firing and extra-key powerup. Production boss lifecycle remains unavailable. Sparse supported NPC weapon IDs now use value-based duplicate suppression.
