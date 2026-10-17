# Ordinary original NPC death and CCorpse

> Current extension: [original blood pools](blood_splat_contracts.md) now render primary-world corpse hits. Earlier pool omissions below are historical; blood spurts/gib impact callers and M2 remain incomplete.

> Extended by [initial placement contracts](corpse_placement_contracts.md): the original raw body now receives bounded source-side clearance before transfer. Impossible placements fail visibly and roll back deferred, retaining the source; general/retail depenetration remains unavailable.

Selected original `AI_Mgr::HandleDamage`, `MC_Dead`, `AI_STATE_Dying`,
`CreateCorpse` and `CCorpse` now handle ordinary Cultist/Civilian death.
[Source selections](../implementation/sources/m2_corpse_sources.json) and
[validation](../../../Documentation/Implementation/m2_corpses.md) define this subset.
M2 remains incomplete; fire, gibs, limb loss and client blood-pool FX are unavailable.

## Death selection and transfer

Preserve the original Cabal weapon-drop attempt before lethal response. A nonsolid
object support sets ALWAYSGIB; primary-world support has no invented object token.
Fire requires burning services. Explosive/ALWAYSGIB/at-least-0.90 frame damage
requires gibs unless NEVERGIB is set. These missing branches suspend the source
with a diagnostic. NEVERGIB does not implement fire. Ordinary eligible deaths
consume the original 0..99 optional kill-voice draw; requests below 6 for a player
attacker are counted, not played. The original post-damage script/recoil selection
still executes; AI_Update subsequently selects Dying for zero HP.

CDestructable now maintains the original per-update damage fraction in addition
to damage amount. Armor-adjusted damage accumulates relative to maximum HP;
MID_UPDATE clears both. DEATH initializes amount=maxHP and fraction=100 before
adding the resisted incoming packet. Nonpositive maximum HP skips division as an
explicit safety adaptation, rather than producing nonfinite state.

MC_Dead's first call stops its separate per-actor retained sound, selects one of
12 death clips by region/side (or the NIGHTMAREDEATH 50% branch), and sets command
20/animating. It does **not** start an animation on the NPC or increment Metacmd.
The second call creates a corpse, hides the NPC and requests its next-update
removal. Dying phase 2 directly assigns Inactive without SetNewState side effects.
Shared CAnim_Sound voices and the separate actor sound are distinct ownership.

The corpse receives owned model/skin filenames and sound-root text. The typed
GetModelFilenames validates a live model and leaves outputs untouched on failure.
There is no pointer truncated into the source's 32-bit m_UserData. Creation uses
CMovement's cached position/rotation and m_vDims, including the source's timing.
The sanity fallback draws a death clip if the selected clip precedes death[0].
Visual scale is copied, but collision dimensions remain the raw death-clip
user dimensions. Y placement subtracts `(oldDims.y - corpseDims.y) - 0.001`.
This now uses the bounded atomic initial-placement service described above, not
proven retail SetObjectDims2/collision parity. A failed setup removes the partial child deferred
and retains the visible source; the unchecked original could hide it anyway.
Spawn reports retain checked child identities and outcomes after source removal.

## Corpse services and lifetime

CCorpse is a runtime-only original model class; world/string/save construction is
rejected. Setup requires real model initialization. Original visible/ray-hit,
gravity, model-key and outside-world-removal flags, HP50, armor0, mass200 and
mass-derived friction9 are retained. Damage impulses are disabled. Flesh, saveable
and singularity user bits retain their numeric values without claiming save/restore
or singularity services. The existing character-body integrator supplies gravity
once; corpses are nonsolid and independent of the removed actor or its weapon.

Hit-sphere measurements reuse the same original formulas in CAnim_Sound. Unlike
CCorpse's unchecked source reads, missing head/invalid poses fail; later missing
nodes use the existing documented previous-node fallback. These are measurements,
not implemented corpse hit/limb damage. All damage and trigger packets are refused
before aggregate mutation, with a separate diagnostic that does not stop the
working death animation. Hidden limbs, SoulDrudge-specific limb eligibility,
corpse destruction, burning/smoke, gibs and save/restore remain unavailable.

Initial scheduling is 0.01 seconds, then 0.1 while the nonlooping animation runs.
Completion casts the original 40-unit downward solid ray. Only a primary-world
hit records the blood-pool request at hit Y+0.1; the unavailable CClientSplatFX is
explicitly reported and never counted as rendered. Single-player corpses persist
with no further updates. Multiplayer-mode lifetime logic sets now+29.9 and schedules
30 seconds, then removes on the next due callback. This isolated rule is tested;
multiplayer gameplay is not implemented.

Model keys parse argv[0] with the bounded Sparam reader: play_sound, optional
sound_random (B2_LITE variants 1..3), radius and volume. Actual WAV decoding and
attached dry playback use AI_MEDIUM priority3 and inner radius=outer*0.1. Malformed,
nonfinite or out-of-range numeric requests fail explicitly. The original omits
GETHANDLE: voices auto-release on completion/owner removal and repeated keys may
overlap. last_sound is a non-owning inspection ID, never a busy latch or destructor
kill. This follows Blood II's own AppHeaders contract; the later engine generation
is corroborating evidence only. Reverb and priority arbitration remain unavailable.

## Inspection and limits

Skeletons lists corpses including hidden/inactive/missing models, their raw
geometry dimensions, visual scale, animation/time, update/removal deadlines,
real sound attempts, blood-pool result and damage refusal. NPCs expose death slot,
clip, accumulated fraction, transfer count and unavailable optional voice requests.
The normal model renderer samples corpse animation independently of the inspector.

After subsequent Gideon/ExitTrigger integration, SubwayA has 204 created / four
transferred sounds / 372 editor-only / zero unsupported / zero failed source entries.
Corpses are dynamic instances, not extra authored coverage. Required FX,
corpse/fire/gib services, safe NEW_LEVEL and complete original session acceptance
keep M2 open. Linux-only tests and synthetic diagnostics do not establish
original-executable or platform parity.

The [burning death/dismemberment continuation](death_effect_contracts.md) supersedes
blanket fire/gib/corpse-damage omissions for its selected original paths. Real
flame/smoke particles, burned corpses, runtime limbs and flesh models now run with
inspection. Blood-spurt/splat FX, generic gib/save services and full M2 remain open.
