# CityHubC and SteamTunnels entry

This extends [AI runtime](ai_runtime_contracts.md), [pickups](pickup_contracts.md),
[pushable bodies](pushable_contracts.md), [compound shells](compound_shell_contracts.md)
and [music assets](music_asset_contracts.md). It covers level entry through
`GameSession::start_new_game`, which the inspector Levels tab also uses. It does
not certify complete campaign gameplay or reopen M2. Source selections and hashes
are in [the ledger](../implementation/sources/cityhubc_steamtunnels_sources.json).

## Original classes and weapons

`BoneLeech` and `DrudgeLord` use the existing original AI lifecycle, aggregate
chain, checked target handles and per-session shared animation/sound tables.
BoneLeech keeps hearing/sensing/smelling/seeing 0/300/1000/500, walking/running
75/80, jump 600, mass 125, health 10, no armor, non-Cabal and ALWAYSGIB. Its base
scale is 2 with the original random variation. Its close attack faces, runs until
within 100 units, then plays JUMP[4] and launches upward at 600 and forward at
2 × running speed. Landing stops velocity and advances the command. ComputeState
retains the original argument-based smell test. The player-only class argument
in the source still chooses the same enemy list: without FIND_SPECIFIC it does
not filter that list by class. The reversed IsKindOf operands in its standing
player check are retained.

The retail BoneLeech skeleton has `torso` but no `head` or `neck`. The donor's
GenerateHitSpheres reads uninitialized coordinates in this case. This port keeps
humanoid hit spheres explicitly unavailable and applies ordinary whole-body damage
(no regional multiplier), selecting the torso reaction slot. This deterministic
replacement is not claimed as retail hit localization. Other NPCs still require
their existing humanoid table. The client attachment/struggle handshake and attached
sucking state are not integrated: reaching the contact branch records an explicit
runtime failure. No successful attachment or damage tick is fabricated.

DrudgeLord keeps hearing/sensing/smelling/seeing 2000/200/0/1200, walking/running
70/180, jump 500, health 500, armor 100 and mass 250. Health below one quarter or
sight at most 0.70 selects AttackFar; otherwise AttackClose. Close attacks hook
within 100 units and walk otherwise, recomputing at 0.30 × seeing range. Far attacks
retain the source phases (face/fire/walk, bold taunt, approach, recompute) and
0.75/0.30 range thresholds. Far/high targets choose FIRE_STAND[0] with FIREBALL;
other attacks choose a random hook clip [4..6]. Clip completion restores SOUL_HOOK.
FIRE_KEY remains the actual firing event.

Soul Hook, BoneLeech Suck and Drudge Fireball retain their complete original
WeaponDefs entries. The two melee weapons select the source random primary slash
sound; hook damage uses the existing ray/damage path. Hook aiming uses the cached
body forward; DrudgeLord's fireball starts at the head, computes the target direction,
then replaces X/Z with cached forward as in the source. Its projectile is
`CNapalmProjectile`, not the separately named CFireballProjectile. It flies without
gravity, starts at zero scale, becomes 0.25 after 0.01 s, plays the original
750-radius loop and publishes EXP_NAPALM_PRIMARY with the 2000-radius impact sound.
The shared projectile owns flight, shooter unlink, radius-300 damage and deferred
removal. Projectile light and smoke trail remain explicitly unavailable; existing
explosion components retain their individual success/failure reports.

`BugSprayPU`, `TheEyePU` and `NightGogglesPU` preserve original resource names,
display IDs, item/weapon IDs and amounts. TheEyePU is also accepted by destruction
SpawnObject. These are **offer-only pickups** in this increment: player Bug Spray,
remote-eye control and night vision are not integrated. The inventory refuses
unsupported offers, leaving the object visible; no pickup acknowledgement or
successful inventory action is invented.

## Source rows that never create an object

