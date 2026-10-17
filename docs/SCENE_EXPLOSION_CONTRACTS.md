# Original scene explosion emitters

The [bound-container index](CONTAINER_INDEX_CONTRACTS.md) now avoids full object-slot scans while preserving fresh owned snapshots, slot order and deferred lifetime.

Continuation: [explosion point lights](EXPLOSION_LIGHT_CONTRACTS.md) now evaluate
selected original transient colors/radii with independent lifetime. Earlier light
omissions are superseded only for that state; surface illumination stays unavailable.

Continuation: [original explosion models](EXPLOSION_MODEL_CONTRACTS.md) now supply
selected model components with independent lifetime and alpha rendering. The earlier
model omission is superseded only for that subset; complete composite FX remain unavailable.

Continuation: [original explosion sprites](EXPLOSION_SPRITE_CONTRACTS.md). Selected original fire/smoke components now render and outlive their emitter. The complete-composite flag remains false; particle/fragment/illumination services remain unavailable. Original null-damager damage and audio behavior stay unchanged.

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

## Sound lifetime and partial client FX

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

Typed delivery now routes selected high-detail sprite branches to original
CExplosionSpriteFX methods; see [sprite contracts](EXPLOSION_SPRITE_CONTRACTS.md).
The original binary client packet/complete manager is not recreated. Up to 32 owned
SceneExplosionReports retain type, initial position/direction, configured damage,
actual radius-delivery counters, sound ID/failure and separate client model, sprite
and light event IDs/failures.
Sprite success does not set complete-composite availability. Selected model components now render; particles, fragments and dynamic
illumination remain absent. A loaded emitter is still a
partial system; required missing services keep M2 open.

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

Current SubwayA source accounting is 203 created, four transferred world sounds,
372 editor-only, one unsupported Gideon and zero failed. Client sprites are owned
visuals, not additional world-source entries. Full composite effects and safe
NEW_LEVEL remain unavailable. No retail parity, other-platform or XR claim is made.

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

Continuation: [original metal fragments](EXPLOSION_FRAGMENT_CONTRACTS.md) now supplies
selected grenade fragment motion/collision and rendering. Earlier blanket fragment
omissions are historical; smoke trails, other required services and M2 remain open.

Continuation: [original explosion flame particles](EXPLOSION_FLAME_CONTRACTS.md)
now implements the selected Tesla component. Earlier blanket flame omissions are
historical for that recipe; surface illumination and the actual M2 gate remain open.
