# Original player VoiceMgr and SOUND commands

This connects the selected original VoiceMgr, weapon-pickup reply helpers and
player SOUND branch. M2 remains incomplete. See the [source ledger](../implementation/sources/m2_player_voice_sources.json)
and [execution evidence](../../../Documentation/Implementation/m2_player_voices.md).
The [audio stream](audio_stream_contracts.md), [conversation](conversation_contracts.md),
[player](player_contracts.md) and [level-change](level_change_contracts.md) contracts still apply.

## Original selection and timing

Four base-game characters have twelve event groups: idle, big gib, pain, kill,
death, burning, powerup, spawn, suicide, weapon, taunt and jump. Enumerate direct
WAV files, case-insensitively, accepting at most 64 paths shorter than 128 bytes.
A nonempty accepted custom group at the archive root (`caleb/weapon`, for example)
replaces the corresponding `sounds/chosen/...` group completely. Empty custom
groups fall back; nested directories are not searched. The resource index supplies
canonical exact paths and deterministic sorted enumeration. This replaces the
legacy GetFileList ordering, whose retail order is not established.

One-file groups consume no random draw and do not change their last index.
Multi-file groups retry the previous index at most 200 times. Selection changes
that index even when subsequent playback fails. The portable client generator is
independent of server gameplay randomness; it replaces the old client CRT rand
seeded from client time. Random sequences are not claimed retail-bit-identical.

Gates run in original order: enabled, previous retained voice completion, external
camera, then **time strictly greater than the deadline**. A failed/stale audio
handle is an explicit failure. Only Jump requests buffered playback; other events
and all nine unique Caleb replies request actual bounded PCM streaming. Compressed
streaming remains unsupported; there is no silent buffered fallback. Local voices
request PLAYER_MEDIUM priority (5), no reverb and no loop. Success sets the deadline
to now + duration + 0.5 seconds; unavailable duration uses the original one-second
fallback. StopAll resets the deadline and releases only the VoiceMgr handle.
Replacing a sound stops the previous one before attempting the replacement,
including a missing-file failure. Playback failure does not set a unique flag.
A previously played unique is refused before the ordinary gates; non-Caleb unique
requests are ordinary source refusals, not successful audio or service failures.

## Connected original callers

After a successful ObtainWeapon, the existing 15% server draw now calls the
original weapon voice helper. Its mapped unique reply is queued first; an independent
20% draw may then queue the Weapon group. This restores a previously missing second
server random draw, so later seeded gameplay draws can differ from earlier builds.
Ammo-only collection and refused weapons do not enter this branch. Delivery and
playback are counted separately; a unique can make the following group busy.
The source mapping does not request the Akimbo unique from this helper.

Player SOUND parsing consumes literal spaces only, compares the first token without
case, and preserves the complete suffix after one delimiter. Single-player commands
with a suffix queue VoiceMgr StopAll, then start the original buffered local
PLAYER_HIGH (6) one-shot. These world-owned tails are unretained and independent of
VoiceMgr; a subsequent StopAll must not kill them. Missing suffixes and multiplayer
commands do nothing. The source has no dead-player guard for this dispatch.
No HSTRING, object pointer or borrowed input buffer survives queued delivery.
Music and multiplayer flag commands remain explicitly unsupported.

The conversation message stops VoiceMgr before CCommLink's busy refusal. The
actual delayed conversation cue stops it again and, on successful playback,
sets the voice deadline to the cue duration plus 0.5 seconds (three-second duration
fallback). This connects the former request-only inspection fields without
stopping footsteps, ambient sounds or other unrelated voices. Updates run outside
inspection and drawing. Other original VoiceMgr callers (including spontaneous
idle/death/jump events) are not wired by this increment merely because their
groups are discoverable and the typed event supports them.

## Lifetime and inspection

Fresh entry resets unique flags; world exit stops the retained voice. Matching
resource groups retain their last selection across entry. Successful NEW_LEVEL
copies only unique flags, enabled state, client generator and matching-group last
indices after all fallible preparation. Sound handles and deadlines stay with the
old world. Rejected transitions preserve source voice, history and identity.
This is in-memory client continuity, not serialized saves or whole-world restore.

The Session inspector shows all 48 groups, empty/custom groups, rejected paths,
last indices, actual retained audio status, unique flags, requests/starts/refusals/
failures and deadlines. Player SOUND resources/errors and queued pickup replies
are separate. Cinematics reports connected stop/delay arbitration while retaining
the original communication-HUD limitation. Reading the UI never advances audio.

The engine retains its existing dry mixer and explicit unsupported priority
arbitration/reverb behavior. Group support does not certify all required voices,
all original callers, addon characters, interactive IMA music or the M2 exit gate.

The [combat voice continuation](combat_voice_contracts.md) connects original pain/burning and NPC/corpse kill/gib callers. The remaining caller omissions above still apply.

The [single-player death continuation](player_death_contracts.md) supersedes the missing selected Caleb death/corpse/gib and client death-camera paths above. Active powers, multiplayer death and whole-world/dead-player restoration remain unavailable. M2 is still incomplete.