The base donor and mounted OBJECT.LTO have no `Gremlin` or `RocketAmmoPU` class
registration. The add-on `GremlinAI` is distinct; RocketAmmoPU is an original AI
lookup typo, not HowitzerAmmoPU. The loader accounts for those exact two names as
`AbsentOriginalClass` and never aliases them. Other unknown names still block
entry. Read-only destination inspection and actual rehearsal retain these rows
with explicit omitted counts. The later LithTech world loader's missing-class
branch corroborates continuing past failed object creation; this is not evidence
of a working Gremlin or rocket pickup.

CityHubC also contains a CDestructableModel with no InitFilename or Filename.
SteamTunnels has three DetailSprite rows pointing to absent
`sprites/bloodsplat1.dtx` through `bloodsplat3.dtx`. Their similarly named SPR files
and DTX files in another directory are not substituted. These known cases become
`MissingResource` **only after** original property validation and creation reach
the corresponding missing-model/sprite failure. Wrong property types, unsupported
spawn rules, malformed existing resources and other missing resources still fail.
All omitted rows remain visible in source accounting and resource inspection.

## Dynamic polygon tables

The source permits Pushable/Gravity with BoxPhysics=false. The two tunnel tables
now use the existing atomic translating-body planner with their enclosing bounds
for motion against other bodies. Other bodies still query the authored triangle
boundary. The BOXPHYSICS flag is not changed; rotation, GOTHRUWORLD and unsupported
stack motion retain their existing restrictions. This conservative dynamic motion
is replacement engine behavior, not a retail physics equivalence claim.

Only non-box world models created with gravity opt into triangle-boundary geometry.
Static world models and container volumes retain strict plane and closed-shell
validation. `table2` has ten faces inconsistent with their stored planes (up to
about 7.74 units), plus unmatched mesh edges; this is not an epsilon adjustment.
The opted-in mode keeps the actual vertices and fan triangles, using stored normals
only for winding. Unmatched or non-manifold edges require boundary-only collision; they never
qualify as closed solid geometry. Invalid indices, transforms, normals and
non-convex face fans still fail. Existing work limits remain. Off-plane faces disable the
convex plane shortcut. A boundary-only mesh has **no claimed solid interior** and uses
only triangle collision/overlap, so gaps are not filled with a box. Inspection
shows off-plane face and open-edge counts. No authored DAT bytes are modified.

## Authored music style filenames

Track1's sections use style labels such as `Track1-a`, while files are named
`track1a.sty`. The installed `iMusic.ini` [Styles] NameN/FileN pairs provide the
mapping. MusicCatalog reads that directory-local index before requesting styles,
retains both the section label and resolved filename, and still validates the
section/style identifiers and musical timing. No punctuation-stripping heuristic
or cross-directory search is used. With no mapping, the existing label + `.sty`
profile remains available.

The index is bounded to 64 KiB, 512 bytes per line, 2,048 fields and 256 styles.
Declared pairs must be complete, have safe ASCII basenames below 80 bytes and a
`.sty` extension, with unique case-folded labels. Duplicate fields/Styles sections,
unsafe paths, embedded NULs, malformed counts and unreadable/ambiguous/symlinked
existing indexes fail. Existing resource-resolution and cumulative asset budgets
remain. GUID/category/time-signature index metadata remain opaque; the decoded
section/style identifiers, not index metadata, determine identity validity.

## Verification boundary

`campaign_entry.02_cityhubc` and `campaign_entry.04_steamtunnels` exercise real
private assets, start_new_game, destination inspection, one second of simulation
and unload ownership. `campaign_entry.actors` covers constructors, leech body
damage/jump, lord decisions/fireball, projectile scale/loop/explosion/unlink and
refused pickup offers. Synthetic container tests distinguish strict closed volumes
from explicit open triangle boundaries; pushable tests retain flag, movement and
rotation restrictions. Music asset tests cover the index parser and mapped file
lookup. These tests establish startup and selected behavior, not complete level
traversal, visual acceptance or the pending inventory/attachment interactions.
