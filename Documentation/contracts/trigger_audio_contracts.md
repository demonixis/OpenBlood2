# Original trigger audio

`Trigger`, `BigTrigger`, `ToggleTrigger` and door-owned SelfTriggers now play
original activation and lock feedback through the existing attached WAV service.
This extends [trigger contracts](trigger_contracts.md) and follows the
[door-audio continuation](door_audio_contracts.md). See the
[source ledger](../implementation/sources/m2_trigger_audio_sources.json) and
[execution evidence](../implementation/reports/m2_trigger_audio.md).

## Original decisions

Activation plays only after sender validation and the activation count threshold,
before target-message delivery. It attaches to the trigger, uses authored
`SoundRadius`, and requests MISC_MEDIUM priority 1. A locked player request plays
the denial cue only when PlayerActivate allows it and the synchronous key query
has not unlocked the trigger. It attaches to the denied player, with outer radius
200 and MISC_HIGH priority 2. Explicit UNLOCK plays only when previously locked;
it attaches to the command sender, which need not be a player, using the same
200 radius and priority 2.

A successful key response does **not** play UnlockedSound: that branch is commented
out in the original Trigger.cpp. The subsequent activation may play ActivationSound.
LOCK itself is silent. Inactive triggers ignore commands as before; counter,
named-filter, relay, delayed-message and reentrancy ordering is unchanged.

ObjectUtilities::PlaySoundFromObject supplies inner radius R*0.1, volume 100%,
requested reverb, attached nonstreaming one-shots, no loop and no retained handle.
Repeated requests can overlap and never kill an earlier voice. The existing
PlayCharacterSound service accepts any checked object, so no new audio API or
backend is introduced.

## Ownership, failures and inspection

The speaker is also the mixer's owner. Removing the trigger ends an activation
voice at the next audio ownership sweep, but a lock/unlock voice attached to a
still-live sender survives trigger removal. Removing that sender ends its voice.
Completion auto-releases every one-shot. World teardown clears all voices/cache;
checked sound IDs are not reused. Audio sampling occurs at the existing pre-motion
stage and does not depend on the inspector.

Empty names make no playback request. Missing WAVs, invalid radii, absent senders
and exhausted limits produce no placeholder voice. The original activation or
unlock still proceeds; inspection retains starts, failures and the last error.
In a Chosen game (`GAMETYPE_ACTION`, `B2BaseClass::GetGameType`), the original
replaces the locked and unlocked sounds with `FormatString(IDS_GENERAL_LOCKED)` (268)
and `IDS_GENERAL_UNLOCKED` (269) sent to `g_pPlayerObj`'s console (Trigger.cpp:802,
921). The port reads them from `cres.dll` (`GetClientResourceString`) and sends them
to the checked local player (`CPlayerObj::SendConsoleMessage`, a `PlayerText` line in
the client's message lines). Only an authored sound triggers the line, no sound plays,
and without a local player nothing happens, like the original's null-player test. A
missing string or a refused delivery leaves `sound_error`; `console_feedback` counts
delivered lines. Generic multiplayer behavior is not certified.

The Triggers inspector displays all three configured paths, request/start/failure
counts, the last speaker/radius/priority and the resolved decoded voice/cursor.
The last voice and speaker are non-owning history, checked when inspected; an
absent voice can mean completion, deletion or a failed request. Empty new cues
clear the last request/error without erasing cumulative failures. Dry playback
still does not implement requested reverb, priority arbitration or occlusion.
Music and the full M2 acceptance gate remain open.

## Verification scope

Synthetic behavioral checks exercise activation thresholds, touch/message paths,
all three trigger classes, overlap, following transforms, actual mixed PCM,
player denial, sender-based UNLOCK, silent key responses, inactive commands,
missing resources, zero radius, absent sender, Action mode without a player, hidden updates,
owner deletion, stale identities and unload. Private checks use three CDM sensors (including a door-owned SelfTrigger) over
two world lifetimes through explicit diagnostic lock/denial commands, not a level
walkthrough. The NVIDIA train trigger smoke borrows a CDM denial cue explicitly,
just as it already borrows CDM key assets; SubwayA has no authored lock cue. This
test-only assignment is not a runtime fallback. The smoke checks decoded audio
cursor and SDL output with inspection hidden while retaining all previous
key-pickup/unlock/door-motion assertions.
