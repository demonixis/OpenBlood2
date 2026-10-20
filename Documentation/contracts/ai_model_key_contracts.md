# Original AI animation keys

> Extended by [AI weapon-action contracts](ai_fire_contracts.md): supported weapon
> choice, aiming, clearance, standing fire and AI fire_key now execute. Earlier
> blanket weapon/fire omissions are historical; full NPCs and M2 remain open.

> Extended by [AI state/action contracts](ai_state_contracts.md): selected original
> state decisions, transitions and stationary animation commands now execute.
> Full AI_Update and production NPCs remain unavailable; M2 is still open.

M2 remains incomplete. This selectively ports AI_Mgr::InitStatics and OnStringKey,
using the existing [character audio](character_audio_contracts.md),
[motion keys](character_motion_contracts.md), [camera](camera_contracts.md) and
[object lifetime](object_contracts.md) services. See the
[source ledger](../implementation/sources/m2_ai_model_key_sources.json) and
[execution evidence](../implementation/reports/m2_ai_model_keys.md).
No production AI class is registered: CivilianAI, CultistAI and Gideon still
require their original states, initialization, perception, movement and attacks.

## Dispatch and source rules

Only argv[0] is examined. Exact case-sensitive bracket lookup retains the first
value. The source's mutually exclusive priority is fire_key, show_weapon,
extra_key, then play_sound, regardless of textual order. Normal branch completion
forwards to CBaseCharacter motion keys. A fire key without a target returns
immediately, including skipping base motion. A pending/stale target fails safely.
Fire with a live target, ShowHandModel and MC_Extra remain explicit unavailable
operations. The original show_weapon branch reads an uninitialized buffer;
this port does not invent a weapon index or successful operation.

The play_sound branch uses the **server g_hActiveCamera**, not client camera
selection. Outside STATE_Script (101), a live camera that disallows player
movement suppresses the sound and returns before base motion. Script state skips
sound but still forwards to base motion. Pending/stale cameras are ignored;
a live wrong-class camera is a checked failure. The default state is source
STATE_Idle (5); no state machine is implied by exposing this field.

A sound name always receives .wav, including a name already ending in .wav.
Presence of sound_random consumes an inclusive 1..3 variant draw. The source
SharedDefs.h unconditionally defines B2_LITE, so NRES(nMax) expands to **3**:
authored maxima such as 11 or 4 do not widen that range. Its atoi result is unused;
a nonempty nonnumeric or nonpositive random value therefore still selects 1..3.
This preserves the supplied source configuration, not an inferred retail edition.

Radius defaults to 1000, volume to 100, chance to 100 and voice to false.
A variant draw precedes option parsing and the chance draw. IsRandomChance always
consumes a 0..99 draw, even at 0/100; the comparison is draw < chance without
clamping. Negative chance skips and values above 100 always pass. The existing
portable session RNG replaces CRT rand/IntRandom; both draws use that one stream,
without recreating separate CRT state. Deterministic local tests do not establish
retail random-sequence equivalence. Camera/script suppression
consumes neither draw. Nonzero voice selects PlayVoice; zero selects PlaySound.
The existing attached WAV decoder/mixer handles actual playback and failure.

## Ownership, bounds and inspection

Original subclasses share class/gender CAnim_Sound tables, including one retained
voice. InitStatics accepts a shared_ptr instead of a borrowed raw pointer to keep
that sharing alive safely. A null table is rejected without replacing the current
one. Caller groups must preserve the original sharing policy when real classes
are integrated; these tests use explicit groups. No per-character voice policy
is substituted. Busy shared voices refuse a replacement; nonvoice one-shots are
independent. Removing one speaker invalidates its attached sound at the next
audio sweep; the table survives other owners. The final owner releases its retained
handle. Session teardown destroys objects before clearing audio and caches.

Existing Sparam bounds remain in force. Sound basenames are limited to 31 bytes.
Numeric options use finite, locale-independent, strict signed parsing with a
31-byte bound, replacing the source's unsafe 8-byte scratch buffers and permissive
atoi/atof. Empty, malformed, overflowing and nonfinite parameters fail explicitly.
The existing audio service rejects invalid paths, volumes and radii. Missing WAVs
never become silent successful placeholders. Bounds/parse failures return before
base dispatch; ordinary chance/busy/playback outcomes still reach base motion.
No new per-frame service, dependency or duplicate physics integration is added.

The Skeleton tab displays state, last bounded key, branch outcome, actual sound
resource, variant/chance draws, suppression counts and unavailable actions. It
also shows the shared decoded voice cursor/duration and whether the selected
object is its owner. Helper counters are shared; per-object key/start counters
remain separate. Unretained PlaySound returns zero even on success, so starts
are established by the helper's actual mixer result, not a nonzero handle.
Inspection has no dispatch/lifetime side effects. Hidden/inactive objects remain
inspectable; inactive animation tracks do not advance.

## Coverage and remaining gate

Synthetic tests cover branch priority, base forwarding/early returns, camera vs
script rules, random presence/range/draw count, suffix construction, real decoded
voices, busy sharing, removal/retry, malformed options, pending identities,
immutable model callback data, inactive tracks and three unload cycles.
The fixture REZ writer now splits a filename at its final dot, allowing the
source's cue.wav.wav suffix behavior to be tested without changing production paths.

The private six-model catalog executes 147 actual animation keys: **140 sound
keys start 140 decoded voices**, six fire keys have no target and Gideon's one
extra_key remains unavailable. All sound attempts succeed in this seeded traversal;
this is not an exhaustive inventory of every variant or an enemy behavior test.
The test advances audio between clips; it does not simulate original AI timing.
Two SubwayA cycles verify intro-camera suppression, playback after normal intro
completion, sample advancement and clean unload.

The existing --skeleton-smoke adds an explicitly labelled **AI model-key probe
(no AI states)** using an unmodified male-cultist ABC clip and source sound root.
The clip restarts when ImGui hides, must dispatch a second key with real voice
advancement, then a newly created probe must play after reload and invalidate the
old identity. Its diagnostic class is registered only in this smoke scenario.
Existing player movement/audio/node and synthetic motion assertions remain intact.
Its extra runtime object never counts as an authored NPC.

SubwayA stays 187 created, 4 transferred world sounds, 372 editor-only, 17 unsupported
and zero failed, plus 7 SelfTriggers. The missing source instances remain five
CivilianAI, ten CultistAI, Gideon and ExitTrigger. AI hearing, aiming/fire/extra,
attachments, required client FX and other services keep --start-game blocked
with exit 6. Other platforms, XR, audible hardware and retail gameplay parity
are not established by Linux tests and SDL dummy output.

The [AI visibility continuation](ai_sight_contracts.md) adds selected original CanSee
and a typed ray object filter. Default weapon/USE ray behavior is unchanged.
Visibility queries do not implement target selection, AI states or complete M2.

The [explicit target/facing continuation](ai_target_contracts.md) adds original
AI_Mgr named target/link commands and deferred target clearing. GetObjectPos now
returns a typed status; target sampling and CanSee reject unresolved poses.
Autonomous acquisition/retention and full AI behavior remain unavailable.
