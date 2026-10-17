# Original KeyFramer audio

Sound keys now play decoded attached WAV voices through the existing typed mixer.
This supersedes the request-only boundary in [keyframer contracts](keyframer_contracts.md).
See the [source ledger](../implementation/sources/m2_keyframe_audio_sources.json)
and [validation report](../../../Documentation/Implementation/m2_keyframe_audio.md).

## Source behavior

`ProcessKey` processes sound before named messages and print requests. Each live
controlled target kills the previously retained sound, then attempts the new one.
The original controller has **one handle for the whole target list**: only the
last successful final-target request survives, not one sound per controlled object.
A failed final request leaves silence. Existing target enumeration order is kept;
this does not claim retail object-list order. Missing targets make no attempt.

Empty sound names do not set SOUND_KEY and do not stop an existing voice. OFF,
Pause, nonlooping path completion and source Resume scheduling rules do not kill
it either. Reactivating a path replaces it only when a sound-bearing key is crossed.
Key-loop overshoot discard and the source's missing final motion snap are unchanged.
No extra event or simulation/audio advance is inserted into production code.

Preserve attachment to the controlled target (not the controller), authored outer
radius, inner radius `R * 0.1`, full volume, MISC_MEDIUM priority 1, requested reverb,
and authored LoopSound. Both loops and one-shots retain their handles; a completed
one-shot remains queryable until replacement, owner removal or controller destruction.
Original GETHANDLE/TIME needs use the existing checked ID/duration services;
no historical binary audio ABI or new backend is introduced.

## Lifetime, failure and inspection

The controller destructor kills its retained handle. The mixer owns the voice
against the controlled object and sweeps it if that object disappears. Link removal
still erases the complete target/offset record; a stale sound ID is safe to kill
and cannot alias later playback. Normal world teardown destroys original objects
before clearing the mixer/cache. Sound failure does not cancel messages or movement.

`last_sound` records the most recent sound-bearing key, even if no target exists.
`sound_target`, radius, loop and error describe the most recent actual target
attempt. Counters distinguish attempts, starts and failures. `sound_voice` is the
single retained ID, which may be stale following target removal; the inspector
resolves it before showing decoded frames, completion, cursor, loops and position.
A finished retained one-shot is not labelled missing. Inspection never advances audio.
The mixed attachment pose is sampled at the existing audio stage before that step's
object movement, so it can precede the current rendered pose by one session step.

Missing WAVs, invalid radii and voice/cache budgets remain explicit failures.
No basename search or fabricated sound is added for missing authored references.
Playback remains dry: requested reverb and priority are metadata, with no acoustic
occlusion or priority arbitration. Print presentation, full save/restore and the
remaining M2 acceptance gate are separate work.

## Checks and scope

`keyframe_audio.timeline`, `replacement`, `lifetime` and `failures` cover retained
one-shots/loops, empty keys, OFF/Resume/restart, multiple targets, failure after a
successful voice, no targets, real mixed PCM, hidden owners, followed transforms,
owner/controller removal, sound-before-reentrant OFF/REMOVE, stale IDs, unload
cleanup, invalid radius and the unchanged 128-voice limit.

The private catalog has 16 sound-bearing keys: 13 decode, while two basename-only
`pickuptruckloop1.wav` references in Abandoned Buildings and one
`sounds/enemies/civilian/scream2.wav` reference in Slaughter House remain unresolved.
The existing malformed `16_bridge.dat` rejection is unchanged. The private runtime
check loads Train Station twice and activates its actual `uhoh` controller through
ON. Its unmodified `tr` keys wait six seconds, then play the authored train-pass
loop on the actual controlled `train` Damage volume (radius 6000, 110592 decoded frames).
Its moving-container path remains explicitly unavailable; this proves audio, not
Train Station motion or level playability.
This direct diagnostic command is not a player traversal of that level.

`gpu.keyframe_audio` uses the same full loaded world and authored delay in the app,
checks attached advancing audio and SDL submission with the inspector hidden,
and repeats after unload/reload. Existing SubwayA camera/path assertions remain
separate and unchanged. NVIDIA/dummy-audio checks are not human listening or native
Metal certification. The implementation evidence records exact results and limits.
