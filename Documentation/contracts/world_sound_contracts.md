# Original world-owned Sound loops

> The later [PCM streaming contracts](audio_stream_contracts.md) supersede blanket FileStream and VoiceType=0 omissions below for Sound/SoundFX only. Compressed streams, nonzero historical metadata, TypeAction and M2 remain open.

M2 remains incomplete. Read [object lifetime](object_contracts.md),
[world loading](world_contracts.md), [cinematic audio](cinematic_contracts.md),
[random emitters](random_sound_contracts.md), [source selections](../implementation/sources/m2_world_sound_sources.json)
and [implementation evidence](../implementation/reports/m2_world_sounds.md).

## Original rules and adaptations

The built-in `Sound` ports AppHeaders/engineobjects.c, not ObjectDLL SoundFX.
It derives directly from BaseClass and has no B2 difficulty/game-mode filters.
Precreate selects OT_NORMAL; world/string properties select Filename, Priority,
OuterRadius, InnerRadius, Volume, Ambient and FileStream. Defaults are empty
filename, priority 0, radii 100/10, volume 100, ambient true and FileStream false.
The source's missing-property FileStream fallback is true, distinct from its
registered false default. Runtime filename is safely initialized empty because the source callback does not
initialize it in that branch; allocator initialization in the retail engine has
not been established. Restore rejects explicitly.

Volume and priority cast to unsigned byte before use. Volume below 100 controls
gain, while values >=100 use full gain, as with the original flag selection.
Ambient selects distance-only output; otherwise spatial panning applies. Neither
case requests reverb. Initial update starts one fixed-position infinite loop and
unconditionally removes the source object, including after playback failure. The
BaseClass callback chain still runs, and destruction remains deferred. There are
no newly invented ON/OFF commands or source-following movement.

`PlayWorldSound` requires a live source, mounted resources, an unattached ambient
or spatial loop and valid existing sound parameters. FileStream/TIMESYNC requests
fail explicitly; decoding the complete resource is not described as streaming.
The source name/position/properties remain in the authored World entry. Priority
intent is retained in the request, but allocation arbitration is unavailable.

## Lifetime and bounded playback

A successful voice is explicitly world-owned. Its original handle is provenance,
not a live attachment: removing/reclaiming the source does not stop playback or
permit a stale handle to issue new sounds. Ordinary object-owned cinematic and
random voices retain their existing removal rules. World unload clears every
voice, decoded cache and queued PCM after original destructors. Reload produces
new monotonic IDs; the SDL output device can stay open across worlds.

The loader records successful consumed sources as `WorldSound` / “World-owned
sound”, separately from `Created` live instances. It retains the voice ID without
an invalid object selection. Missing resources, unsupported streaming, invalid
radii and unknown schemas remain `Failed`, with durable diagnostic details after
source destruction. Pending removal is never mistaken for difficulty filtering.

Whole-clip loops wrap the cursor modulo the actual decoded frame count. Linear
resampling interpolates the final frame toward the first frame, including stereo
and one-frame clips. A saturating wrap counter is inspection evidence, independent
of render visibility. Offline and output clocks advance all elapsed frames; the
existing 250 ms render bound/drop accounting applies after stalls. Looping voices
do not report completion. Existing 128-voice/clip, 256 MiB cache and per-file bounds
remain unchanged.

WAV `cue `, `smpl` or `LIST/adtl` metadata marks region looping as unavailable:
a looping request fails rather than silently replacing an authored region with
a whole-file loop. One-shot decoding keeps the earlier metadata behavior. The
selected retail catalog has only fmt/data chunks. The later-generation LithTech
cue/ltxt reader was consulted as evidence that region loops need explicit work;
its code and millisecond rounding were not imported or claimed compatible.

## Inspection and validation

World sounds lists all authored Sound entries, including failures and consumed
objects, and all actual world-owned voices. Voice selection shows resource, mode,
position, radii, volume, priority intent, decoded format, cursor, wraps and gains.
Archive presence does not imply playback. Source properties remain in World;
output state/errors and submitted/dropped frames are independent of the inspector.

Synthetic checks cover defaults/byte casts, sample seams, repeated and one-frame
loops, long stalls, ambient attenuation, original removal, stale handles, mixed
ownership, budgets, missing/streaming/metadata failures, and successful versus
failed loader accounting over reloads. Private checks catalog 57 readable worlds
and exercise the four actual SubwayA loops across two shell/player sessions.
A GPU check submits their PCM through SDL's dummy driver with hidden inspection
and after unload/reload, also in an inspector-free build.

Streaming, region loops, priority arbitration, reverb, Doppler, occlusion, general
sound callers/music, save/restore, audible hardware assessment and retail
executable parity remain open. Unknown historical VoiceType properties are not
silently discarded. M2's acceptance gate is unchanged.

Distance gain is full at or inside InnerRadius and zero at or beyond OuterRadius,
linear between. Equal radii (authored on retail ambient loops such as amb7/wind1)
are therefore a hard edge and never divide by a zero falloff; this branch order is
later LithTech CSoundInstance behavior, not a verified 1998 trace. Requests still
reject InnerRadius > OuterRadius and a non-positive OuterRadius.
