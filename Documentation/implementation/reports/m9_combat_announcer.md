# M9 — Original combat announcements in independent clients

2026-10-31, following `10205fa9`. Independent network clients now consume the
existing frag, suicide and FinishHim events through original TheVoice selection
and CommLink. The local killer hears the announcement; other players receive
bounded text notices. FinishHim remains private to its addressed attacker.
Protocol 15, server event/RNG production and logical audio are unchanged.
See the [contract](../../contracts/multiplayer_contracts.md#independent-combat-announcements)
and [source ledger](../sources/m9_combat_announcer_sources.json).

## Source behavior and adaptations

All 216 source entries are imported: 184 kill, 24 suicide and eight humiliation
rows, with their original resource IDs, text, flags and 209 distinct filenames.
Source CP1252 fallback text becomes UTF-8. Preserve duplicate audio mappings,
including the same Voice241 for Humiliate Him/Her. Frag/suicide selection uses
any matching flag bit; humiliation alone checks both attacker and victim. The
original 16-bit index wrap and character numbering are retained.

Clients resolve exact object generations in their already-applied roster. Missing
names suppress the announcement. Local killer/suicide and FinishHim use CommLink's
existing busy refusal and delayed VoiceMgr interruption. Observers get text only:
four lines, oldest replacement, five seconds per line and expiry at equality.
The frontend draws those notices at the original (1, 0) coordinates and 16-pixel
line height using its portable UI font, replacing Windows Arial surfaces.

Local cres.dll strings take precedence. Bounded %s/%% formatting treats player
names as literal data; missing, oversized or malformed localized templates use
the exact source fallback and report a diagnostic. The shared Bloodbath startup
lookup and solo CommLink behavior stay unchanged. No server sound handles or
additional gameplay random draws reach this path.

## Verification

**19 distinct native tests, nine final ASan/UBSan tests and eight inspector-free
Release repeats pass.** Commands, initial failures and final outcomes are kept in
`evidence/m9_combat_announcer_*_2026-10-31.*`.

- Table tests cover 3,456 selections across every index and all four attacker/
  victim characters, wrapping indices, the two different flag tests, Unicode
  fallbacks and names containing percent directives. A separate audit compares
  every imported field against the original source. Generated PE resources test
  valid localization, malformed format directives, excess arguments/placeholders
  and the 128-byte limit. Invalid input preserves prior output.
- Three independent synthetic clients verify killer/suicide/FinishHim PCM,
  observer-only text, four-line capacity, exact expiry, duplicate sequences,
  stale identities, busy communication, delayed interruption and missing WAV
  diagnostics. Existing speech/capacity/startup tests still pass.
- Mounted retail strings require **zero fallbacks**. **173 distinct WAVs decode**;
  **36 original paths are absent**: `Sounds\\Voice\\Voice7.wav`, `Voice8.wav`,
  `Voice9.wav`, `Voice49.wav`, and `Voice68.wav` through `Voice99.wav` in that same
  directory. No replacement sound or alternate filename is invented. The resource
  audit permits these exact known absences, reports each one, and fails on any new
  missing path or malformed present sound. It decodes each present file in an
  independent cache; this is resource coverage, not simultaneous playback.
- Dedicated native Blood Arena with two actual clients: 2,400 ticks, zero discarded
  ticks, 13 Beretta shots, one frag, 13 impulses and health-100 respawn. Sanitized
  concurrent verification completes the same lifecycle with 14 shots and **46
  discarded server ticks** under compilation/test load. This is not load acceptance.
- Graphical Fields host plus two separate combatants: 1,200 frames, 3,610 server
  ticks, **nine discarded server ticks**, zero local discards, three admissions/
  departures, 33 shots, one frag and health-100 respawn. Both remote clients verify
  their distinct announcement routes and produce PCM peak 1 with two local speech
  starts. The final capture shows terrain/sky, held weapon, crosshair and HUD;
  notices have expired at that capture, so their raster appearance is not qualified.
- Five fresh solo traces are byte-identical: effects 240 frames, audio 360,
  movement 360, environment 720 and camera/HUD 300. Seven objectives cases plus
  credits-text and four-character profile tests pass after separating their shared
  pure PE writer from the server-backed fixture. The profile test's obsolete
  PlaySound call is updated to the already-established VoiceServices API.
- Native, sanitized and Release combat announcement executables contain no server
  gameplay symbols. All **6,839 protected hashes** remain unchanged, including PBR
  files, immutable reference captures and earlier evidence. No commercial assets
  are committed. CMoveMgr still integrates once; server logical audio is unchanged.

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json ctest --test-dir build/m9-fx-runtime -V -R '^(network\.(announcer|announcer_private|combat_announcer|announcer_formats|combat_speech|combat_announcer_private|speech|speech_capacity|pvp_processes)|gpu\.network_pvp_host)$'
ctest --test-dir build/m9-fx-runtime-asan -V -R '^network\.(announcer|announcer_private|combat_announcer|announcer_formats|combat_speech|combat_announcer_private|speech|speech_capacity|pvp_processes)$'
ctest --test-dir build/release -V -R '^network\.(announcer|announcer_private|combat_announcer|announcer_formats|combat_speech|combat_announcer_private|speech|speech_capacity)$'
ctest --test-dir build/m9-fx-runtime -V -R '^(objectives\.|menus\.(credits_text|profiles)$)'
```

The first independent test link exposed server inclusion in the string fixture;
its unchanged pure PE writers now have their own header. The first retail audit
incorrectly required all source sounds to exist, then reached the existing
128-clip cache limit while decoding the whole catalog in one cache. Both failures
are retained. The final audit explicitly accounts for the missing resources and
separates individual-file decoding from cache capacity. Production cache behavior
is unchanged in this lot: long sessions can still exhaust 128 distinct buffered/
streamed clips; bounded client cache reclamation is the next audio correction.

Application/network builds are warning-free. Rebuilt menu tests retain warnings
for enum/float comparisons and the malformed-PE fixture patch; the unchanged
external effects-oracle helper emits its existing indentation warning.

Communication portrait/text drawing, join/leave notices, chat/scoreboard, music,
remote effects, match limits/rotation and Team/CTF remain in progress. No physical
speaker, human LAN, impaired combat/audio, 22-map gameplay, 16-player load,
Windows/macOS interop or mobile/Quest qualification is added. M9 remains active;
M2-GPU-001 and M3 acceptance are separate.
