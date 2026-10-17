# Original Cultist/Civilian initialization and AI_Update

> The [animation-clearance correction](ai_animation_clearance_contracts.md) supersedes
> earlier blocked-resize failures/CrawlUnder fallback claims: original SetAnimation
> continues a valid clip while physical bounds stay unchanged on collision refusal.
> The AI caches retained physical dimensions; the request remains inspectable.
> Invalid/unavailable services still fail; the replacement resize solver is unchanged.

> Extended by [activation visibility contracts](activation_visibility_contracts.md): replacement player/camera pings now reuse the original glass/volume object filter, with atomic refresh, checked inspection and authored Gideon intro acceptance. This is not retail PVS; M2 remains incomplete.


> Extended by [Gideon lifecycle contracts](gideon_runtime_contracts.md): production initialization, AI_Update, health delivery and fifth-table isolation are now integrated. Earlier blanket boss-registration omissions are historical. M2 remains incomplete.

> Continued by [ordinary corpse contracts](corpse_contracts.md): original Dying/CCorpse now
> transfer ordinary deaths. Earlier blanket corpse omissions are historical;
> fire/gibs, corpse damage, required FX and complete M2 remain unavailable.

> Extended by [NPC hit/recoil contracts](ai_hit_contracts.md): original visible-node
> localization, region damage and nonlethal reactions now execute for initialized
> Cultist/Civilian NPCs. Earlier blanket localization/recoil omissions are historical;
> lethal death/corpse/drop services remain unavailable and M2 is incomplete.

CultistAI and CivilianAI are now registered for world creation. Their selected
original constructors, properties, model selection, initialization and AI_Update
execute through typed services. See [source selections](../implementation/sources/m2_ai_runtime_sources.json)
and [execution evidence](../../../Documentation/Implementation/m2_ai_runtime.md).
This supersedes earlier statements that no production NPC is registered. M2 is
still incomplete: reachable unimplemented actions fail visibly, and this is not
complete combat or campaign acceptance.

## Construction and initialization

Production factories apply the original class defaults before precreate. Diagnostic
subclasses retain their isolated fixtures. AI properties are read for world/string
creation; the subclass Male/LabTech/Scared/skin booleans only for world creation.
The original base precreate chain reads placement/name and damage properties before
subclass model selection. Source random skin draws, female-civilian skin-1 selection,
Special8 skin-4 precedence, sorority/clown paths and class/gender sound roots are
preserved. Add-on robe Cultists reject explicitly. Restoration is unavailable.

Animation/hit-sphere tables are shared by class/gender. The source civilian
precedence quirk rebuilds the male table for every male, and a female LabTech also
initializes the independent female table. Shared tables survive individual actor
removal, but are reset after world destruction to avoid cross-world clip/voice
ownership. A rejected second session cannot reset the bound session's tables.
A read-only candidate preflight prevents failed creation from poisoning
an existing table; no voice ownership is copied. Missing head/model data fails.

Initialization connects inventory, damage and movement aggregates; strength 5,
unique supported weapon acquisition, source ammo order (Charges and Cells both
feed batteries), focus 200, difficulty health/armor/damage factors and Cultist's
0.5 damage factor are retained. NONE, BERETTA and MELEE are supported; other authored
weapons and nonzero proximity inventory reject instead of silently substituting.
Weapon delivery applies the multiplier without changing recorded sampled damage.

Default RandomHitPoints=0 produces the source reversed Random(1,0) request. The
adapter uses a unit draw and the linear interval formula, also seen in the other
LithTech generation's ic_Random. This is source evidence, not retail random-sequence
verification. RandomHP=1 has an exact +1 result. Scale varies as (1-f,1+f,1-f),
f in [-0.05,0.1]. Guard captures the spawn position before ground placement. Model
animation-zero centered bounds precede the existing atomic foot-anchored resize.
World creation moves to the ground; runtime/string creation does not.

Blocking priority is 50; damage mass is 250; typed character-body mass/friction are
250/10 (source SetMass formula). The existing bounded replacement integrator applies
ground drag and gravity once; MIN_FORCE=0 drops no nonzero forces. This does not
claim retail physics parity or full NPC damage impulse behavior. Source flags are
retained, including REMOVEIFOUTSIDE: a center outside the loaded DAT world bounds
is removed deferred on the next object advance, including inactive objects.
The inventory aggregate updates supported held models after the AI callback.

## Update order and activation

