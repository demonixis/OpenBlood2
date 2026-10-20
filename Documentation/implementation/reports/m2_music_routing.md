# M2 — Original band channels routed to DLS voices

M2 remains **incomplete**. Selected IMA bands can now drive diagnostic notes through
the configured DLS instruments, with their authored volume and pan. This connects
band setup to actual PCM synthesis. The original soundtrack transport and MUSIC
service remain unavailable.

## Behavior

The DLS reader and IMA band reader retain distinct bank representations. The new
lookup reproduces the installed synth's packed instrument identity, exact patch
and inclusive key search, and percussion-only fallback to drum bank/program zero.
It never substitutes a melodic instrument when a requested program is absent.

The world-owned music catalogue prepares MIDI channel snapshots from the already
verified original setup messages, and shares the single configured decoded DLS
bank. Empty collection labels in Cabal sections do not prevent their legacy
program messages from selecting loaded instruments. The diagnostic treats the
bank's decoded instruments as resident; original download/reference tracking and
the OS GM collection are separate unfinished work.

Each note captures volume, expression and pan by value. Volume/expression use the
existing squared gain law; pan combines CC10 with the instrument's articulation.
Existing direct region auditions keep the same defaults. Silent controllers still
advance note clocks. Invalid controllers, missing routes and ambiguous packed
identifiers fail without changing other voices. The inspector displays these
values and offers selected-band channel auditions; no playlist starts from drawing.

See the [contracts](../../contracts/music_routing_contracts.md) for
bounds, ownership, compatibility adaptations and remaining unsupported services.

## Original comparison

A [bounded probe](evidence/m2-music-routing-probe.py) executes hash-pinned external
MSynth25 instructions for DLS locale conversion and the complete resident
instrument/key list lookup, including its caller's percussion fallback. The original
reset also confirms MIDI channel 9 as the default percussion channel. Only
critical-section imports are stubbed. No DLL entrypoint or host OS imports run;
no commercial executable bytes, samples or disassembly enter the repository.

| Observation | Result |
| --- | --- |
| Generated locale conversions | 512 |
| Patch/key/fallback cases | 66 |
| Original/portable decision FNV64 | `6386313400906083943` |
| Original decision SHA256 | `1e96e617d132022020f4aba966009d9bd6213ef8bc13dae8886d9c7b35d29f9a` |

Additional tests cover PCM controller endpoints, volume/expression multiplication,
constant-pan clamping, silent clocks, rejected-value atomicity, bank ownership,
channel snapshot independence and duplicate packed IDs. Private world tests exercise
every selected Cabal section in SubwayA and CityHub, with visible and discarded
PCM prefixes. Each world checks 510 routed notes across 34 selected sections, plus 34 explicit
missing-percussion routes. Source channel 15 uses legacy program 1 in these
sections, even though its raw custom program is zero; a focused test initially
assumed it was missing and was corrected against the private band bytes.

A separate ASan probe reproduced a use-after-free when a caller supplied a channel
reference from the sampler's own voice vector and note creation grew that vector.
The implementation now snapshots the channel before adding the voice. The permanent
routing test forces repeated growth while reusing a voice's channel; failed and
corrected probe logs are retained. Missing routes remain failures rather than
successful silent notes.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 110/110 passed | 16.52 s |
| release-tests | 110/110 passed | 1.61 s |
| sanitize-tests | 110/110 passed | 65.67 s |
| debug-gpu | 2/2 passed | 11.27 s |
| release-gpu | 2/2 passed | 3.77 s |

The [ledger](evidence/m2-music-routing.json) records commands, frozen source hashes,
probe results and raw logs. The [driver](evidence/m2-music-routing-validation.py)
builds relevant targets and runs the music, object/world, audio and configuration
suites. All nine changed C++/CMake hashes match their tested versions; eight C++
files pass formatting. The alias regression passes under ASan/UBSan after the
copy-before-growth correction. The initial full builds report five existing
`GameType` enum/float comparison deprecations in untouched original ports;
changed files and final incremental builds emit no warnings.

Both GPU suites use NVIDIA GeForce RTX 3060 Laptop Vulkan, including inspector-free
Release and hidden-inspector/reload checks. The reviewed Debug capture shows the
sound summary with two completed diagnostic notes; the nested routing controls
were not visually exercised. Dummy audio verifies PCM/submission, not a listening
comparison. Native Metal, Windows and mobile were not tested.

All 10,923 inventoried reference files and six reference repositories remain
unchanged. Local documentation links pass, and the user's root README is preserved.


Remaining work includes live tempo/activation, active-note scheduling, controller
curves, instrument residency, repeats and playlist/intensity transitions. The M2
exit gate is unchanged.

Implementation commit: `7431f3a33f094059a59f8f93fcda11ed9a5ab2c3`.
