# Original MadScientistAI

This extends the [NPC lifecycle](ai_runtime_contracts.md), [perception](ai_perception_contracts.md),
[states](ai_state_contracts.md) and [escape](ai_escape_contracts.md) contracts. Source:
`ObjectDLL/MadScientistAI.cpp/.h` (03_Museum). Retail parity is not claimed.

## Construction

The constructor sets hearing 0, sensing 200, smelling 0, seeing 1000, walk 75 and run 180,
keeps the AI_Mgr roll (2) and jump (500) speeds, flags 0 and the Cabal faction, with 30 hit
points, no random health and no armor. `BEGIN_CLASS` declares HitPoints, RandomHitPoints,
ArmorPoints, AIState and the digitless AIWeapon; `AI_Mgr::ReadProp` reads only
RandomHitPoints and AIState of these, so authored HitPoints/ArmorPoints/AIWeapon are
accepted and ignored and the scientist carries no weapon. The base difficulty applies
(no Cultist 0.5 factor); `Random(1, 0)` keeps the reversed-endpoint health rule.
`PostPropRead` fixes `Models/Enemies/madscientist.abc` and its skin for every
non-savegame precreate. The class uses its own shared CAnim_Sound table (slot 9) with the
`sounds/enemies/madscientist` root.

## Behavior

`ComputeState` selects the authored state without a stimulus and Escape_Hide with one.
The overridden Idle state computes stimuli each update: a stimulus schedules 0.001 s and
recomputes the state; otherwise it schedules 0.1 s, plays MC_Idle and recomputes after it.
The overridden Escape_Hide has no hiding-spot search: without a target it recomputes the
state; otherwise it faces the stored target position, undoes the facing command increment,
plays the beg taunt (MC_Taunt_Beg) and recomputes once the taunt finishes. Failed
perception remains a visible state failure.

## Shared rules added with this class

- `madscientist.abc` names two nodes `torso`. Node lookups (hide status, transforms and
  `QueryModelNodeTransform`) now resolve a duplicate name to the first node in model order,
  as later LithTech `Model::FindNode` does with its first case-insensitive match. This
  supersedes the earlier ambiguous-node refusal.
- The model has only idle, talk, taunt (dance/beg/wave), two death and limb clips: no
  recoil or locomotion. The source `GetAnimIndex` leaves such an index invalid, and
  `AI_Mgr::SetAnimation` then keeps the current clip (the engine ignores the out-of-range
  `SetModelAnimation`). The later [invalid-ordinal continuation](ai_animation_clearance_contracts.md)
  ports that no-op: a nonlethal hit large enough to recoil runs MC_Recoil on the retained
  current clip, and the scientist keeps reacting instead of suspending. This supersedes the
  earlier explicit missing-clip failure.
- Without any monster, the original Enemies list falls back to the Cabal list, which holds
  the observer itself. In game the player belongs to both lists; tests without a player
  must create a monster first.
