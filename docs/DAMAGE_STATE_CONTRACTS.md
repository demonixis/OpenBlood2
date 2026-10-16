# Original CDestructable state preservation

Original CDestructable Save/Load value fields now have an owned typed
`DestructableState`: health/armor and their four caps, mass/resistance, death health,
raw dead flag, remaining death delay, nigh-invulnerability, god/trigger-only and
physics flags, last reaction type/location/direction/accumulators, trigger strings
and destruction spawn/velocity. This is aggregate state, **not a complete player
checkpoint, retail save format or level transfer**. See [source ledger](M2_DAMAGE_STATE_SOURCES.json)
and [validation](../../Documentation/Implementation/M2_DAMAGE_STATE.md).

## Source rules

- Save writes health 1,000,000 when CanDamage is false. Load infers CanDamage from
  `health < 1,000,000`, retaining the raw saved health. This also means exactly
  1,000,000 supplied as ordinary health restores invulnerability. Do not conflate
  the aggregate's CanDamage with a brush's separate Destructable/removal flag.
- Load assigns raw values, without pickup grants, maximum-stat clamps or attribute
  recalculation. Death health is a separate original accumulator: it starts at 1,
  ordinary post-armor damage subtracts from it, and Death sets it to zero. Heal and
  MegaHeal synchronize it to the resulting HP. Source SetHitPoints does **not**
  synchronize it. Negative death health is valid, including before ordinary death.
- IsDead returns false while the death delay is positive, even with raw dead=true.
  MID_UPDATE subtracts frame time only while raw dead is true and delay is positive;
  overshoot below zero is preserved. Load restores the **remaining duration**, with
  no world-clock offset. Raw-dead damage guards still reject further packets;
  destruction notifications are not postponed by this getter.
- The original nigh-invulnerable modifier multiplies ordinary damage by 0.05 for
  CBaseCharacter owners, after localization and before impulses/armor. It does not
  apply to generic damage owners, and Death bypasses it. The saved flag and modifier
  are real; timed powerup collection/expiration and player powerup-array association
  are not thereby implemented.
- Last reaction type and saved location are separate from mapped hit node/side.
  The source location byte is never initialized or written by hit localization in
  the examined source; this port initializes it to zero rather than saving undefined
  memory or inventing a node mapping. Raw bytes remain round-trippable.

## Binding, validation and lifetime

The destination must already have a live engine owner. Load retains that owner and
its inventory binding; it never restores the old source handle. Original intrusive
list insertion occurs only when the aggregate is unlinked. Repeated Load must not
duplicate membership, and destruction removes it once. No source object, attacker,
list node, inventory pointer or resource handle appears in the state value.

Save/Load reject an active damage callback or pending/stale owner. Load also refuses
an associated player, whose HUD/client state is not yet restored transactionally.
Save replaces output only after validation. Load validates every field and the
available SpawnObject grammar/class before changing existing values or list state.
No creation, triggers, damage delivery or pickup grant is performed by Load.

Health/armor/caps/mass/resistance/accumulators must be finite and nonnegative;
death health and delay may be signed finite values. Vectors use existing finite
physics bounds. Unknown damage types and strings over 100 bytes or with embedded
NUL reject; empty source strings remain valid. Spawn failures preserve existing
state. Owned strings outlive their source. Last raw damage packet and attacker
memory are cleared; only the original saved direction/reaction fields are restored.
The old diagnostic hit-pose report is cleared, not advertised as destination data.

## Inspection and remaining gate

Exit processing captures damage state alongside the previous inventory capture,
outside callbacks and independently of UI visibility. These are separately labelled
aggregate snapshots, not an atomic whole-player save. Only an active associated
player is captured. Failures clear that capture candidate and retain a visible error;
unload/PreStartWorld clears both records. NEW_LEVEL continues to refuse before
world/player teardown, regardless of successful aggregate captures.

The Objectives tab shows saved values, source sentinel meaning and missing transfer
services. Player, NPC and brush inspection show live death health/delay and raw versus
effective death where applicable. No UI draw mutates state or advances clocks.

Player-owned timers/strings/movement, client saved data/association, engine object
state and safe named destination entry remain unported for keep-alive. General
saves, full NPC/brush restoration, other damage modifiers and the previously absent
MID_UPDATE maximum-stat clamps are not implemented here. Do not treat these value
snapshots as complete object restoration or M2 acceptance.
