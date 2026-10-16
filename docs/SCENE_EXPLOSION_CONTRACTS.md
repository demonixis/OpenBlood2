# Original scene explosion emitters

M2 remains incomplete. This extends [object contracts](OBJECT_CONTRACTS.md),
[brush explosion contracts](EXPLOSION_CONTRACTS.md) and [audio contracts](CINEMATIC_CONTRACTS.md).
Read the [source ledger](M2_SCENE_EXPLOSION_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_SCENE_EXPLOSIONS.md).

## Original behavior

CClientExplosionSFX retains CClientSFX -> B2BaseClass ancestry and callback order.
All precreate reasons read ExplosionType, Damage, DamageRadius, Direction and
WaveFile. World/string/runtime properties are supplied by the typed registry;
restore rejects. Signed ExplosionType converts to the original uint32 value.
Direction remains unnormalized on the server. Damage/radius are finite and bounded
in absolute value by 1,000,000; signed and zero values are retained. Initial update
stores the initial position, sets SAVEABLE and schedules 0.1 seconds. Each update
rearms 0.1 and removes a triggered emitter through deferred destruction.
CClientSFX preserves FORCECLIENTUPDATE; original difficulty filtering runs last.
SAVEABLE does not mean save/restore is implemented.

MID_TRIGGER accepts case-insensitive whole-token TRIGGER only. It does not check
the triggered bit before firing, so repeated messages before the next scheduled
update repeat the effects and sound. Triggering does not restart that timer.
Inactive objects still accept direct messages, but their update/removal waits for
activation. The cached initial position stays the explosion origin even if a
controller moves the emitter. Only the authored-world SetupFX path is imported;
programmatic SetupModel/Sprite/Ring/Wave/Light/Frag and Setup overloads are absent.
Strings and reports own their values, without legacy borrowed HSTRING storage.

**The supplied original code passes NULL as the damage sender.** Its shared
DamageObject immediately returns for a null sender. Thus these emitters deliver
**zero damage**, even when Damage is nonzero and targets are inside DamageRadius.
The call retains the header's default Normal damage type (no message reaches a target).
The port preserves this rule, including squared-distance candidate/in-radius
accounting, without inventing a non-null sender. The shared radius helper now
accepts this original caller, reports null_damager separately from cancellation,
and constructs no damage messages. Existing non-null brush explosions still
apply the original falloff. The recorded counts distinguish candidates, in-radius
targets, actual deliveries and cancellation. Nonpositive radii produce no traversal;
malformed inputs reject, including underflow of a positive squared radius.
This source observation has not been compared to the retail executable.

## Sound lifetime and missing client FX

The source PlaySoundFromPos request becomes a typed PlayDetachedSound call:
fixed cached position, spatial, one shot, volume 100%, inner radius **100**, outer
radius **1000**, priority **2** (MISC_HIGH) and reverb requested. WAV decoding,
sample-count clocks and dry SDL output use the existing bounded mixer. Reverb and
priority arbitration remain unavailable. Empty WaveFile means no sound; a missing
or undecodable sound records an explicit failure while original triggering and
scheduled removal still proceed. The default Sounds\exp_tnt.wav is actually
missing from this installation; no alias is guessed.

The new service requires a live source, no attachment, nonlooping spatial request,
valid positions/radii and the existing voice/clip budgets. A started voice belongs
to the world: emitter movement/removal does not relocate or kill it. Its owner token
is provenance only and is never dereferenced. Completion automatically frees the
voice, allowing later events to reuse the 128-voice capacity. Clips remain cached
within existing budgets until unload. Existing source-owned cinematic/random
voices and persistent world loops keep their distinct lifetime contracts. Unload
clears all voices, caches and event records; stale IDs cannot address a new voice.

The original client explosion packet is **not implemented**. Up to 32 owned
SceneExplosionReports retain type, initial position/direction, configured damage,
actual radius-delivery counters, sound ID and playback failure. They are explicitly
unavailable client-FX requests, not successful service calls, simulated explosions,
particles, dynamic flashes or rendered effects. A loaded emitter is a functional
server/audio subset, not a completed explosion system. Required client FX keep M2
open; no readiness gate is weakened by registering this class.

## Inspection and verification

The Explosions tab lists live hidden/inactive emitters and their scheduling,
properties and commands, plus durable source dispositions and activation reports
following removal. TRIGGER is queued at a frame boundary. Audio clocks and emitter
updates run with the inspector hidden and in the inspector-free build. Decoded
voices, frame submission and unavailable FX are labeled separately.

Synthetic checks cover world/string/runtime properties, inherited flags/filtering,
whole-token commands, repeated activation, cached position, exact removal timing,
null-sender damage suppression and unchanged non-null damage, inactive/stale lifetime,
missing sound, invalid properties, voice budgets/completion and unload.
Private checks scan 57 readable worlds: 43 emitters, all accepted; 19 authored WAV
references decode and 24 use the absent default. The six authored type IDs are
1 (3), 2 (10), 3 (19), 6 (7), 8 (3) and 24 (1). 16_bridge retains its strict DAT
reader rejection. Two SubwayA/player cycles verify all nine emitters, independent
voices, removal and cleanup. The GPU scenario checks original activation/removal
and dry SDL sample submission while hidden and after reload.

SubwayA: 187 live-created source entries, 4 transferred world sounds, 372 editor-only,
17 unported, zero failed; 7 SelfTriggers give 194/197 runtime instances before/after
player entry, then 185/188 after nine Keys are consumed. Remaining sources are
5 CivilianAI, 10 CultistAI, 1 Gideon and 1 ExitTrigger, in addition to required missing
services. No retail parity, other-platform or XR claim is made.

## Hot-query allocation adjustment

The larger active scene exposed the existing 90-second weapons.private sanitizer
limit. Container membership/physics queries now gather only object IDs with a bound
container shape, rather than copying and looking up every unrelated object. This
is a fresh per-call snapshot in the same slot order, with the same generation and
life checks; there is no cached membership or new container index. New/removing
volumes, four-body limits, hidden/head precedence and callback timing are unchanged.
The general object snapshot still includes all objects. Existing membership,
mutation/lifetime and full movement/weapon scenarios validate this optimization;
the full sanitizer suite must pass without increasing the timeout.
