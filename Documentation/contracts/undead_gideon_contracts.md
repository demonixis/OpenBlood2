# Original UndeadGideon

The original Ancient City boss now has its fixed model/skin, independent animation
and sound table, five weapons and runtime inspection. Its profile retains 12000
HP, registered random HP 50, zero armor, mass 3000, strength 5, walk/run/roll
180/350/320, jump 360, hearing/smell 2000, sense 500, sight 1000, JUMP/DODGE/ALWAYSRECOIL
and non-Cabal membership. Original damage impulses remain enabled. Existing
original difficulty and scale adjustments apply; resources remain external.

The first scheduled update publishes full health and captures the current HP.
Damage applies once through the typed boundary, then publishes the actual ratio
and runs the base reaction. A zero/nonfinite denominator fails explicitly. Health
requests without a client are distinguished from deliveries.

ComputeState preserves the argument-based smell test, normalized sight threshold
and height override. Close/far attack face the cached target on every phase and
retain the original distance, height, ledge and lost-target branches. Far attacks
choose the original 70/50-percent magic split or vomit; a high target selects
MELEE vomit. Close attacks use spear unless an imprisoned player permits the
40-percent vomit/shockwave choice. Playback completion restores spear. The existing
[weapon contracts](gideon_weapons_contracts.md) cover actual imprisonment/vomit;
Skull, lightning and shockwave use their existing implementations and limitations.

Fire uses the actual head node, normalized target direction with cached forward
XZ replacement, then normalized rotation alignment. It intentionally omits the
ordinary clear-shot gate. Missing poses/services remain failures. Actual model
fire markers dispatch the override, not a diagnostic-only imitation.

Forward jump uses MELEE playback, jump Y and three times run speed in XZ. Landing
zeros all velocity and retains the original self-class check: command 999, no
Metacmd increment, then relaunch on the next call. Virtual jump dispatch also
covers shared navigation. Lateral dodges use run speed, finish only on primary
world support with nonpositive Y velocity, then increment Metacmd. Recoil chooses
Dodge; the original right-before-left dodge selection is preserved.

## Complete-scene initial placement

Ancient City's Fanatic-Jerry overlaps the later Box-ammo1 row by about 1.493 units;
other difficulties expose a Shikari overlapping the later Model-FanaticBody.
Initial grounding cannot see rows not yet created. Session initialization now
repeats the existing bounded face-adjacent actor separation after all rows exist,
before PostStartWorld. Candidate ordering, combined-body distance limits, stricter
world-model shell budgets, clear endpoint and checked swept route are unchanged.
A refusal marks the source entry failed and suspends the actor. No runtime
penetration escape or arbitrary relocation is introduced. Tests cover successful,
idempotent placement and an impossible move with unchanged pose/counters.

## Animation loop roundoff

At 60 Hz a 300 ms loop could subtract a rounded endpoint a few ULPs beyond its
remaining step, producing -1.7763568394002505e-14 ms on the next loop. The internal
remaining duration is now clamped to zero after subtraction; incoming negative
or nonfinite clocks still fail. A 1800-frame test verifies all boundary keys and
valid clocks; the real Ancient City scene runs 180 frames in every difficulty.

This is verified entry, selected combat behavior and rendering, not complete boss
battle/campaign traversal or NPC save support. M2 closure and its GPU reservation
remain unchanged. Source hashes and raw evidence are in the implementation ledger.

[Save/load lot 12](save_load_contracts.md#implemented-bosses-and-special-enemies-lot-12) saves UndeadGideon's full health; the boss bar is
re-armed by the constructor's flag, as in the original.
