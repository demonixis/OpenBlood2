# Original energy projectile boundary

Read [object lifetime](OBJECT_CONTRACTS.md), [weapon/audio](WEAPON_AUDIO_CONTRACTS.md),
[collision](MODEL_AND_COLLISION_CONTRACTS.md), [sprite presentation](SPRITE_VIEW_CONTRACTS.md),
[source ledger](M2_ENERGY_PROJECTILE_SOURCES.json) and
[validation](../../Documentation/Implementation/M2_ENERGY_PROJECTILES.md).
M2 is incomplete; Gideon is not registered.

## Weapon and original callbacks

The complete base-game Energy Blast definition is retained: ID 27, AMMO_NONE,
25–50 damage, 1000 units/s, radius 175, one projectile, 0.5-second reload in both
modes, TYPE_MELEE, and null held models. Player acquisition remains refused;
there is no supported player view/profile for this NPC weapon. Original
CEnergyBlast changes the primary WAV to Gideon's distfire when the owner's class
ancestry is Gideon; alternate fire keeps Zealot's WAV. This ancestry check does
not register missing NPC classes. Existing four-voice lifetime and hearing apply.

CWeapon dispatches a projectile when the selected definition names one. Unknown
projectile families fail explicitly, never silently use vector damage. Existing
collision preflight and post-hearing revalidation still precede dispatch; the
long preflight ray is diagnostic, not a projectile impact. After checked runtime
creation, Setup sets direction/damage/speed/radius, links the shooter, aligns the
rotation and writes velocity/flags. CEnergyBlast clears gravity. Missing SPR/frame
resources or failed Setup report unavailable, without a successful child handle.
Shots already accepted by the base weapon remain counted, as do sound attempts.

AI ENERGY_BLAST selects this weapon; the generic original firing branch uses
r_hand_extra2 and bypasses CheckClearShot. A missing node fails rather than using
uninitialized output. Gideon's distinct rr_gun/extra override is still absent.
Raw no-ammo best-weapon selection and invalid TYPE_MELEE switch-table columns
remain as documented in [AI firing](AI_FIRE_CONTRACTS.md).

CProjectile retains its BaseClass/aggregate chain and CDestructable aggregate.
INITIALUPDATE runs before weapon Setup, creates HP5 with no damage impulses,
sets dimensions (1,1,1), initial scale 1, SAVEABLE/SINGULARITY_ATTRACT flags and a
0.001-second schedule. This does not implement save/restore. Only the concrete CEnergyBlastProjectile runtime class is registered (under
BaseClass); generic CProjectile class lookup/creation is not exposed by this subset.
Only runtime creation is accepted. World/string/savegame creation is refused. Energy uses the external
Sprites/yellowflare2.spr; the source's fallback grenade skin is retained but unused
by sprite rendering. Zealot/divine setup explicitly fails; those variants need
the original class state.

Energy Update uses scale zero while elapsed time is strictly below 0.01 seconds,
then scale 0.25. Zero scale is permitted for sprites; model zero-scale rejection
is unchanged. The renderer retains inspection rows but emits no zero-size quad.
Base Update schedules itself, explodes on contact or HP death, then applies the
strict time > start + 10 expiry. Engine scheduling uses float source delays over
a double clock, so tests cross the 0.001 deadline instead of assuming exact decimal
identity. Lifetime boundary tests use the actual source float expression.

## Replacement flight and contacts

A separate session phase, after ordinary physics and before animation, advances
live active projectiles once. Generic physics already excludes OT_SPRITE, so there
is no second integrator. Flight uses current velocity and a continuous point sweep
against primary world/current solid object shapes, excluding the shooter and the
exact projectile class. Nonsolid RAYHIT objects do not block it. One travelled
segment is committed; no speculative contact callback is delivered. A solid target
with TOUCH_NOTIFY receives its original callback after the projectile contact.
World contacts carry a polygon, never a fabricated HOBJECT.

This is replacement physics, not verified retail physics. The energy path has no
gravity/acceleration or sliding; gravity/acceleration requests fail explicitly.
It supports a maximum 0.5-second catch-up step, rejects unavailable/invalid collision,
and removes the failed projectile with a durable reason. Its 1-unit dimensions
remain inspectable but collision uses POINTCOLLIDE (zero extents). First contact
places it at the actual intersection and projects inward velocity; further flight
waits for the next original Update, which explodes/removes it. No generic friction
or force-threshold API is claimed; the straight-flight phase applies neither drag
nor a minimum-force cutoff. The existing REMOVEIFOUTSIDE world-bounds rule applies.

Original contact guards exclude the shooter, nonsolid objects and exact same-class
projectiles. A primary-world SKY material removes without explosion. Other contacts
set the explosion flag and retain a checked non-owning hit link. Shooter/target
removal clears links in source else-if order; losing the shooter does not remove
the projectile. Missing/stale targets cannot receive damage. The engine-wide
65,536 lifetime-token limit bounds object allocation; histories retain 64 results.

## Explosion, attribution and effects

Original AddImpact normalizes the stored direction, multiplies it by 2 for the
backward ray origin, then by 10 for the forward endpoint. The resulting stored
vector has magnitude 20, including for direct damage; tests preserve that quirk.
The normal ray uses solid objects with no owner filter. A miss uses the negated
20-unit vector. Hit removal suppresses the original object-dependent retry.

Explosion FX/audio are requested before damage. Radius damage retains existing
squared-distance/average-dimension falloff, no visibility filter, and snapshot
order, including the projectile itself and the shooter. Radius zero uses direct
damage with Explode type. The message sender is the projectile, while the
responsible attacker is its shooter. Typed WriteDamage accepts this difference
only for a live configured projectile and its checked shooter. After shooter
unlink, null responsibility is allowed; unrelated callers cannot forge it.
A removed responsible actor or sender cancels subsequent radius deliveries.
Repeated explosion entry is guarded, reentrant damage is guarded, removal is deferred,
and counts mean deliveries,
not necessarily health changes.

The existing sprite pass draws the real flare and decoded DTX. The original
OBJFX_FIRETRAIL_2 request (scale 0.5/SCALELIFETIME) is explicitly unavailable.
EXP_TESLA_PRIMARY is type 7; it is **not** the supported type-8 alternate recipe.
Its component publication reports failures individually. Actual Tesla impact WAV
playback is detached, radius 750, source miscellaneous-medium priority. No generic
explosion substitutes for missing components. Illumination remains unavailable.

Objects shows active state, HP, scale, motion, shooter/hit validity and trail
failure. Weapon inspection shows the live child or creation failure; Services
retains terminal histories, and Scene explosions retains component/audio reports.
These clocks/history survive hidden UI. Rendering is read-only. Unload clears
objects, voices and histories. No mutable process-global state was added; isolated
destination rehearsal preserves the source projectile and aggregate lists.

The [wind/shockwave continuation](WIND_WEAPON_CONTRACTS.md) extends the runtime-only projectile subset with OT_NORMAL scale metadata and the original type20 model/light recipe. Earlier type/resource restrictions are superseded only for this bounded subset; production Gideon and M2 remain incomplete.
