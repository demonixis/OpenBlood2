# M9 — Remote logical sounds and independent playback

2026-10-31, following `6fa73b97`. Dedicated and playable-host sessions now send
server sound starts/stops to each independent NetworkClient. The graphical
frontend opens its own SDL output and applies existing sound/music preferences.
The server still owns only logical duration, looping and completion. This lot
extends the [local audio boundary](m9_client_audio.md) and
[multiplayer contract](../../contracts/multiplayer_contracts.md#remote-logical-sound-lifetimes).
[Provenance](../sources/m9_network_audio_sources.json) distinguishes existing
source sound services from the new transport/playback policies.

## Sound lifetimes

ServerAdapter observes successful starts and retirements synchronously. It
captures owner/speaker ObjectId values before original handles can disappear;
there are no serialized pointers or server audio resources in the client.
PlayDetachedSound publishes once through PlaySoundAt. Original logical ownership,
auto-removal, IsSoundDone/GetSoundDuration and sample advancement stay unchanged.
Solo installs no observer and retains its existing immediate ClientAudio frames.

A retained cinematic voice whose speaker disappears stops remote playback when
prepare_audio detects the loss. Its logical handle still remains until the
original controller polls/kills it. Later retirement sends no duplicate stop.
No new source RNG draw or cosmetic sound allocation occurs on the dedicated.

Protocol **14** adds B2AU Start/End messages on reliable Control. Each packet is
at most 256 bytes, encoded explicitly little-endian with bounded strings,
finite values, asset dimensions, world/recipient/sequence and sound/object IDs.
A recipient has at most 512 queued events; overflow fails only that recipient's
publication. Starts/ends are captured even when a short sound is created and
retired between two network pumps or inside catch-up. State packet loss does not
remove these events. Existing ENet packet and 2 MiB peer-queue limits are unchanged.

Audio begins after the client's world-baseline acknowledgement. Initial delivery
contains only currently playing voices at their current logical cursor, including
loops; historical or already-completed one-shots are omitted. Local player-owned
sounds are addressed only to that player. A cinematic controller's local sound
remains broadcast, as do spatial/ambient voices. Provenance and private ownership
are captured once at creation and retained by sound ID until retirement: a
world-owned private one-shot cannot become public to a late joiner after its
player disappears or its object slot is reused. No distance culling or invented
sound substitution is introduced.

NetworkAudio loads its own buffered/streaming resources and verifies rate,
channels and frame count before admitting a voice. Duplicate delivery does not
restart it; sequence gaps, stale recipients/worlds, reused IDs, invalid metadata
and missing resources fail explicitly. Attached sounds follow generation-checked
ClientWorld poses. If membership has not arrived yet, playback retains the
captured position until the speaker can be resolved. Original handles in playback
voices are null.

New one-shots start at the captured cursor, normally zero, without compensating
for measured network latency. Natural server completion retires the logical
handle but lets delayed client playback finish; explicit cancellation stops it.
This is a replacement network policy, not verified retail transport timing.
The mixer retains the existing 128-voice budget. When delayed natural tails fill
it, new logical voices can displace only the oldest retired tail; active logical
voices are never displaced silently. Counts expose those displacements.

Mixing advances on the client's elapsed time and uses the existing camera
listener position/rotation rules. Device lead, menu/focus state and client
resource/output failures never advance server clocks or mutate another client's
sound. Disconnect clears playback. The same endpoint runs for host and remote
clients; a remote client still links no original server gameplay classes.

## Verification

**16 distinct native tests and five ASan/UBSan tests pass.** Commands and full
output are retained under `evidence/m9_network_audio_*_2026-10-31.*`.

- Codec: golden End packet, both variants round-trip, every truncation, trailing
  bytes, protocol mismatch, invalid IDs/assets/values and 40,000 byte mutations.
- Independent generated-resource receiver: exact PCM peak/cursor, start and
  natural end delivered together, duplicate/gap/world/private gates, streaming
  loop late cursor, explicit cancellation, atomic missing/changed resource
  refusal, delayed speaker membership and bounded 128-voice tail displacement.
- Retail server fixture: zero-client advancement, private local versus broadcast
  sound, a one-shot started/finished without intervening publication, late loop
  cursor, explicit stop, cinematic speaker-loss with retained logical handle,
  no duplicate stop, private tails after owner departure and isolated 512-event overflow.
- Existing local audio receiver/device/private/effects and server clock tests;
  existing admission, movement UDP, reliable impulses and weapon ACK delivery
  while State packets are discarded.
- Real-process dedicated PvP: server plus two separately linked clients on Blood
  Arena, actual Beretta fire, one frag/death and USE respawn. Native run: 13 shots,
  ammo 37, 13 impulses, new force 2. Both clients started 30 audio voices, observed
  9 explicit stops and 20 natural retirements; final measured PCM peaks were 0.872040
  and 1. Server: 2,400 ticks, zero discarded ticks, two admissions/departures.
- Actual graphical host with two remote combatants on Fields: 33 shots, ammo 17,
  13 victim impulses, one frag/death and health-100 respawn. Remote clients each
  started 46 voices with 29 explicit stops and 17 natural retirements; PCM peaks
  0.651280 and 0.771209. Host rendered 1,200 frames, admitted/departed three
  players, ran 3,609 ticks and reported **eight discarded server ticks**, zero
  local discarded ticks. This is not a load-capacity qualification.

Five solo traces remain byte-identical to the previous lot: effects 240 frames,
audio 360, movement 360, environment 720 and view/HUD 300. Protected-file hashes
cover 6,753 PBR/reference/prior-evidence files. No reference image or commercial
asset was changed. The host capture was inspected for terrain/sky, held Beretta,
crosshair and HUD; its hash is recorded without replacing a golden image.

The first compile failed because the previously engine-only SoundParameters
header used an include-search-path-dependent legacy header name. The include
now names its explicit existing path, allowing the standalone network library
to use these passive values. This failure is retained. Subsequent project builds
have no new warnings; the external pre-existing effects-oracle helper retains
its known indentation warning. Native and sanitizer independent-client symbol
checks exclude ServerAdapter, GameSession, server shell, object gameplay, CWeapon
and inventory-manager definitions.

## Remaining scope

This is server logical sound playback, not all remote audio. CViewWeapon animation
keys, private inventory/VoiceMgr/CommLink/frag/FinishHim announcements, music and
cosmetic effect sounds still need their client consumers. Their receipt must not
be reported as playback. Visual effects/lights/fog, Wonky visuals, scoreboard/chat,
match rules/rotation, remaining weapon/creature multiplayer branches, Team/CTF,
LAN discovery and original Host/Join screens remain in progress.

The host fixture uses SDL's dummy output driver and separately verifies PCM in
remote processes. It does not certify physical speakers, human LAN, all 22 maps,
16-player load, impaired combat/audio, Windows/macOS interoperability or mobile/
Quest execution. Five synthetic/headless sanitizer cases and bounded retail GPU
coverage are not full M9 acceptance. M2-GPU-001 and M3 acceptance remain separate.