AI_Update preserves: removal guard; >=1-second sleep-gap random desynchronization;
last-update/0.001 scheduling; cinematic movement restriction excluding Script,
Passive and Idle; first-update 3-second automatic shutdown; target retention and
current-position sampling; tracked-position refresh; health<=0 Dying selection;
character support DONTFOLLOWSTANDING; Metacmd<=0 repair; original state dispatch.
The existing CMovement aggregate refreshes after the callback, preserving cached
basis timing. Physics/model clocks run once outside AI_Update. Script messages now
prepare the owned program and force the original immediate update.

SetDeactivationTime and AUTODEACTIVATE_NOW preserve pending deadlines. Player
Update pings strictly after its 0.5-second deadline, using the active camera when
present. PingObjects snapshots eligible active/auto-deactivated identities, checks
actual center rays and only then refreshes visibility timers. Manual inactive
actors remain inactive; missing collision/invalid rays fail without partial pings.
This visibility selection is replacement code: retail PVS data is not decoded,
so occlusion/corner activation can differ. It never depends on the inspector or
render visibility. Expiry and wake delays use simulation time, not wall time.

A live target sets HasTargeted and expands positive sensing range to seeing range.
Dead targets clear and request Idle (which can become SearchVisual under the
original idle deadline); occluded targets clear unless ALWAYSCLEAR. Wrong-class
and unresolved required poses fail. Pending targets clear through checked identity
and link handling. Nonlinked tracked-object removal clears the stale identity and
suspends the actor with its historical coordinates available for diagnosis.

Civilian exact uppercase TRIGGER preserves Hide/Script/Special guards, selects
LabTech Hide or other Special1, and then forwards the original base message.
Checked non-owning linking replaces the raw target assignment. Damage forwards
once before selecting EscapeRun. Every HSTRING read owns exactly one release.

## Failure and inspection

Initialization rejection rolls back the actor and its inventory/attachment children.
A required runtime failure latches its reason, stops scheduling/velocity and makes
the still-owned actor manually inactive. This deliberately retains evidence instead
of deleting an actor as the unchecked original error path could do. No missing
state succeeds as a no-op. Nested update and callback removal are guarded.

The Skeletons panel shows initialization/failure, state/phase, update count,
automatic activity/deadline/pings, wake delays, camera pauses, target loss and HP/
damage factor. Existing model, inventory, attachment, query, script, sound and
failure panels remain available for hidden/inactive actors. The UI never advances
AI. The attachment list starts collapsed so a populated world does not push the
lifecycle panel out of view. Model-key/audio diagnostics remain separate from lifecycle success: availability
of these registered classes does not assert every authored key/effect is supported.

Without a player, the original enemy-list fallback can search the actor's own
faction and sensing can select self. The existing distinct-object target/link
contract rejects that selection. Stepping SubwayA before player entry currently
suspends three NPCs on the first tick; the nested target failure remains visible.
Entering a player later does not silently revive a failed actor. The successful
SubwayA run enters the player before the first simulation tick. This is an explicit
remaining integration boundary, not a reason to weaken the identity contract.

## Remaining boundary

Gideon and ExitTrigger still do not instantiate in SubwayA. NPC recoil/death,
shade/smell/assist/pickup actions, some model keys, broader weapons, effects and
session services remain open. A Dying or another missing action suspends with an
explicit error; there is no fabricated death sequence. Skin selection is not a
promise that every add-on texture decodes. Engine replacement activation, resize,
friction and outside-bounds handling require retail comparison. Linux is the only
verified platform; other platforms/XR and full M2 acceptance remain pending.

The [destination initialization rehearsal](destination_trial_contracts.md) extends the structural scan with temporary original callbacks and copied-player named entry.
It isolates the current original globals, destroys the temporary session and resumes the unchanged source gameplay state.
Ordinary second-session binding remains forbidden; no automatic transfer or completed M2 acceptance is claimed.

The [interactive entry correction](session_playback_contracts.md) now holds application simulation until player entry, preventing the reproduced pre-player NPC suspension. Raw diagnostic stepping retains that boundary; broader combat and M2 remain incomplete.

The [Naga weapon continuation](naga_weapon_contracts.md) extends the supported NPC weapon subset with original eye-beam damage, head_gun aiming and red-small beam presentation. Gideon and the eye impact composite remain unavailable; earlier blanket weapon omissions are superseded only for this subset.

The [wind/shockwave continuation](wind_weapon_contracts.md) extends the runtime-only projectile subset with OT_NORMAL scale metadata and the original type20 model/light recipe. Earlier type/resource restrictions are superseded only for this bounded subset; production Gideon and M2 remain incomplete.

The [Gideon combat continuation](gideon_combat_contracts.md) supersedes the selected-method omissions for boss decisions, shield armor, node firing and extra-key powerup. Production boss lifecycle remains unavailable. Sparse supported NPC weapon IDs now use value-based duplicate suppression.
