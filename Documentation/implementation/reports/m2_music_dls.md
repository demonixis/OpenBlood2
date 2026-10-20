# M2 — DLS instrument data prerequisite

M2 is **incomplete**. The original client needs its external DLS bank before
interactive music can play. This increment decodes that bank's sample and
instrument data; `MUSIC LEVEL 3` remains explicitly unsupported. It does not start
music or substitute a recorded soundtrack.

## Implemented

An independent bounded engine reader resolves DLS pool cues to actual sample
boundaries, decodes mono PCM into owned float samples, and preserves instrument
bank/program identifiers, ordered key/velocity regions, signed tuning/attenuation,
forward loops and raw articulation graphs. Region settings override wave defaults.
Overlapping regions and repeated/reordered pool cues remain distinct.

The catalog retains immutable decoded data once per world, separately from RIFF
validity and decoding errors. Frame updates and UI visibility do not reload it;
unload releases it. The World sounds inspector exposes instrument/sample counts,
region mappings/settings, raw graphs, all waves, memory usage and invalid defaults.
The existing sound GPU check now verifies the actual Cabal bank while the inspector
is hidden and after world reload.

See [contracts](../../contracts/music_dls_contracts.md) and
[format/source references](../sources/m2_music_dls_sources.json).
No reference engine or external decoder dependency was imported. Commercial data
stays external.

## Data findings

| Bank | Result | Instruments | Regions | Waves | PCM frames | Raw graph connections |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Ancient | Decoded | 12 | 106 | 55 | 1,131,051 | 21 |
| Bloat | Decoded | 8 | 30 | 28 | 1,073,503 | 26 |
| Cabal | Decoded, unused invalid default exposed | 14 | 38 | 35 | 913,403 | 44 |
| Crudux | Refused: effective region loop exceeds its sample | — | — | — | — | — |
| Track1 | Refused: effective region loop exceeds its sample | — | — | — | — | — |

All installed DLS waves have an 18-byte PCM format with `cbSize=18`. An initial
zero-only assumption rejected them; the corrected decoder follows Microsoft's
[PCM rule that ignores that field](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex),
while retaining exact format/chunk bounds. Synthetic tests include both ordinary
16-byte PCM and nonzero ignored `cbSize`.

Cabal wave 8 has 13,277 frames but a default loop starting at 7,000 with length
13,277. The actual regions override that default with valid settings. The raw
default remains visible and unusable; no sample is read out of bounds. Crudux's
program 2 uses a 664,633-frame loop in 64,633 frames, and Track1's program 4 uses
32,025 frames in 23,025. Those effective loops are refused with exact diagnostics.
No clamping or retail fallback is assumed. This is an observed file discrepancy,
not evidence of how the original executable handled it.

SubwayA and CityHub both use Cabal: 3,653,612 retained PCM bytes per loaded world,
35 waves and 38 regions. The three accepted banks total 3,117,957 decoded frames;
every sample was compared with an independent signed-16-bit calculation. The
rejected banks produce no partial public bank.

## Verification

| Configuration | Result | Wall time |
| --- | --- | --- |
| GCC Debug, inspector enabled | 62/62 passed | 43.02 s |
| GCC Release, inspector disabled | 62/62 passed | 6.91 s |
| Clang ASan/UBSan, leak detection enabled | 62/62 passed | 250.32 s |
| NVIDIA Vulkan, Debug | 3/3 passed | 38.27 s |
| NVIDIA Vulkan, Release | 3/3 passed | 10.09 s |
| Final Debug inspector readability check | 1/1 passed | 4.61 s |

These are selected cases, not the entire registered project suite. Commands,
per-case results, raw logs and limits are recorded in
[the evidence ledger](evidence/m2-music-dls.json).

Synthetic tests cover every truncation of a complete bank, PCM endpoints, signed
values, region override/inheritance/missing defaults, cue reordering/aliasing,
range/count/duplicate/format failures, invalid effective loops and transactional
sample-budget exhaustion. Private tests exercise all five banks, both worlds,
player entry, stable data through updates, unload and weak-pointer expiry.
Existing resource/object/session/service/audio/train-route cases guard integration.

Initial failed checks are retained in the evidence: PCM `cbSize` assumptions,
the distinction between invalid wave defaults and effective region loops, and
one intermediate old-library/new-test diagnostic-string mismatch during a rebuild.
The final checks use consistent rebuilt artifacts and retain the unsupported MUSIC
assertion. No assertion for successful playback or effective-loop bounds was removed.

Sanitizers cover headless execution; NVIDIA checks use Vulkan and dummy audio.
No human listening, native macOS/Windows/mobile execution or retail musical
fidelity is claimed. The user-edited workspace README and reference repositories
are preserved.

## Remaining work

Implement the required articulation/envelope/modulation behavior and bounded
synthesized voices, then decode the actual section/style musical events and
personality references and connect original timing/playlist transitions. Unknown
metadata and malformed effective loops need explicit treatment before broader
music support. This increment supplies data for those services; the M2 gate is
unchanged.

Implementation commit: `2b6b6a8fa842d28f920c5135e1dfbc1a32bc8d30`.
