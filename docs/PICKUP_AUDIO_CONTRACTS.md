# Original pickup collection audio

M2 remains incomplete. This extends [pickup contracts](PICKUP_CONTRACTS.md),
[key contracts](TRIGGER_CONTRACTS.md) and the existing detached voice lifetime in
[scene explosion contracts](SCENE_EXPLOSION_CONTRACTS.md). See the
[source ledger](M2_PICKUP_AUDIO_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_PICKUP_AUDIO.md).

## Original rules and typed adaptation

`PickupObject::PickedUp` requests `PlaySoundFromPos` only after successful
collection. The port keeps its existing typed acknowledgement, outstanding-touch
identity, full-pool refusal, dead-player guard and consumed-before-callback rules.
The accepted pickup hides its visible/touch flags, starts the sound, records the
result, dispatches the authored pickup trigger, then requests deferred removal.
An absent/invalid WAV or exhausted voice budget does not undo collection, suppress
the trigger or keep the item alive. Refusal, forged acknowledgements and repeated
contacts produce no collection voice. Empty sound paths mean no request.

The original helper uses the pickup's position at acknowledgement, spatial sound,
outer radius **300**, inner radius **30**, full volume, priority **2**
(`SOUNDPRIORITY_MISC_HIGH`), reverb requested, no loop and no attachment. The
existing bounded decoder and SDL mixer produce dry audio; reverb and priority
arbitration are still unavailable. No replacement asset alias is invented.
The existing portability adaptation performs sound/trigger callbacks before
actual removal so their source identity remains checked. Engine destruction is
still deferred, including reentrant triggers that remove the pickup or player.

Original constructors choose `ammo1.wav`, `health1a.wav`, `health2.wav`,
`armour1.wav`, `powerup3.wav`, `weapon1.wav`, `inventory1.wav` or `ammo2.wav`, all
under `sounds/powerups`. The separately ported `KeyPickup` uses the
source constructor override `inventory1.wav` through the same helper. This adds no new pickup
classes, respawn rules or weapon acquisition behavior. Character weapon-pickup
quips are a separate pending source path.

## Ownership and inspection

A successfully started sound becomes world-owned and auto-retires on completion.
Its source token is provenance only: source movement/removal and player removal
cannot move or cut off the sound. No raw object pointer is retained. World unload
clears voices, cached clips and reports; audio IDs are not reused across worlds.
Existing attached voices, ambient loops and retained weapon voices are unchanged.

The latest 32 collection reports own name, class, optional typed pickup values or
key name, requested sound, fixed position, voice ID and explicit start failure.
A key is not represented as an ammunition grant. Reports survive pickup removal;
a retained successful-start record does not imply its voice is still active.
The Pickups inspector reads live decoded frame cursors/gains when available and
labels retired voices separately. It never advances the sound clock. Hidden UI
and inspector-free builds use the same gameplay/audio path.

## Verification boundary

Synthetic fixtures exercise accepted/refused/dead/repeated collection, key audio,
source and player removal, trigger reentrancy and ordering, completion, bounded
history, missing/malformed/empty sounds, and voice-budget exhaustion. Private
fixtures collect 21 supported classes with external retail models and decode the
eight original WAV choices. These are diagnostic grants/contacts, not an authored
walkthrough. The NVIDIA pickup scenario collects ammo, a weapon and a medkit
with hidden UI and verifies decoded voices and SDL sample submission after their
source objects have been removed. No human listening or native Metal validation
is inferred from successful sample submission.
