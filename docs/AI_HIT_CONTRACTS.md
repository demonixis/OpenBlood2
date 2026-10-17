# Original NPC hit localization and recoil

> Extended by [Gideon lifecycle contracts](GIDEON_RUNTIME_CONTRACTS.md): production initialization, AI_Update, health delivery and fifth-table isolation are now integrated. Earlier blanket boss-registration omissions are historical. M2 remains incomplete.

> Extended by [node visibility and player model restoration](CHARACTER_NODE_CONTRACTS.md):
> original named resets now affect rendering/hit candidates, and the selected Load
> model/flags/velocity tail is available before client association. Earlier blanket
> node-hide/Load-tail omissions are historical. Full engine/client restore, limb/gib
> behavior and the M2 transfer gate remain incomplete.

> Continued by [ordinary corpse contracts](CORPSE_CONTRACTS.md): original Dying/CCorpse now
> transfer ordinary deaths. Earlier blanket corpse omissions are historical;
> fire/gibs, corpse damage, required FX and complete M2 remain unavailable.

> Extended by [NPC weapon-drop contracts](WEAPON_DROP_CONTRACTS.md): the selected
> Cabal drop, independent falling hand and Beretta collection now work. Corpse/gib
> and remaining death services still fail explicitly; M2 is incomplete.

Production CultistAI/CivilianAI now enable the original CDestructable hit-region
selection and AI_Mgr nonlethal damage/recoil branches after successful initialization.
[Source ledger](M2_AI_HIT_SOURCES.json) and [execution evidence](../../Documentation/Implementation/M2_AI_HIT.md)
record the selected source and tests. This extends [runtime contracts](AI_RUNTIME_CONTRACTS.md)
and [character measurements](CHARACTER_ANIMATION_CONTRACTS.md); M2 remains incomplete.

## Localization and damage

The original algorithm projects the incoming hit position along a segment whose
length comes from the current animation's **unscaled** user dimensions and the
strictly largest absolute direction component. Tied axes and zero direction give
no segment/candidate; explicit zero handling replaces floating-point NaNs. Current
world-space animated nodes (including object scale/rotation) are compared against
the shared original fixed-radius hit spheres. Candidates must project within the
segment, be within their radius and be strictly closer than the previous best
(initial distance 999). Equal distances retain the first node. The radii do not
receive an additional object-scale factor.

The typed node query distinguishes absent names from unavailable/ambiguous poses.
Missing head data fails. Missing later nodes retain the previous node transform,
matching the source's ignored failed transform read; the missing count is visible.
Invalid present-node poses never use that fallback. The bounded implemented path
uses visible nodes: current actors have no per-node hide state, and renderer and
hit queries both include every node. Limb hiding/dismemberment remains unavailable.
This is not an always-successful GetModelNodeHideStatus facade.

The raw indices map to neck 1, torso 2, right arm 4, left arm 7, left leg 10 or
right leg 14. The source `DAMAGE_TYPE_NORMAL` is zero, so its
`!(type & DAMAGE_TYPE_NORMAL)` condition also takes the random fallback for normal
bullet misses. Preserve the inclusive raw draw 0..18: 18 maps to -1, still receives
the non-neck/non-torso half-damage multiplier, then skips HandleDamage reactions.
Missing required services fail before HP/armor or attacker-memory mutation; they
never become successful random hits. Portable random sequences are not verified
against the retail CRT.

Facing and reversed incoming yaw are independently mapped to [0,2pi); their
difference is deliberately **not wrapped**. The original front test selects side
0 when difference <=pi/2 or >=3pi/2, otherwise back side 6. This includes the
negative-angle front classification quirk.

Resistance precedes localization; neck/torso/limbs multiply damage by 1.5/1/0.5
before the existing armor absorption. Cabal neck hits set HP to zero before armor,
including zero-damage packets, as in the source. SoulDrudge is still unported.
Death-type damage bypasses localization and retains previous node/side metadata.
The post-armor amount accumulates until the original aggregate MID_UPDATE resets
it; reaction type only changes on nonzero damage. Raw delivered damage remains
separately inspectable. Existing checked non-owning attacker memory is retained.
NPC damage impulses are still unavailable; this increment does not enable them.

## Reactions and AI_Update

AI_Mgr forwards MID_DAMAGE through the base/aggregate chain exactly once before
HandleDamage. Dying/removing actors and mapped node -1 return early. FLAG_LIMP
latches the first left/right leg injury only below half maximum HP. The existing
original injured walking-animation selection consumes that same field.

Noninterruptible SCRIPT (missing flag bit 1) keeps its state. Otherwise ALWAYSRECOIL
wins; accumulated damage >= one quarter of **remaining** HP selects Recoil except
in Teleport/Dodge. Smaller hits outside AttackClose/AttackFar call the original
ComputeState. CivilianAI then applies its original EscapeRun override after the
base reaction; it does not play Cultist recoil merely because the base selected it.
A latched runtime failure cannot be revived by that subclass override.

MC_Recoil uses region plus front/back side to select the original 12-entry table,
starts a nonlooping animation and records command 19. Completion increments the
phase; StateRecoil phase 2 calls ComputeState. Physics/animation clocks remain
external to AI_Update and independent of rendering/UI. FIRE uses the original
**unmapped** random raw node: unmatched draws keep the current animation but still
make it nonlooping and start command 19. The typed Fire damage value is accepted;
this does not implement fire weapons, burning effects or fire death.

Safety adaptations: only newly accepted nonnested damage receipts invoke the base
reaction; refused packets do not replay stale hit metadata as the unchecked source
could. CivilianAI still applies its unconditional source EscapeRun override to
a received damage message, even when the aggregate refused it, unless removal or
a latched runtime failure prevents the state change. Removal during a damage trigger cancels the reaction while preserving deferred
destruction. Checked model/animation/resize failures suspend the NPC through the
existing runtime failure path, and phase increment rejects integer overflow.

## Inspection and remaining boundary

The Skeletons panel exposes activation, accepted damage/query counts, raw/mapped
node, side, fallback, missing nodes, distance, multiplier, Cabal head-kill flag,
accumulated damage, injured leg, reaction/recoil counters, selected slot and failures.
The last localization query is labelled separately from accepted damage: GPU smoke
queries use real production poses without inflicting damage or changing authored
flags. Hidden-inspector and reload checks repeat those queries; old identities fail.
Private headless tests drive actual production damage and AI_Update recoil.

Lethal health changes are applied, but the first unavailable original death service
(weapon drops/corpses/gibs/death effects) explicitly suspends the actor. It remains
inspectable with zero HP and an error, not a fabricated corpse or completed kill
sequence. This boundary, Gideon, ExitTrigger, required client FX and other missing
session/state services keep M2 open. The earlier no-player self-target boundary
also remains. Only the documented Linux configurations have been verified.

The [burning death/dismemberment continuation](DEATH_EFFECT_CONTRACTS.md) supersedes
blanket fire/gib/corpse-damage omissions for its selected original paths. Real
flame/smoke particles, burned corpses, runtime limbs and flesh models now run with
inspection. Blood-spurt/splat FX, generic gib/save services and full M2 remain open.
