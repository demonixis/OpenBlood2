# Original M16 and grenade

Read the [source ledger](M2_ASSAULT_SOURCES.json), [weapon](WEAPON_CONTRACTS.md),
[projectile](ENERGY_PROJECTILE_CONTRACTS.md), [pickup/drop](WEAPON_DROP_CONTRACTS.md),
[AI firing](AI_FIRE_CONTRACTS.md) and [model](MODEL_AND_COLLISION_CONTRACTS.md) contracts.
See [execution evidence](../../Documentation/Implementation/M2_ASSAULT.md).
M2 remains incomplete.

## Weapon and acquisition

Original weapon ID 9 retains every WeaponData field. Primary fire uses bullet
pool 1, one round/vector, damage 10–16, range 3500, spread 75/25 and a looping
animation. Alternate fire uses **DieBugDie pool 5**, five charges, damage 100–140,
speed 1500, radius 200, range 400 and a nonlooping animation. GetAmmoType and
CheckAmmo now take the original alternate-mode argument on both server and view
paths. Having bullets cannot authorize a grenade; having charges cannot authorize
a bullet. Insufficient charges consume none. Existing FIRE_KEY delivery, accepted
shot accounting, hearing, retained fire sounds and deferred ownership still apply.

CAssaultRifle has no dual-hand acquisition (two-hand strength requirement zero).
AssaultRiflePU uses m16_pu, display name Assault Rifle, resource 353 and **50 bullets**,
not five alternate charges. Full inventories/ammo refuse duplicates; partial pools
clamp normally. NPC detached-hand transfer produces this real pickup and survives
owner removal. KeepAlive snapshots preserve both ammo pools and reject a fabricated
left M16. The typed pickup service accepts explicitly registered supported weapon
IDs 1, 2, 3 and 9; IDs between them are not implicitly accepted.

The original case-sensitive ASSAULTR prefix and generic AI firing service are
connected. The installed male Cultist lacks fire_stand_rifle and
fire_stand_autorifle. Its original animation table cannot supply this action:
automatic AI_Update reports a missing clip instead of silently substituting pistol
animation. Direct original AI Fire with the M16 damages a player in a labelled
arena; that service test does **not** establish autonomous M16 combat or add Fanatic/
Prophet classes. Current authored CityHub NPC loadouts are unchanged.

## Original grenade rules

CGrenade is a runtime-only OT_MODEL projectile using the original Grenade ABC/DTX,
1-unit half-dimensions, gravity, RAYHIT, HP5, EXPLODE damage and a three-second fuse.
It is not a point projectile or an instantaneous radius hit. World/string/save
creation rejects. Model resource failure remains an actual creation/setup failure.
The existing checked shooter link permits independent flight after owner removal.

Update starts/retries the attached looping assault/projectile WAV, remembers the
position, calls base Update, then changes pitch/yaw. Speed strictly below five
zeros spin velocity and writes pitch zero without changing the stored pitch field.
Otherwise the stored angular velocities advance with the source frame time.
The fuse expires only when time > start + 3. Damage death and initial live-character
contact also trigger the original explosion path on the next scheduled update.

Before the first bounce, a living CBaseCharacter uses base impact handling; a dead
character is ignored. Later contacts use the bounce branch, including characters.
The shooter/nonsolid objects are ignored. Up to five accepted bounces consume the
count. Polygon contacts apply the original post-engine-velocity expression
`(v - normal * dot(v, normal) * .3) * .5`, random pitch/yaw in +/-2pi and the actual
bounce WAV. Nonpolygon contacts zero velocity and play that WAV. The grenade
branch does not inherit the base sky-discard rule. No sixth bounce sound/spin is
invented. Removal and target callbacks remain checked and deferred.

The source calls base Explode and then calls it again from CGrenade::Update on
contact/death with gravity. **Both sequential explosion passes are retained**,
including damage and audio; fuse-only expiry has one pass. A separate active-call
guard prevents reentrant recursion. Histories retain pass count and total deliveries
while each pass still uses the existing checked radius snapshot/falloff and source
shooter attribution. Removal can interrupt deliveries. Original AddExplosion uses
world-up and EXP_GRENADE (3), with the actual detached c4/explosion_1 WAV. Each
available component is published separately; missing components are not reported
as a complete effect. Source screen shake and object-bound SMOKETRAIL_1 remain
explicitly unavailable. The fragment recipe with the same number is not silently
used as a replacement trail.

## Replacement gravity/collision boundary

A dedicated typed AdvanceGrenade service performs at most 60 substeps at <=1/120s
for a <=0.5s call. It sweeps the actual 1-unit box against primary geometry and
current solid objects, excluding the shooter, then commits the accepted segment
and runs the original contact callback. Generic model physics excludes all
CProjectile instances, so this model has exactly one integrator. Container
physics uses the existing original volume callbacks. Velocity/pose inputs,
resource presence, embedded starts, unavailable queries and recursive motion fail.

Air displacement uses v*dt + a*dt²/2; ground support projects inward acceleration
and uses the existing exponential friction policy with source coefficient 18.
Contact projects inward velocity before the original bounce calculation; no
elastic restitution is invented. A 0.001-unit separation prevents repeat interior
contacts. Unused travel after a contact is discarded for that bounded substep.
This conservative box solver can stop/slide rather than recreate retail bounce
arcs. It is **not recovered retail grenade physics**, a rigid-body solver, or a
claim of parity. Failed movement is retained and removed with an explicit error.

## ABC metadata and inspection

The original M16 view model contains finite, inverted per-key bounds in the
unused end_alt_fire_craig clip. Reference readers consume then discard those
bounds. The new reader retains every value and marks invalid ordering on the key
and model; it never uses per-key bounds for pose, animation dimensions or collision.
All coordinates must still be finite and bounded; timestamps, monotonic order,
counts, animation/node/global bounds, pose/deformation data and payload lengths
remain strictly checked. This is a documented metadata warning, not a synthesized
pose or a silently repaired animation. Rendering samples the actual model.

Weapons shows separate primary/alternate ammo pools and diagnostic M16/charge
grants at the existing frame boundary. Objects shows grenade fuse, motion, spin,
remaining bounces, contacts, live loop and missing trail. Diagnostics retains
terminal pass/delivery counts. Both the selected view weapon and world model
inspection expose invalid-key-bound warnings. Reading or rendering advances no
clocks. Hidden objects, deferred removal and unload/reload retain the existing
checked resource ownership. No new dependency or mutable original global is added.

Chrome, multiplayer boost, view-key sounds, recoil, muzzle illumination, full
composite FX, supported autonomous rifle-bearing NPCs, retail physics/presentation
parity and safe NEW_LEVEL remain open. CityHub is 433 created / 31 transferred
sounds / 541 editor-only / 20 unsupported / one failed. Missing fallengrills geometry
remains a failure. These counts do not close M2.
