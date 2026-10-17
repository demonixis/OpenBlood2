# Original random sound emitters

The later [world sound contracts](world_sound_contracts.md) add the selected built-in
Sound whole-clip loops and consumed-source accounting. Earlier blanket loop/world
ownership omissions below are historical; streaming and the M2 gate remain open.


M2 remains incomplete. Read [cinematic audio](cinematic_contracts.md),
[object lifetime](object_contracts.md), the [source ledger](../implementation/sources/m2_random_sound_sources.json)
and [implementation evidence](../../../Documentation/Implementation/m2_random_sounds.md).

## Original rules

`RandomSoundFX` selectively ports the original constructor, destructor, property
reading, initial/update and trigger callbacks. The B2BaseClass callback chain and
filters remain intact. World/string defaults select positional sound, whereas the
original runtime constructor selects ambient sound. InitiallyOn schedules a random
MinWaitTime..MaxWaitTime interval. Zero disables the schedule, including an on
controller with zero wait; it is not replaced with a minimum interval.

The playable list stops at its first empty SoundFile1..10 property. Later authored
slots remain inspectable but are ignored by the source rule. Volume casts to the
original unsigned byte before clamping to 100. The registered Ambient property is
unused in the original; PositionalSound alone selects spatial versus ambient output.
Position randomness independently offsets X and Z in +/-PosRangeRadius, a square
rather than a circular distribution; Y is unchanged. Playback stays at that sampled
position even if the emitter subsequently moves.

Each update first kills the previous voice, including one still playing. Random
mode chooses an inclusive integer index; sequential mode increments and either
wraps or turns off after its last slot. A failed sound still advances the sequence
and schedules the next interval. Delay is independent of clip duration. An empty
list removes the controller on its first update, rather than rejecting creation.
The ten-slot sequence is separate from audio looping; it does not loop a WAV.

Trigger comparisons are case-sensitive prefixes: TOGGLE, ON and OFF. Every trigger,
including unknown text, resets the sequential index when on and sets the next update
to 0.1 seconds when on, or disables it when off. OFF does **not** kill the playing
voice. Its completed handle remains owned until replacement or destruction. The
source's duplicate FreeString is corrected: each owned message read releases once.
Resource strings use values, and voice IDs use the existing checked monotonic type.

Invalid/nonfinite/negative ranges, reversed waits, radius <= 0 or values above
1,000,000 reject precreate; restore creation also rejects. These are explicit bounded
adaptations. Save/load and original cache callbacks remain unavailable. `RandomInt`
uses the existing session-seeded generator with an inclusive uniform distribution;
resetting the world restores its seed. It does not claim the retail random sequence
or cross-standard-library bit identity. Sharing the generator with light phase
selection means additional original random calls can change later phases.

## Typed fixed-position audio

`SoundSpace` makes Local, Ambient and Spatial mutually exclusive. The existing
cinematic local/attached behavior remains covered by its regression tests.
`PlaySoundAt` requires a live owner, mounted resources, an ambient/spatial request
without a speaker attachment, a finite bounded position and valid audio parameters.
It performs real bounded resource decoding and returns no voice on failure. Ambient
uses distance attenuation without orientation/panning; spatial adds the existing
replacement constant-power panning. These are dry mixer policies, not recovered
retail audio. Reverb intent is retained for positional requests but not rendered.
Doppler, occlusion, priority arbitration and full legacy PlaySound flags are absent.
The source low-priority intent is not an implemented allocation policy.

Fixed voices do not use a speaker handle. Live owner validation, deferred destruction,
explicit replacement and unload release their voices. The same 128-voice, decoded
cache, WAV preflight, sample-count clock, bounded PCM output and SDL stream contracts
apply. Completed voices remain queryable until their original owner releases them;
no stale ID resolves after reload. Sampling and output are independent of ImGui.

The built-in `Sound` class is still unsupported. Its source in
`sources/blood2/AppHeaders/engineobjects.c` starts a loop, optionally requests streaming,
and immediately removes its own object. That requires world-owned playback lifetime,
not the object-owned fixed voices added here. Do not register it using a short-lived
RandomSoundFX or claim its four SubwayA entries are implemented.

## Inspection and verification

The Random sounds tab includes hidden/inactive/pending emitters, distinct object IDs
for duplicate authored names, schedules, random/sequential state, commands, attempted
and failed starts, all ten authored slots (including ignored gaps), resource presence,
actual decoded voice format/cursor/duration, fixed position, gains and output errors.
ON/OFF/TOGGLE are queued checked original messages. Presence and attempt counters
are not successful decoding or audible playback evidence. World retains unported
classes and rejected entries. The inspector does not supply game rules.

Tests cover constructor/world defaults, byte wrapping, first gaps, zero intervals,
prefix/case handling and OFF tails, sequential replacement/wrap/stop, seeded random
bounds, ambient orientation independence, fixed versus attached positions, failed
starts, pending-owner removal, stale IDs and unload. Private tests inspect 57 readable
worlds and exercise both actual SubwayA emitters across two full shell/player reloads.
The GPU test starts an authored emitter through ON and verifies PCM submission to
SDL's dummy driver while inspection is hidden and after unload/reload, including
an inspector-free build. This does not validate audible hardware quality, retail
executable parity, other platforms, world rendering or the complete M2 gate.
