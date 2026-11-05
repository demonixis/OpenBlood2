# M9 — Bounded client audio cache reclamation

2026-10-31, following `17d2eec6`. The combat-announcement audit exposed that a
network client retained every decoded sound until world unload and eventually
refused new sounds at 128 distinct clips. Independent NetworkAudio now reclaims
unused entries when necessary, preserving the existing combined 128-entry and
256 MiB budgets. Both remote sounds and private speech opt in. Logical server
AudioTimeline and solo playback retain their previous policy.

See the [contract](../../contracts/multiplayer_contracts.md#bounded-network-audio-cache-reclamation)
and [baseline ledger](../sources/m9_audio_cache_sources.json). This is replacement
engine cache policy; no original gameplay, source RNG or commercial data changes.

Only entries owned solely by the cache are eligible. Live voices, completed voices
whose handles remain retained, and externally prepared clips pin their assets.
Each budget check first establishes that enough unused space exists, then evicts
only enough entries in stable key order, buffered before streamed. Stream-reader
validation pointers are cleared after eviction; window sizes/phases and source
extent checks remain unchanged. Normal cache hits retain their existing behavior.
An evicted sound reloads from client resources on its next request.

The policy never stops a voice, changes a cursor, changes loop counts or completes
retained speech early. Fully pinned cache budgets still refuse admission. Physical
voice and local completion-record limits remain 128. Buffered decoding retains
its existing bounded temporary allocation before the final cache-byte check;
the cache limit is not a bound on every temporary/external reference. Eviction
counts are inspectable and reset on clear. Storage stalls and full pinned budgets
are not solved by this change.

## Verification

**17 native tests, ten ASan/UBSan tests and nine inspector-free Release repeats
pass.** Commands, complete outputs and checks are retained in
`evidence/m9_audio_cache_*_2026-10-31.*`. All project builds are warning-free.

- A generated-resource client requests 512 distinct buffered/streamed clips while
  retaining an actual server loop and private stream. It performs 384 evictions,
  stays at 128 cache entries, preserves both clip identities and cursors, then
  produces the expected PCM. Further server starts, duplicate delivery, reload,
  stop and clear are checked. Completed-but-retained and externally prepared clips
  remain pinned. Fully pinned admission fails; releasing the last external owner
  allows one eviction and succeeds. Default logical audio still refuses a 129th
  distinct clip even when earlier voices have been released.
- A separate synthetic test applies 256 MiB pressure using distinct resource names
  sharing generated WAV bytes. It reclaims one unused clip, keeps eight cached
  entries within the byte budget, and preserves the retained clip, cursor and PCM.
- One independent client plays all **173 available retail combat-announcement
  WAVs sequentially**, with **45 evictions**, zero playback failures and PCM peak
  **0.244141**. The 36 absent original paths remain explicitly accounted for by
  the combat-announcement resource audit; no substitute audio is introduced.
- Existing wire codec/mutation, playback, speech, shared-voice capacity and combat
  announcement checks pass. Six solo streaming tests cover PCM, malformed data,
  mixing, seams, truncation and window phases. Five fresh solo traces are byte-
  identical: effects 240, audio 360, movement 360, environment 720, camera/HUD 300.
- Native and sanitized dedicated Blood Arena repeat two actual clients, 2,400
  server ticks without discards, 13 shots, one frag, 13 impulses and health-100
  respawn. Fields hosting with two remote combatants completes 1,200 frames,
  3,608 server ticks with nine discarded ticks, zero local discards, three
  admissions/departures, 33 shots, one frag and health-100 respawn. These scripted
  lifecycle checks do not qualify loaded or impaired network play.
- Independent native/sanitized/Release cache-test executables contain no server
  gameplay definitions. All **6,845 protected hashes** remain unchanged, including
  PBR files, immutable GPU captures and previous evidence. CMoveMgr still integrates
  once. The external effects-oracle helper retains its known indentation warning.

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json ctest --test-dir build/m9-fx-runtime -V -R '^(network\.(audio_cache_count|audio_cache_bytes|audio_cache_private|audio_codec|audio_playback|combat_speech|combat_announcer_private|speech|speech_capacity|pvp_processes)|audio_stream\.(pcm|malformed|mixer|seams|truncation|phases)|gpu\.network_pvp_host)$'
ctest --test-dir build/m9-fx-runtime-asan -V -R '^network\.(audio_cache_count|audio_cache_bytes|audio_cache_private|audio_codec|audio_playback|combat_speech|combat_announcer_private|speech|speech_capacity|pvp_processes)$'
ctest --test-dir build/release -V -R '^network\.(audio_cache_count|audio_cache_bytes|audio_cache_private|audio_codec|audio_playback|combat_speech|combat_announcer_private|speech|speech_capacity)$'
```

This closes cache-only accumulation for network playback. It does not change the
server/solo retaining policy, certify unbounded simultaneous sounds, or qualify
physical speakers, human LAN, impaired audio/combat, all maps, 16-player load,
Windows/macOS interoperability or mobile/Quest execution. Communication UI,
remaining remote effects/music, match rules, Team/CTF and LAN screens continue
under M9. M2-GPU-001 and M3 acceptance remain separate.
