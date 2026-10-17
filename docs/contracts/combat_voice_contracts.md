# Original combat voice callers

This extends [VoiceMgr](player_voice_contracts.md), [death effects](death_effect_contracts.md)
and [player restoration](player_restore_contracts.md). The [source ledger](../implementation/sources/m2_combat_voice_sources.json)
and [execution evidence](../../../Documentation/Implementation/m2_combat_voices.md)
record the selected original branches. M2 remains incomplete.

## Damage feedback

CPlayerObj::UpdateStats compares current truncated unsigned health with its previous
integer value. A decrease records the net loss and consumes the original random
0..99 draw against `69 + loss`, including when this threshold exceeds 99. Fire
selects event 5 (Burning); every other last effective damage type selects event 2
(Pain). The single-player filter follows the draw; client busy, external-camera
and cooldown gates remain in VoiceMgr. Unchanged integer health, healing and
ammo/armor changes alone consume no pain draw. Several hits before a stat update
produce one net-loss decision. Fractional health in the compact HUD stays exact.

The original initial/Load health and last-damage caches start at zero. Restoring
lower health never compares against an unrelated pre-restore baseline. Force-send
of unchanged stats does not replay feedback. Nonfinite/out-of-range health fails
the voice conversion explicitly without undefined casts; the chance comparison
uses widened arithmetic. Valid game health uses the original rules. Cosmetic
queue or playback failure never undoes accepted damage. Full player DoDeath,
suicide, idle, spawn, taunt, powerup and dry-jump callers remain separate; group
availability alone does not implement those callers.

## Kill and corpse replies

AI_Mgr retains the existing original draw order: seven-percent BigGib inside the
actual gib branch, then six-percent Kill after the lethal-hit branch. Each NPC
reply requires the accepted lethal packet attacker to equal the checked associated local player.
This supplies the original WhoKilledMeLast identity; AI retaliation memory is
separate and is not overwritten by instant Death packets.
The original missing-hit-node early return still skips both voice decisions;
no replacement localization or chance is invented. Fire/NEVERGIB behavior, body
placement, limb creation and removal are unchanged.
If both replies qualify, BigGib is queued first; normal VoiceMgr arbitration can
refuse Kill while that voice plays. No extra NPC chance draw is introduced.

CCorpse consumes its existing seven-percent draw after gib creation and burn
cleanup. This caller deliberately has **no killer attribution filter**. It can
address the player after another actor destroys a corpse. Source single-player
and client voice gates still apply. Client messages contain values and survive
the killed actor's destruction; the retained local sound belongs to the checked
player, never the dead actor/corpse. World exit and player removal retain the
existing voice cleanup policy.

The source raw single-player global is replaced by a bounded search of the
existing Cabal list, including inactive entries. Object identities and client
association are checked through the owning adapter; pending, foreign, unassociated
or ambiguous players cannot receive a reply. No new global pointer, dependency,
engine facade or registry is added. Multiple-player behavior remains unsupported.

## Inspection and validation boundaries

Session / Original player voices exposes integer health, last loss/draw/event,
checks and eligible requests alongside actual starts, refusal reasons, resources
and failures. Skeleton exposes NPC/corpse rolls, eligible replies and queued
messages. A queued message is not counted as successful playback. Reading or
hiding the inspector never advances the decisions.

Generated fixtures cover net and fractional damage, healing, ordinary versus fire
selection, busy refusal, forced stat resend, game-mode filtering and restore
baseline invalidation. Private fixtures use original Cultist/corpse classes and
external models/WAVs on a labelled diagnostic floor: player and non-player kills,
instant-Death attribution distinct from earlier retaliation memory, the missing-hit-node
early return, postmortem replies without player attribution, streamed PCM advancement, actor
destruction before message delivery and world cleanup. These isolated encounters
are not an authored playthrough or a retail probability/random-sequence comparison.
The continuous train and transition regressions remain separate acceptance evidence.

The [single-player death continuation](player_death_contracts.md) supersedes the missing selected Caleb death/corpse/gib and client death-camera paths above. Active powers, multiplayer death and whole-world/dead-player restoration remain unavailable. M2 is still incomplete.
