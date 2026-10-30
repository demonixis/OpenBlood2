# M9 — local client audio and server logical voices

Implemented against `99bdfbae`, following the authorized
[multiplayer contract](../../contracts/multiplayer_contracts.md#local-client-audio).
This is a solo-local separation, not multiplayer audio admission. M9 remains in
progress; M2-GPU-001 and M3 acceptance remain independent.

## Delivered boundary

`ServerAdapter::audio` is now an `AudioTimeline`: validated resources, original
voice ownership, durations, stops, looping cursors and sample completion. It has
no output device, PCM buffer, output gain/pause controls or music transport.
`IsSoundDone` and `GetSoundDuration` continue to read these authoritative voices.
The existing WAV/stream readers, cache limits and 48 kHz fractional-frame clock
are reused. The common voice kernel preserves sample interpolation, loop and
completion arithmetic; logical advancement supplies no PCM span. Streaming
metadata/extent failures remain logical failures, never successful completion.

`ClientSession::audio` owns a separate `AudioMixer`, clip/stream caches, output
stream, original IMA/DLS music and presentation diagnostics. Its passive frames
contain world/sequence identities, monotonic sound IDs, generation-qualified
owner/speaker provenance, resource names, playback settings, positions and logical
cursor/status values. There are no original handles, asset pointers or callbacks
in those frames. Playback voices contain null original owner/speaker handles.
Client stream readers and buffered clips are independent of server resources;
a failed client reader cannot poison another receiver or the server reader.

Admission checks the current world and increasing sequence, at most 128 voices,
unique/nonzero/nonretired IDs, bounded finite values, valid provenance and matching
local asset metadata. A rejected frame leaves active voices and the accepted
frame unchanged; preflight asset-cache reads may remain cached. Descriptors cannot
change a live voice's resource/settings. Removed IDs cannot restart in the same
world. Repeated snapshots update positions/status without rewinding or replaying
already rendered audio. Initial/late snapshots start at the supplied sample cursor.

The immediate bridge preserves the preceding completed listener pose and performs
owner/speaker cleanup before playback and logical advancement. It publishes stops
and completion afterward, and new source/FX voices at the end of the simulation
step without advancing them. Source world-owned/detached sounds survive removal;
attached sounds and explicit kills retain their existing service rules. Sound ID
allocation remains solely in source/menu `play` calls: constructing extra receivers
does not consume server sound IDs or random draws. CMoveMgr and its integration
path are unchanged.

Frontends, PCM inspection and the benchmark use the client mixer. Source
CinematicTrigger/music/weapon rules are not rewritten. New game, save promotion
and NEW_LEVEL transfer the client output device and its gains/pause/lead settings;
world voices/caches stay with their own session. Unload clears the replica after
original object cleanup. The menu keeps its separate mixer. Inspection distinguishes
server logical completion from client cursor/gains and displays replica identities.

## Deliberate clock separation

The interactive client keeps the existing 60 ms device-queue lead. Previously the
same mixer advanced both audible and gameplay voice cursors by the extra queue
refill frames. The server now advances only the validated simulation delta: device
availability, queue depth, gain, pause and failures cannot accelerate its completion.
This removes the old device-dependent early completion (up to the refill lead);
it is an explicit timing adaptation, not a claim that interactive legacy completion
was already device-independent. Zero-lead/offline solo arithmetic remains exact.
The local solo pause gate freezes both timelines, while `step_server` ignores all
client pause/device state and continues with zero clients.

## Verification

The [source ledger](../sources/m9_client_audio_sources.json) distinguishes existing
reconstruction arithmetic from original game references. Linux is the only platform
executed for this lot. The [validation record](evidence/m9-client-audio-2026-10-30/validation.json)
records the selected tests, traces, platform and preserved files.

- **Headless development:** 307 selected tests (243 synthetic, 64 private-data).
  The [initial run](evidence/m9-client-audio-2026-10-30/headless-initial.log) passed
  304. `randomsound.spatial` still inspected server listener gains;
  `audio_stream.city` still expected PCM reads from the logical stream. Both now
  check client playback, retaining server cursor/lifetime and independent-reader
  checks; the [16-test audio rerun](evidence/m9-client-audio-2026-10-30/headless-audio-final.log)
  passes. Aggregate result: **306/307**, not an all-green suite.
  `world.destination_private` still expects 39,745 CityHub triangles and observes
  39,724, the [previously recorded failure](reported_level_loading.md). Its
  expectation and collision implementation are unchanged by this lot.
- **ASan/UBSan:** [41/41 targeted synthetic tests](evidence/m9-client-audio-2026-10-30/asan-ubsan.log)
  pass in the Clang server-only tree, including the independent receiver and
  logical clock. The private audio test also exits 0 on both retail worlds below,
  without sanitizer diagnostics.
- **GPU:** [12/12 pass](evidence/m9-client-audio-2026-10-30/gpu-final.log) on Linux
  Vulkan / NVIDIA RTX 3060 Laptop: movement, doors, triggers, pickups, keyframes,
  keyframe audio, cinematics, random sounds, sounds, level changes, menus and menu
  traversal. The [initial run](evidence/m9-client-audio-2026-10-30/gpu-initial.log)
  had two failures: diagnostic code inspected newly created/killed sounds before
  publishing them. Explicit publication after those direct diagnostic commands
  fixes both; the production step already publishes at its normal boundaries.
  This is automated rendering/device coverage, not an audible listening session.
- **Release:** inspector disabled, application built; the
  [240-frame movement smoke](evidence/m9-client-audio-2026-10-30/release-movement.log)
  exits 0, including walk/jump/crouch, hidden playback and unload/re-entry.
- **Dedicated:** a [real server process](evidence/m9-client-audio-2026-10-30/server-private.log)
  completes 180 ticks on `multi/bb_crypt` with no clients and deliberately
  unavailable SDL audio/video drivers. The ASan run records 182 discarded ticks:
  this proves device-independent progress, not real-time 60 Hz performance.

The development and Release builds use GCC 16.2.1; the server-only sanitizer build
uses Clang 22.1.8. The existing development Ninja dependency log still reports
`premature end of file; recovering`, with repeated broad rebuilds. This is not
claimed fixed. Local dev/Release module scanning was disabled in CMake cache only.
After completing the broad build, the final two test compile fixes and application
diagnostic fixes used the recorded compile/link commands (see
[rebuild helper](evidence/m9-client-audio-2026-10-30/rebuild-selected.py)); archive
members were checked for zero size. Initial compile failures and fixes remain
listed in the validation record.

The engine-core-only receiver covers atomic invalid-frame refusal, duplicate and
old-world/sequence/retired IDs, excessive counts, resource/metadata failure,
independent receivers, late cursors and no rewinding. Its symbol audit contains no
ServerAdapter, Objects, CBloodServerShell or original object dispatch. Local tests
exercise independent stream readers, attached motion/removal, world ownership,
kills/unload, client-error isolation and detached playback after server teardown.
A dummy SDL output tests queue lead, gain/pause and independent dedicated completion.
The private test runs 300 1/60-second ticks on each of SubwayA and CityHub, comparing
all logical/client cursors, loops and done flags with PCM inspection enabled.
These are opening/short-scene checks, not campaign traversal or audible listening.

A 360-step synthetic baseline was built from the `99bdfbae` source snapshot and
its existing libraries before rebuilding them. It records attached/world-owned/
detached playback, source removal, stop, mixed PCM bit hashes, peak, logical cursor,
loop/completion and server RNG draws, at three interleaved frame deltas. Baseline
SHA-256: `ab764f40ffb1fb5d3b04e43a0c9d40437acde00aeba64fc87c94a5102c7c3563`.
The [harness](evidence/m9_client_audio_oracle_2026-10-30.cpp) and
[baseline trace](evidence/m9_client_audio_trajectory_2026-10-30.txt) are retained;
the separated implementation produces identical bytes, including PCM/RNG.
The existing movement (360 steps), environment (720) and camera/HUD (300) traces
also remain byte-identical. Their hashes are in the validation record; their
previous evidence files were not rewritten.

The old save-refusal fixture opened a candidate audio device through the bound
server singleton. That access no longer exists. It now verifies occupied-destination
refusal directly on `AudioMixer::transfer_output_to`, with source output preserved;
actual save/load output transfer still has its integration checks. Injecting this
specific final promotion failure through a server restore callback is no longer
covered. Other restore/validation refusal cases remain in that suite. Synthetic
resampling clips now mutate a standalone client mixer, rather than falsifying
server asset metadata that the new receiver must reject.

## Reproduction

Use a configurable external retail data directory; each CTest uses an isolated
user directory. The private test names are retained in the validation record.

```sh
cmake --build build/dev --target blood2_app $(cat Documentation/implementation/reports/evidence/m9-client-audio-2026-10-30/build-targets.txt)
ctest --test-dir build/dev --tests-from-file Documentation/implementation/reports/evidence/m9-client-audio-2026-10-30/selected-tests.txt --output-on-failure -j6
ctest --test-dir build/dev -R '^(randomsound\.|audio_stream\.)' --output-on-failure -j4
ctest --test-dir build/dev -R '^gpu\.(movement|doors|triggers|pickups|keyframes|keyframe_audio|cinematics|randomsound|sound|level_change|menus|menu_traversal)$' --output-on-failure -j1
cmake --build build/server-clang-sanitizers
ctest --test-dir build/server-clang-sanitizers -R '^(objects\.|network\.(audio_|view_|environment_|collision_|movement_|replica_|pickups|server_clock_audio|server_help))' --output-on-failure -j4
build/server-clang-sanitizers/tests/network/network_audio_local_tests private "$BLOOD2_DATA_DIR"
SDL_AUDIODRIVER=unavailable SDL_VIDEODRIVER=unavailable build/server-clang-sanitizers/blood2-server --data-dir "$BLOOD2_DATA_DIR" --world multi/bb_crypt --mode bloodbath --port 0 --ticks 180
cmake --build build/release --target blood2_app
BLOOD2_USER_DIR=/tmp/blood2-m9-audio-release-user build/release/blood2 --config build/release/tests/empty.ini --data-dir "$BLOOD2_DATA_DIR" --movement-smoke --capture /tmp/m9-audio-release.png
```

## Limits and preservation

Frames are complete synchronous publications, not a wire codec, queue or delayed
network synchronization. Original client FX/VoiceMgr requests still enter the local
server sound service; recipient filtering and the remaining client-shell/FX
extraction are pending. No LAN, impaired-network or multiple-process qualification
is claimed. Native Windows/macOS/mobile/Quest, XR hardware and audible-device
qualification remain outstanding; no simulator/emulator was used.

Server and client each retain the existing 128-clip/256 MiB cache budget and 128
voice limit. Server buffered sounds still validate/decode their real asset; this
lot does not add a duration-only WAV reader or shared mutable stream cache. Copies
increase local CPU/cache work; no performance improvement is claimed. Original
reverb/occlusion/priority and region-loop limitations remain unchanged.

The 39 existing PBR files and all prior evidence/captures are protected separately
by SHA-256 and excluded from this lot. Original reference repositories remain
unchanged. The root's existing PBR guidance remains outside the commit.
