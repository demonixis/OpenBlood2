# M2: continuous prepared-playlist audio

The original level-selected transition can now play once followed by a repeating
playlist. `AudioMixer` schedules the repetition independently of the inspector;
this is real instrument audio with bounded retained state. Game MUSIC activation,
live boundary switching and original variation regeneration remain incomplete.
The actual M2 exit criteria are unchanged.

Implementation commit: `29dc356bac79b43963795aaec995240decea08ac`.

## Implementation and source basis

- [Transport](../../../src/engine/ima_transport.cpp) owns two prepared numeric
  templates and an immutable instrument bank. It pumps whole passes before the
  existing audio block, preserving order, anticipations and overlapping note tails.
- [Clock](../../../src/engine/ima_clock.cpp) retains the fractional boundary
  time across repetitions. Two fixed maps replace an ever-growing tempo history.
- [Catalogue](../../../src/game/music_catalog.cpp) shares the existing bounded
  original-list preparation between finite and repeated auditions.
- [Inspector](../../../src/app/inspector_world_sounds.cpp) reports actual running
  state, submissions, queue size and failures, with start/stop controls.

The original [PlayList API](../../../../sources/blood2/AppHeaders/client_de.h) specifies
transition-before-list, ordered playback and list repetition. Original game-level
selection remains the previously verified `CMusic` decision. The clock arithmetic
continues the original runtime clock comparison; no new commercial binary is
executed or copied. See [contracts](../../contracts/music_transport_contracts.md)
and [provenance](../sources/m2_music_transport_sources.json).

The transport repeats **prepared** variations; its gain, seed and setup/preroll
remain explicit audition policies. It does not claim retail random-state,
activation/band lookahead or PCM/listening parity. Missing banks/resources still
fail. Live MUSIC message handling and NEXTSONG cancellation/replacement are the
next integration work, not completed by this transport.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 153/153 passed | 33.93 s |
| release-tests | 153/153 passed | 7.80 s |
| sanitize-tests | 153/153 passed | 136.31 s |
| debug-gpu | 2/2 passed | 52.18 s |
| release-gpu | 2/2 passed | 18.64 s |

The [validation ledger](evidence/m2-ima-loop.json) records the complete logs,
commands, frozen sources and private counters. All configurations agree on the
private note/PCM accounting. The compact clock matches twenty unrolled multi-tempo
passes; repeated PCM matches both a fully queued unrolled sequence and 17-frame
fragments exactly. Synthetic tails cross multiple sections and early setup crosses
repeat boundaries. Stop, invalid starts, bounded atomic failure, receiver failure,
zero steps and bank release pass.

The external Cabal test plays three passes of all four lists, including authored
duplicates, and continues SilenceList for thirty minutes with fixed template size.
It also advances after catalogue destruction. These are explicit development
playlists, not a live game music-message test.

NVIDIA sound checks play the actual SubwayA transition/list repeatedly through the
ordinary mixer, with hidden inspection and after reload. A running transport and
future MIDI note are left for teardown. The player keep-alive GPU scenario passes
in Debug and inspector-free Release too. The captured inspector shows real active
transport state. UI actions were exercised through their backend, not manually
clicked; dummy output is not a listening comparison.

The expanded sound smoke initially exceeded its 15-second application watchdog
(23.99 seconds including the in-flight audio work). Its frame/event assertions
remain intact; this scenario alone now has a 60-second application budget and a
90-second CTest budget to admit the extra complete playlist passes. The initial
failure log is retained alongside the passing final results.

Formatting, local Markdown targets and owned whitespace pass. The integrity check
preserves all 10923 reference files and six reference repositories. The user's
root README remains byte-identical. Release and Clang builds report five existing float/enum deprecation warnings
in unchanged PlayerVoice, ItemPickups and PlayerContainers code. The ledger retains
them; no warnings are suppressed to pass verification.


## Remaining scope

M2 remains incomplete. Continuous prepared playback removes the finite-duration
limitation, but original live client music requests, boundary switching and
cross-section variation state still need integration and verification. Other
required service coverage and the unchanged observable acceptance gate remain.

The next source path to connect is `CPlayerObj::ProcessTriggerMsg` -> server
`CMusic::HandleMusicMessage` -> `SMSG_MUSIC` -> client `CMusic::PlayMusicLevel`.
The existing `PlayerContainers.cpp` still records MUSIC as unsupported. Original
client world entry rebuilds playlists and requests the retained level; exit stops
music, destroys playlists and resets that level to silence. Preserve these source
lifetime rules when adding live switching; the inspector's preview selectors are
not the player's logical state.
