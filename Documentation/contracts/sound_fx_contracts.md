# Original SoundFX controllers

> The later [PCM streaming contracts](audio_stream_contracts.md) supersede blanket FileStream and VoiceType=0 omissions below for Sound/SoundFX only. Compressed streams, nonzero historical metadata, TypeAction and M2 remain open.

This is the original ObjectDLL `SoundFX`, distinct from built-in `Sound` and
`RandomSoundFX`. Read [audio ownership](world_sound_contracts.md),
[object lifetime](object_contracts.md), [source ledger](../implementation/sources/m2_sound_fx_sources.json)
and [implementation evidence](../implementation/reports/m2_sound_fx.md).
M2 remains incomplete; registration never implies successful authored playback.

## Original behavior

World/string creation reads StartOn, RampUpSound, RampDownSound, Sound, Priority,
OuterRadius, InnerRadius, Volume, Ambient and FileStream. Defaults match the
original registration; runtime creation retains the constructor's zero radii,
zero volume and non-ambient mode. Restore creation reads nothing; the original
Save/Load block is ported, keeping the saved next update ([save/load lot 10](save_load_contracts.md#implemented-effect-publishers-and-audio-emitters-lot-10)).
Priority/volume cast to bytes; volume below 100 controls gain, otherwise full gain.

InitialUpdate marks the object saveable, immediately enters ON when StartOn, and
then **disables updates**, including any duration scheduled by that first playback.
Startup bypasses the ramp-up sound. The base initial callback and difficulty
filter run afterwards, as in the source. A filtered object releases its newly
started voice during deferred destruction. A restore skips this initial update.

Trigger commands match whole strings case-insensitively: ON/TRIGGER, OFF and
TOGGLE. Unknown/empty/null commands do nothing. Each recognized command calls
UpdateSound, which kills the previous retained voice before selecting a new one.
ON while already ON therefore restarts playback. ON during ramp-down restarts
ramp-down; it does not reverse it. TOGGLE treats every non-OFF state as on.

Ramp-up/down use their respective one-shots when present; missing paths select
ON/OFF immediately. Only **ambient ON loops**. Spatial ON plays once and schedules
ramp-down from the decoded sample duration. Scheduled Update first disables its
deadline, then advances ramp-up -> ON, ramp-down -> OFF or ON -> ramp-down. No
invented minimum delay or retry is added. A missing resource can leave a ramp
state without a deadline, matching the original failed-start path.

UpdateSound itself does not cancel an old deadline. OFF without a ramp-down
retains an already scheduled callback; that callback later observes OFF and
cancels itself. Failed replacement playback similarly leaves the old deadline.
The original boolean precedence attempts a duration query for failed spatial ON
as well; a missing voice yields no duration and does not schedule success.

## Typed audio and lifetime

Existing PlaySoundAt provides actual bounded WAV decoding, retained checked voice
IDs, fixed sampled position, ambient distance attenuation or spatial panning and
whole-clip looping. The emitter owns its voice; unlike built-in Sound it stays
alive, and its removal stops playback. Moving the object does not attach or move
an already sampled sound. Inactivity stops scheduled object callbacks but the
independent audio clock continues. World unload destroys original objects while
the adapter is bound and clears voices/caches. Reload uses new IDs. No mutable
game global or dependency is added; destination rehearsal needs no new global
exchange.

FileStream/TIMESYNC fails explicitly before playback. Invalid radii, missing or
malformed WAVs, region-loop metadata and voice budgets retain existing mixer
failures. Spatial reverb intent is retained but output is dry; priority is
metadata without arbitration. CacheFiles is omitted: mounted resources are
indexed and decoded on demand, not claimed precached. Streaming remains
unavailable. Historical VoiceType schemas remain rejected, including all
five CityHub SoundFX entries; their FileStream=true request is an additional
unimplemented service even if the schema were resolved. Do not discard that
property or pretend full-clip loading implements streaming.

Strings are owned values rather than retained HSTRING fields. Message reads are
released once, and the source destructor's duplicate RampDownSound FreeString is
removed. Null trigger text is a checked no-op rather than a null dereference.
These are memory-safety adaptations, not gameplay replacements.

## Inspection and tests

Objects displays state, startup/ambient/streaming flags, all three resources,
radii/volume/priority, command/start/failure counters, current retained voice and
its real cursor/loop/fixed position, plus the last playback failure. Generic
activity/scheduling/properties and hidden/inactive entries remain available.
World retains rejected source schemas and references; indexed is not decoded.

Behavioral tests cover startup cancellation, byte casts, exact command matching,
ramp interruptions, ambient looping, spatial one-shot completion, sampled position,
stale deadlines, missing/streaming failures, filtering order, inactive clocks,
deferred destruction and repeated unload. The private catalog enumerates every
SoundFX and attempts only its unmodified authored schema; resource decoding is
reported separately from creation/playback. The existing GPU Sound scenario adds
a labelled SoundFX probe using an actual SubwayA WAV, verifies PCM submission,
ON/OFF removal while UI is hidden and after reload. This is not a CityHub emitter
success or a listening assessment. Safe NEW_LEVEL and the actual M2 gate remain open.
