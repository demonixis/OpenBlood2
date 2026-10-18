# Original AI trigger commands and hearing

> Extended by [weapon impact contracts](weapon_impact_contracts.md): original multi-hit
> material rules, selected impact audio and automatic impact hearing now execute.
> Earlier impact-audio omissions are historical; visuals and the M2 gate remain open.

> Extended by [weapon fire-audio contracts](weapon_audio_contracts.md): Beretta/knife
> retained playback and automatic gunfire hearing now execute; source weapon voices
> survive disposable destination rehearsal. Earlier fire-audio omissions are historical.
> Required services and safe NEW_LEVEL still keep M2 incomplete.

Later [production NPC lifecycle contracts](ai_runtime_contracts.md) supersede
this snapshot's absence of Cultist/Civilian initialization, AI_Update and activation.
The existing action/service limitations remain unless explicitly superseded.

This integrates selected `AI_Mgr::HandleTrigger`, `PlayAnimation`, its MID_TRIGGER
message branch and `ObjectUtilities::SendSoundTrigger`. Player sound attempts now
send the original alerts. Read [source selections](../implementation/sources/m2_ai_trigger_sources.json),
[execution evidence](../../../Documentation/Implementation/m2_ai_triggers.md),
[object lifetime](object_contracts.md), [state actions](ai_state_contracts.md),
[target ownership](ai_target_contracts.md) and [character audio](character_audio_contracts.md).
No production NPC is registered; M2 remains incomplete.

## Commands

Command names are ASCII case-insensitive. Owned text is parsed with the existing
bounded semicolon/quoted-token parser. Input is at most 1,279 bytes (the original
64*20-byte token buffer minus its terminator), 64 commands and 64 arguments per
command. Embedded NUL, unterminated quotes and invalid numeric values fail.
The complete syntax is checked before mutations; a malformed later command cannot
execute an earlier prefix. Numeric conversions are strict instead of unchecked
atoi/atof. This is a safety adaptation, not a claim of complete SDK Parse parity.

SETSTATE uses the existing original prefix-to-state mapping and transition rules.
TARGET selects/faces the named object and then selects AttackFar. An unmatched
name is the source no-op: it retains the previous target and AttackFar is still
selected; failed services reject and no invented attack action executes. PLAYANIMATION
resolves a named clip, selects it through the checked size/animation service and
disables looping. Validation happens before changing looping, unlike the original
unchecked call order. PLAYSOUND starts a real attached spatial one-shot WAV with
priority 2, volume 100%, integer radius 5,000 by default (nonpositive becomes 1,000), and
inner radius 10%. It is unretained and expires through the existing mixer. Missing
assets fail. These four source branches return immediately, skipping later commands.

An exact whole-message TRIGGER with a configured relay target sends the same text
to named objects, using the AI as sender; zero matches is valid. Names are limited
to 100 bytes. Targets are snapshotted before delivery. Relay diagnostics count
transport deliveries, not successful recipient actions. An active-command guard
rejects recursive delivery to the same AI. SCRIPT explicitly fails without changing
state or consuming a pretend script; unknown commands also fail visibly.

MID_TRIGGER copies the caller-owned HSTRING then releases it before callbacks,
processes owned text, and preserves the original base/aggregate forwarding chain.
The boolean helper reports accepted/ignored commands separately from failures;
the original returns false even after some successful hearing branches. Engine
message delivery success still does not assert successful recipient behavior.

## Hearing rules

Sound identifiers are gunfire 0, gun impact 1 and player sound 2. Self-sounds are
ignored. A noninterruptible Script state returns immediately, including skipping
later commands. The source interruptible flag is bit 1. Gunfire/player sound
reactions require Idle, live sampled positions, and distance <= hearing range.
They use the sender's current position, not the coordinates encoded in the message.
No occlusion ray is added. Gunfire retains the source's reversed IsKindOf argument
order: `IsKindOf(CPlayerObj, senderClass)`. That accepts a player or its base class,
but rejects a derived player class; this source quirk is deliberate.

Gunfire stores the heard position and selects SearchVisual. Player sounds at
strictly less than half the hearing range do the same; at exactly half through
the outer boundary they face the sender and request Idle. Existing has-targeted/
idle-deadline rules can convert that request to SearchVisual. Hearing caches a
location without assigning a target/link or changing has-targeted. Impact sounds
require the source dodge flag (8), current/previous state not Dodge, and previous
state not Passive. They store the three message coordinates and select Dodge,
without a hearing-distance check. Guarded-out impacts do not parse unused positions.
Search/dodge state selection is not execution of their still-missing actions.

## Broadcast and lifetime

SendSoundTrigger snapshots Cabal followed by Monster identities in list order.
The player belongs to both; duplicates are deliberately retained. Sender entries
are excluded. The squared position/radius test is inclusive, using double arithmetic
to avoid float overflow. Radius must be finite/nonnegative and positions bounded.
Inside-radius recipients receive the original SOUND_TRIGGER string with one decimal
coordinate precision and a terminal semicolon. Formatting is locale-independent.
No playing voice, hearing range or visibility is required to send the notification.
Inactive/hidden Alive recipients remain eligible, pending/stale recipients do not.

Mutable list traversal is replaced with a bounded identity snapshot before any
callbacks; removal cannot invalidate an iterator or cause a later pending object
to receive a message. New list entries are deferred to the next broadcast. Sender
removal stops subsequent deliveries. Every allocated string/message/list is released,
including allocation/service failure and nested relay paths. Report fields distinguish
visited, eligible, skipped and delivered; completion permits zero recipients and
never asserts a reaction or health change. Historical sender handles in the inspector
are always resolved through checked object lookup.

CPlayerObj::PlayPlayerSound sends a player-sound alert after every playback attempt,
including missing files and streaming rejection, as in the original. Suppressed
footsteps that never call PlayPlayerSound still send no alert. Playback success and
alert success are separate counters/results. Weapon PlayFireSound/impact callers
remain unported and do not claim automatic gunfire/impact broadcasts; the receiver
and callable helper are tested with those original IDs.

## Inspection and limits

The Skeleton panel shows commands, outcome, failures, hearing range/events/reactions,
ignored events, sampled location/distance, checked sender and relay deliveries even
without a model. Player sound inspection shows real alert attempts/deliveries and
errors. The GPU diagnostic has one original hearing transition before its existing
standing-fire driver and verifies the recorded outcome with UI hidden and after reload.
That driver is not AI_Update. Full initialization, deactivation/PingObjects visibility,
autonomous update/target retention, navigation, scripts, death and production NPC
registration remain open. Required FX and ExitTrigger also keep M2 incomplete.

[Script preparation/state dispatch](ai_script_contracts.md) is now callable by an
explicit diagnostic driver. SCRIPT trigger messages still reject the missing
forced AI_Update; this boundary is intentionally not bypassed by running only
its script-state fragment.
