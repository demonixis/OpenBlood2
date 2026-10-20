# M2 — original jump voices and landing sounds

MoveMgr now invokes the original client Jump voice when a ground jump succeeds.
CPlayerObj tracks the original sampled fall range and plays the material-specific
landing sound after a qualifying contact. These use real decoded audio and the
existing VoiceMgr, attached character voice and AI hearing paths. Inspection shows
requests, actual starts, sampled height range, selected resources and failures.
M2 remains incomplete.

The implementation retains the original strict 56-unit threshold, Incorporeal
suppression, free-movement reset, raw foot-container precedence and separate client
jump/server landing ownership. It preserves ground jumps in shallow water and
silent swimming/ladder ascent. Unknown materials and missing WAVs remain explicit;
audio failure never cancels movement. The supported player snapshot preserves
falling state, relative start time and extrema with validation before mutation.

See [contracts](../../contracts/movement_audio_contracts.md) and
[source ledger](../sources/m2_movement_audio_sources.json).
No impact-force estimate, falling damage, new jump physics or game balance change
was introduced. Reference sources, dependency pins and commercial files remain
external and unchanged; the user's README edit is preserved.

## Verification

Implementation commit: `ecd2b769d4956da2bd5a37e94818fe01e691be0c` in the independent
Blood2 repository.

| Configuration | Targeted behavior/lifetime tests | NVIDIA GPU checks |
| --- | ---: | ---: |
| GCC Debug, inspector enabled | 115/115 (69.00 s) | 4/4 |
| GCC Release, inspector disabled | 115/115 (9.32 s) | 4/4 |
| Clang Debug, ASan/UBSan, inspector disabled | 115/115 (327.89 s) | Not run |

The five new movement-audio cases pass in all three configurations. After the broad
runs, their private assertions were strengthened to require actual decoded PCM
cursor advancement and retained nonzero audio identities through cleanup; those
five cases were rebuilt and rerun in each configuration. Production code did not
change. The suite also covers original objects, movement, locomotion, containers,
cameras, character/VoiceMgr audio, death, player field/checkpoint/KeepAlive state,
inventory and the continuous train-to-CityHub route at 30 and 60 Hz.

Private audio tests use real Caleb resources on a labelled diagnostic stone floor:
one jump voice and one landing sound at each of 30/60/120 Hz. The sampled spans are
75.0092 / 83.4683 / 87.8672 units; source frame sampling remains explicit. Both PCM
cursors advance, both retained handles are absent after player exit, and final
unload leaves zero object records and audio voices. Generated tests cover the exact
56/56.01 boundary, nine materials, hidden/raw water precedence, swimming and ladder
ascent, low ceilings, dead/blocked/Incorporeal and camera gates, and failed audio
without cancelled movement, repeated landings or leaked voices.

The four GPU regressions pass in Debug and Release on **NVIDIA GeForce RTX 3060
Laptop GPU / Vulkan**: SDL movement, character pose/audio, player restoration and
death effects. Movement continues with the UI hidden/folded and survives re-entry;
the character scenario verifies real sample submission. These are separate from
the new caller assertions. The ignored movement capture was inspected locally;
no commercial image or asset is committed.

[Machine-readable evidence](evidence/m2-movement-audio.json) includes commands,
configurations, source integrity and hashed logs. Broad results:
[Debug](evidence/m2-movement-audio-debug-tests.log),
[Release](evidence/m2-movement-audio-release-tests.log),
[ASan/UBSan](evidence/m2-movement-audio-sanitize-tests.log).
Final focused behavior:
[Debug](evidence/m2-movement-audio-debug-behavior.log),
[Release](evidence/m2-movement-audio-release-behavior.log),
[ASan/UBSan](evidence/m2-movement-audio-sanitize-behavior.log).
GPU: [Debug](evidence/m2-movement-audio-debug-gpu.log),
[Release](evidence/m2-movement-audio-release-gpu.log).

All four source selections/hashes and 12 changed C++ files' formatting were checked.
The reference inventory's 10,923 files and six independent reference repositories
remain unchanged. The user's workspace README edit is excluded from these commits.
Audio automation uses the dummy output driver. Sanitizer results are headless;
the previously documented SDL/libdbus GPU LeakSanitizer residual was not retested.

## Remaining gate

M2 remains incomplete. Contact-force/fall damage, the other original voice callers,
interactive IMA music and whole-world save/restore still need real implementation
and acceptance evidence. Existing replacement collision and frame integration are
not certified as retail physics. Native macOS/Windows and human audible playback
are not tested by this run.
