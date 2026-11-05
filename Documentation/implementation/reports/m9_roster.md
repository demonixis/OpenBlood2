# M9 — Independent Bloodbath player roster

2026-10-31, following `07d60ab1`. Remote clients now own names, characters and
frag scores for current Bloodbath players. Reliable arrival/departure state and
original frag events supply the data required by announcement and scoreboard
consumers. This lot does not draw a scoreboard or play those announcements.
See the [contract](../../contracts/multiplayer_contracts.md#reliable-bloodbath-player-roster)
and [source ledger](../sources/m9_roster_sources.json).

## Source behavior and adaptation

Original BloodServerShell initializes a profile, broadcasts its AddPlayer,
then sends existing players to the newcomer. It publishes RemovePlayer before
dropping the flag and destroying the player. BloodClientShell uses FragInfo's
names/characters for frag messages. The port restores the original GetPlayerName
accessor and reads the initialized original name, character and m_Frags value.
It changes no score rule, damage, movement, simulation clock or RNG draw.

Protocol 15 appends private tag 26. A full admission-ordered roster replaces
the original add/remove messages. It carries at most 16 connection IDs, exact
object generations, 100-byte names, character IDs and signed bounded scores.
The maximum payload is 2,016 bytes including the private header, below the
unchanged 2,048-byte limit. Decode rejects oversized/empty lists, duplicated or
unordered connection IDs, reused object slots, invalid values and trailing or
truncated bytes before changing output. The original team field awaits Team/CTF.

Publication runs after profile initialization and before departure destruction.
Late arrivals get current scores; reliable sequencing keeps snapshots and
FragEvent's absolute score updates in order. If a queue cannot accept a roster,
that recipient fails closed at its next drain. Gameplay departure still releases
the body; other recipients continue. No dropped update silently leaves a ghost
in a surviving client's list.

NetworkPlayerEvents owns the client rows independently of ClientWorld and the
server registry. Replacement validates every row and requires its own matching
identity. An admitted name, character or object cannot change; a departed
connection ID cannot return. Reconnect uses a new admission and object generation.
Exact object identity selects a frag row, including suicide decrements. Duplicate
events do not reapply scores. A new-world begin clears all rows and the connection
high-water mark. Names containing percent signs remain ordinary data.

## Verification

Native: **10 distinct tests pass**. The final ASan/UBSan outcomes pass for **eight
tests**; the initial process timeout remains recorded below. Inspector-free
Release: **five tests pass**, repeating native cases. Raw commands/logs are retained under
`evidence/m9_roster_*_2026-10-31.*`.

- Private-event codec: 26 tags, canonical little-endian headers, every truncation
  and 39,000 packet mutations. The full 16-player, 2,016-byte roster additionally
  checks every truncation and count/identity/name/score bounds. Independent
  receiver tests cover atomic refusal, duplicate events, stale world/generation,
  removal, reconnect, self omission, ordinary frags and suicide scores.
- Retail Crypt fixture verifies original frag/suicide production, names and
  characters, arrival after a frag, ordered removal, fresh same-name reconnect,
  refused admission and cleanup with a saturated recipient. No movement is run.
- Native and sanitized ENet admission checks use 16 actual peers over 186 ticks,
  including four timeout phases and reconnect. All peers receive the 16 current
  names/identities/scores and exclude the retired object. Both runs produce
  992 pose publications, 34,031,552 pose bytes and zero state queue drops. These
  are controlled single-process peers, not a 16-player gameplay load test.
- Dedicated Blood Arena and two independent processes complete Beretta combat:
  13 shots, ammo 37, one frag/death, 13 impulses and health-100 respawn. At frag
  delivery both clients verify the actual killer/victim names, character IDs and
  authoritative score rows. Native server: 2,400 ticks, zero discarded ticks,
  two admissions/departures; both clients produce PCM peak 1.
- Graphical Fields host plus two remote processes verify all three roster rows
  during actual lethal combat: 33 shots, ammo 17, one frag/death, 13 impulses and
  health-100 respawn. Host: 1,200 frames, 3,609 server ticks, **nine discarded
  server ticks**, zero local discarded ticks, three admissions/departures.
  Remote PCM peaks are 0.976562 and 1; host output is dummy. The inspected capture
  shows the existing Fields scene, held Beretta, crosshair and HUD. No reference
  image is replaced.
- Existing private HUD/events, weapon loadout and audio codec/playback pass.
  Five rebuilt solo oracles remain byte-identical to the preceding lot: effects
  240 frames, audio 360, movement 360, environment 720 and camera/HUD 300.
  Native/sanitized independent inbox and PvP executables contain no server
  session, adapter, shell, original player, inventory, weapon or object symbols.
  All **6,821 protected hashes** remain unchanged; PBR and historical evidence
  are excluded from edits. No commercial content is committed.

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json ctest --test-dir build/m9-fx-runtime -V -R '^(network\.(admission_codec|admission_private|player_events|server_player_events|roster_private|server_weapon_loadout|audio_codec|audio_playback|pvp_processes)|gpu\.network_pvp_host)$'
ctest --test-dir build/m9-fx-runtime-asan -V -R '^network\.(admission_codec|admission_private|player_events|server_player_events|roster_private|audio_codec|audio_playback|pvp_processes)$'
ctest --test-dir build/release -V -R '^network\.(admission_codec|player_events|roster_private|audio_codec|audio_playback)$'
```

The first native and sanitizer builds exposed the still-private original name
member; using the restored original accessor fixed compilation. Project builds
then produce no compiler warnings; the unchanged external effects-oracle helper
retains its indentation warning. Review also found the old private-codec header
expectation still at protocol 13; all affected golden headers now specify 15.

The first sanitized process test timed out during connection while compilation
and other test jobs were active. The isolated rerun passes in 43.34 seconds with
13 shots, one frag and health-100 respawn. The first failure remains retained.
NetworkClient currently includes synchronous content preparation
in its five-second connection deadline; slow preparation needs a separate fix.
This lot does not extend timeouts or claim this failure is repaired.

## Remaining scope

Original join/leave text, frag/FinishHim announcements, scoreboard ordering and
rendering, chat, match limits/rotation, remaining multiplayer branches, remote
visual effects, Team/CTF and Host/Join/LAN interfaces remain in progress. Full
22-map combat, 16-player gameplay load, impaired combat/audio, physical speakers,
human LAN, Windows/macOS interoperability and mobile/Quest execution are not
qualified by these checks. M9 remains active; M2-GPU-001/M3 stay separate.
