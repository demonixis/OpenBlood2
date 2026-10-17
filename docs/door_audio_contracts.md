# Original door audio

`Door::StartSound` now uses the existing decoded WAV mixer for sliding and
rotating doors. This supersedes the request-only audio boundary in
[door contracts](door_contracts.md) and [interaction contracts](interaction_contracts.md).
The [source ledger](m2_door_audio_sources.json) identifies the original branches;
[validation](../../Documentation/Implementation/m2_door_audio.md) records actual checks.

## Rules and ownership

Every call first kills and clears the previously retained busy loop, including
when the new name is empty or playback fails. An empty name makes no new request.
Busy sounds loop; stop sounds are attached one-shots without a retained handle.
Consequently a later busy/stop request does not interrupt an earlier stop sound.
Original open delay, endpoint, reversal and start-open rules decide when to call
StartSound. No extra timing, motion, trigger or damage rule is introduced.

Preserve spatial attachment to the door, volume 100%, priority
`SOUNDPRIORITY_MISC_MEDIUM` (1), requested reverb and fixed inner/outer radii
500/1500. Original StartSound does not use the authored `SoundRadius` property.
`PlayObjectLoopSound` handles retained loops; the existing `PlayCharacterSound`
boundary accepts any checked attached owner and provides unretained one-shots.
No new generic audio API or dependency is needed.

Attached positions are sampled at the existing session audio stage, before that
step's object motion. Inspection therefore shows the last mixed pose, which may
precede the current render pose by one session step; no extra audio advance is
inserted to make them equal.

The door destructor kills its retained loop before its owned trigger is removed.
One-shots auto-release on sample completion; the existing owner sweep also releases
them after owner removal. Audio follows current door transforms and runs with the
inspector hidden. Stale IDs cannot kill later voices. Missing/invalid WAVs and
voice-budget exhaustion produce no placeholder voice, increment the failure
counter and preserve original motion/state progression. World unload clears the
mixer and decoded cache after original object destruction.

`loop_voice` is the only caller-owned sound handle. `last_voice` is non-owning
history for inspection and may identify an already completed/removed sound.
The inspector resolves it each frame and shows decoded frame count, cursor,
loop count and position, plus attempts/starts/failures and the latest error.
An empty command clears the latest request/error without erasing failure totals.
Reverb and priority remain requested metadata: playback is dry, without priority
arbitration, occlusion or retail acoustic parity. Lock/unlock Trigger audio,
portals, music and full save support remain separate limitations; M2 stays open.

## Checks

`door_audio.states` runs original sliding/rotating state transitions with synthetic
WAVs and checks actual mixed PCM, looping, attachment, fixed radii and completion.
`replacement` checks empty/missing replacement, surviving unretained stop sounds
and continued movement after a failed start. `lifetime` checks hidden playback,
destruction, stale IDs, attached one-shot owner removal and unload/cache cleanup.
`private` exercises authored SubwayA door sounds over two world lifetimes; direct
TRIGGER injection is labelled diagnostic, not a player walkthrough.

`gpu.doors` requires decoded, advancing attached sound and SDL audio submission
while inspection is hidden, in addition to original motion and rendering checks.
The continuous train tests exercise actual USE/contact paths separately. NVIDIA
and dummy-audio validation does not establish human listening or native Metal
execution of this change.
