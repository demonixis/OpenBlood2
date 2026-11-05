# M9 — Independent player speech and communication audio

2026-10-31, following `09f39c92`. Original CVoiceMgr and CCommLink now link into
the independent network client. Bloodbath startup announcements, private voice
events, conversations and voice stops produce actual client PCM. The solo shell
uses the same classes through an immediate adapter that preserves its previous
server sound calls. See the [contract](../../contracts/multiplayer_contracts.md#independent-client-speech)
and [source ledger](../sources/m9_client_speech_sources.json).

## Service boundary and source rules

VoiceServices exposes only client time and retained local sound play, stop,
completion and duration. It carries no player/server handles. LocalVoices binds
the current solo player and delegates to the existing GetTime, PlayCinematicSound,
KillSound, IsSoundDone and GetSoundDuration services. VoiceMgr/CommLink no longer
include ServerDE or link server gameplay. The solo bridge still uses the same
logical sound ownership and clocks; this lot does not migrate that ownership.

The source selection and timing bodies are unchanged: four characters, twelve
groups, custom-group precedence, bounded direct WAV enumeration, one-file/no-draw
behavior, nonrepeating selection and independent client RNG. Retain enabled,
busy, external-camera and strict cooldown gates in their original order. Only
Jump is buffered; other event groups and Caleb's unique lines stream. A missing
replacement still stops the prior voice, and a failed unique does not become
played. Local voice priority is 5, without reverb or looping; duration plus 0.5
seconds and the original unavailable-duration fallback remain unchanged.

CommLink keeps busy refusal, strict 0.2-second entry/exit transitions, text-derived
duration and buffered priority-2 local playback. Conversation receipt stops
VoiceMgr even when CommLink is busy. BloodbathStart does not make that immediate
stop; CommLink stops VoiceMgr when its voice actually starts after entry. Its
text deadline can stop a longer sound. The original portrait/text state remains
available, but this lot adds no communication HUD renderer.

NetworkSpeech consumes only applied, addressed private events, once per sequence.
It uses the client's monotonic elapsed time, independent of server tick mapping
and device lead. The original float arithmetic remains inside VoiceMgr/CommLink.
The selected camera gates private voice events. The endpoint advances CommLink
once per outer update and clears retained speech before clearing its audio mixer.
No protocol change, server sound allocation or gameplay RNG draw is introduced.

## Shared client playback budget

NetworkAudio admits local retained voices into its existing mixer and resource
cache. They use high-namespace sound IDs, null original handles and at most 128
retained local records. Total physical playback remains bounded to 128 voices,
including server sounds. Local calls cannot stop server sound IDs.

A new server sound first displaces the oldest naturally retired server tail. If
none exists, it can preempt the oldest local physical voice, retaining that
voice's duration and remaining completion deadline. It never preempts another
active server voice. An unadmittable local request fails explicitly. Silent
completion uses the mixer's full client sample clock, including output lead and
frames skipped by its bounded rendering block, not the count of rendered frames.
Preemption therefore cannot release the original busy gate early. Stop releases
the retained record; disconnect clears all state. This is a bounded replacement
playback policy, not a claim about the original engine's channel allocator.

## Verification

**32 distinct native tests and five ASan/UBSan tests pass.** Full commands, outputs
and intermediate failures are retained under `evidence/m9_client_speech_*_2026-10-31.*`.

- Generated-resource independent speech tests verify real PCM/cursors, streaming
  and buffered groups, unique flags, missing resources, external-camera/busy/
  cooldown ordering, conversation interruption and startup's delayed stop.
  Stale recipients, duplicate events and backwards clocks are checked.
- Capacity coverage fills all 128 voices, admits a server sound by preempting
  local PCM, and verifies retained duration, non-early completion, bounded local
  refusal and isolation from server handles. Existing audio codec and independent
  playback checks still pass, including 40,000 packet mutations.
- Existing player_voice, conversation, movement_audio and native save/load cases
  pass. Five solo traces are byte-identical to the preceding lot: effects 240
  frames, audio 360, movement 360, environment 720 and camera/HUD 300.
- Dedicated retail Blood Arena with two actual client processes: 13 Beretta shots,
  ammo 37, one frag/death, 13 impulses and health-100 USE respawn. Both clients
  start 30 server voices (9 explicit stops, 20 natural retirements); attacker and
  victim additionally start one and two private voices respectively. Both produce
  PCM peak 1. The server runs 2,400 ticks, zero discarded ticks, and two admissions/
  departures. Native respawn force changes 2 → 3; sanitizer changes 1 → 2.
- Graphical host on Fields with two independent remote combatants: 33 shots,
  ammo 17, one frag/death, 13 impulses and health-100 respawn. Each remote starts
  46 server voices (29 explicit stops, 17 natural retirements), plus one/two private
  voices. Peaks are 0.976562 and 1. The host renders 1,200 frames, runs 3,608 server
  ticks with **eight discarded server ticks**, zero local discarded ticks, and
  three admissions/departures. This is not a load-capacity qualification.
- The existing solo GPU conversation smoke passes on Linux/NVIDIA. Captures were
  inspected: the host shows Fields terrain/sky, held Beretta, crosshair and HUD;
  the solo capture shows actual communication/audio state. Capture hashes are
  recorded without changing golden images.
- Inspector-free Release builds successfully. Its four network audio/speech
  tests and the solo GPU conversation smoke pass as five additional runs of
  cases already counted above. Compiler warnings occur only in unchanged
  MainMenus.cpp and game_setup.cpp aggregate initializers.

Reproducible focused commands (the native/sanitizer directories retain their
existing private-data configuration):

```sh
ctest --test-dir build/m9-fx-runtime -V -R '^(conversation\.|player_voice\.|movement_audio\.|save_native\.|network\.(audio_codec|audio_playback|speech|speech_capacity|pvp_processes)$)'
ctest --test-dir build/m9-fx-runtime-asan -V -R '^network\.(audio_codec|audio_playback|speech|speech_capacity|pvp_processes)$'
cmake --build build/release --target blood2 network_speech_tests network_audio_events_tests -j6
ctest --test-dir build/release -V -R '^(network\.(audio_codec|audio_playback|speech|speech_capacity)|gpu\.conversation_private)$'
```

GPU invocations on this machine selected the NVIDIA Vulkan ICD through
`VK_DRIVER_FILES`; this is a test environment choice, not a production requirement.

Native and sanitized independent speech/PvP executables contain no ServerAdapter,
GameSession, server shell, original object gameplay, CWeapon, inventory manager or
LocalVoices definitions. All **6,773 protected hashes** remain unchanged, including
PBR files, immutable reference images and previous evidence. No commercial data
is committed. CMoveMgr integration and server logical audio remain unchanged.

The first builds exposed missed test call sites after service extraction, then
a movement-audio fixture still using the old pre-M9 CMoveMgr API. These were fixed
without changing their behavior expectations. The first capacity check exposed
use of rendered-frame count for preempted completion; it now uses the full sample
clock. The player_voice.damage fixture attempted to restore solo with reversed
set_game_settings arguments and was correctly refused by multiplayer SaveFields.
It now names both enums, asserts multiplayer refusal, then explicitly restores
solo before its existing save/load assertions. The save guard was not changed.
All failed runs remain in the evidence. The pre-existing external effects-oracle
helper's indentation warning remains separate from project build results.

## Remaining scope

FinishHim/frag announcements, music, inventory activation sounds and network jump/
impact cosmetics remain to connect. Original multiplayer weapon SOUND_KEY,
SOUNDLOOP_KEY and SOUNDSTOP_KEY go through the server; a future implementation
must validate animation keys and preserve the per-player loop without duplicate
prediction playback. The existing shotgun-only local solo adaptation is unchanged.

Visual effects/lights/fog, Wonky visuals, scoreboard/chat, match limits/rotation,
remaining multiplayer branches, Team/CTF, LAN discovery and Host/Join screens
remain in progress. Dummy host output plus measured remote PCM does not qualify
physical speakers, human LAN, all 22 maps, 16-player load, impaired combat/audio,
Windows/macOS interoperability or mobile/Quest execution. M9 remains active;
M2-GPU-001 and M3 acceptance are separate.
