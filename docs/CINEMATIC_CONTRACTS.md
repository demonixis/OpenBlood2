# Original cinematic cues and bounded audio

> The later [PCM streaming contracts](AUDIO_STREAM_CONTRACTS.md) supersede blanket FileStream and VoiceType=0 omissions below for Sound/SoundFX only. Compressed streams, nonzero historical metadata, TypeAction and M2 remain open.

> Extended by [character audio contracts](CHARACTER_AUDIO_CONTRACTS.md): original
> voice helpers, player sound keys and material/liquid footsteps now execute.
> Earlier blanket character-audio omissions are historical; AI hearing and M2 remain open.

The later [world sound contracts](WORLD_SOUND_CONTRACTS.md) add the selected built-in
Sound whole-clip loops and consumed-source accounting. Earlier blanket loop/world
ownership omissions below are historical; streaming and the M2 gate remain open.


M2 remains incomplete. The later [random sound contracts](RANDOM_SOUND_CONTRACTS.md)
extend audio with original RandomSoundFX, fixed-position voices and ambient distance-only
output. Earlier general-emitter omissions below are historical for this bounded subset.
 Read [camera](CAMERA_CONTRACTS.md), [trigger](TRIGGER_CONTRACTS.md)
and [object](OBJECT_CONTRACTS.md) contracts with the [source ledger](M2_CINEMATIC_SOURCES.json)
and [implementation evidence](../../Documentation/Implementation/M2_CINEMATICS.md).

## Original sequence

CinematicTrigger retains ten Delay/Sound/WhoPlaysSound/StartSoundTriggerTarget/Msg
properties, TypeActionMode=false, inherited B2BaseClass filters and callback chain.
It starts unscheduled. Every MID_TRIGGER payload rearms Delay1 + 0.0001; ON/OFF are
not new commands and neither index nor active voice resets. Each cue sends its
paired target/message before playing. The next cue delay starts only after the
active sound reports completion. An empty sound ends and removes the controller;
a missing/failed sound retries the same cue (including its message) at the source
0.001 update interval. No fabricated duration or successful placeholder advances it.

The case-insensitive speaker name `player` selects local playback. Empty or missing
named speakers fall back to the controller; named lookup uses the first object.
Attached voices follow the speaker on subsequent ticks. Removing the speaker marks
the voice done; removing the owner releases it. This speaker-removal policy is new
engine behavior, not verified retail parity. Named lists always release, fixing the
source's early-return leak. After a synchronous cue message, pending owners cannot
start audio. Destruction remains deferred. Values own strings; stale monotonic
64-bit voice IDs never resolve after removal/reload or in a later mixer instance.
Restore and save/load are unavailable. Invalid delays (nonfinite, negative or over
1,000,000 seconds) reject precreate. These are deliberate validation adaptations.

## Resource and output service

PlayCinematicSound requires a live owner, mounted resources and a live attached
speaker. It decodes real resource bytes; failures expose an error and allocate no
voice. The selected typed API is not general legacy PlaySound compatibility.
There are at most 128 owned voices and 128 cached clips / 256 MiB decoded cache.
Each encoded WAV is at most 16 MiB, decoded float samples at most 64 MiB, with
1–2 channels, 8–96 kHz and at most 1,024 chunks. Supported encodings are PCM8/16,
MS ADPCM and IMA ADPCM with complete validated blocks. Unsupported encodings,
duplicate/missing format or data chunks, invalid layouts and truncated payloads fail explicitly.
SDL's pinned WAV decoder and float conversion provide samples; engine preflight
bounds allocations. Read failures preserve the previous output value.

Observed retail PCM8 variants permit only a **final odd data** pad to be missing,
or present but excluded from RIFF's length. Every payload must still fit exactly;
other chunk padding, arbitrary trailing bytes and mismatched envelope lengths
remain errors. No commercial bytes are committed. Of 201 authored cue references
in 57 readable worlds, 198 decode (all PCM); three genuinely missing paths remain
failures. `16_bridge.dat` retains the existing strict world-reader failure.

The simulation owns voice clocks. Before original-object updates, the service
advances cursors in 48 kHz frame units with fractional-frame carry; completion
uses actual decoded sample count. New voices begin advancing next tick. Offline
runs advance the same clocks without producing PCM unless explicitly inspected.
The listener uses the preceding completed client pose: camera pose only when its
published listener flag is true; otherwise player-body position and player view
rotation. The existing retail-world suppression of camera listeners is retained.

The dry mixer uses linear resampling, linear distance attenuation between original
250/1000 radii, constant-power left/right panning and clamping. Local stereo keeps
its channels. These are replacement engine policies, not claimed retail mixing.
The source reverb request is inspectable **but not implemented**. Occlusion,
Doppler, priority arbitration, general emitters, music and other audio callers are
also pending. Do not infer their availability from CinematicTrigger playback.

Interactive application runs open SDL's default playback stream (48 kHz stereo
float); failure is visible and leaves offline clocks operating. Each update renders
at most 250 ms and records dropped frames beyond that bound. A queued backlog over
250 ms is cleared and counted. Stream errors are retained for inspection; submitted
frames mean SDL accepted PCM, not proof that a person heard it. Headless and normal
GPU tests are silent; the cinematic device/GPU tests use SDL's dummy driver.
Unload clears voices/cache/queued PCM/counters after original object destruction;
the device remains open until session destruction, before global SDL shutdown.

## Inspection and validation

The Cinematics tab includes hidden/inactive controllers, all authored cues, current
index, schedule, commands/messages/starts/failures, active voice format/duration,
cursor, speaker/position/gains, PCM plot, cache and output/drop diagnostics.
Start/message counters count attempts/requests, not successful playback or recipient
deliveries; the active voice and submitted-sample counters provide separate evidence.
Every cue shows archive presence (including missing paths), separately from decode success.
TRIGGER queues the same checked original message independently of visibility.
World retains missing and rejected entries. The inspector does not implement game
rules or make audio completion depend on rendering.

Synthetic tests cover PCM/ADPCM, truncation/layout/padding, timing and channel output,
spatial motion/rotation, exact cue order/delays/retrigger, removal during a cue message,
owner/speaker lifetime, stale handles, missing sounds, budgets and dummy output.
Private tests decode the complete cinematic catalog, reload full SubwayA/player twice
and run its authored two-cue crash sequence to completion in isolation. GPU checks
verify actual decoded samples submitted to SDL with hidden UI and after reload.
These tests do not establish audible-device quality, retail executable parity,
additional-platform support or the M2 exit gate.

The expanded source-object population exposed a tight existing ASan weapon-test
timeout. `Objects::snapshot` now reserves its slot-count capacity before gathering
IDs, avoiding repeated vector growth without changing membership/order or callback
lifetime. The unchanged scenario and 90-second limit pass after this allocation-only
correction; full regression evidence includes the original two timeout attempts.
