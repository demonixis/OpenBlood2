# M2 — Original IMA note event preparation

M2 remains **incomplete**. The original processing after harmonic pitch conversion
now prepares note-on/off events with signed timing/duration/velocity variation,
optional explicit percussion remapping, band transpose and octave folding. The
inspector exposes this as a diagnostic with explicit inputs. It does not play
music or claim that the missing harmonic conversion has been implemented.

## Evidence and behavior

The installed `IMUSIC25.DLL` note body at RVA `0x173ee..0x17595` checks zero pitch,
consumes range draws, applies transpose/remapping, folds pitch into range and
produces the paired messages. The type dispatch at `0x1712b..0x1714d` admits both
8 and 9 to this body. The preceding pitch conversion is deliberately outside
this increment; the API requires its resolved result explicitly.

The original treats range and velocity bytes as signed. Negative ranges therefore
have different jitter from unsigned ranges; range 1 still consumes a draw. Zero
resolved pitch suppresses everything before transpose. Source-channel-5 remapping
happens before transpose, while octave folds happen afterwards. The note-off
retains the note-on velocity. These details are reproduced rather than replaced
with conventional MIDI assumptions. [Contracts](../../contracts/music_event_contracts.md)
describe exact order, arithmetic and deliberate malformed-input refusals.

The [isolated original-code probe](evidence/m2-music-events-probe.py) executes the
hash-pinned external DLL blocks in bounded x86 emulation. Random returns, one
bounded allocation and outgoing note-on submission are intercepted; no native DLL
initialization, imported OS service or audio output runs. Sixteen synthetic cases
cover signed extremes, zero suppression, range order, octave wrap and mapping.
A separate synthetic observation records the legacy timestamp wrap, which the
portable implementation deliberately refuses.

The probe then processes all **7,030 installed notes** with two explicit contexts,
producing **14,060 decisions / 28,120 messages**. These use the raw-pitch branch,
synthetic grid times/transposes and an explicit synthetic mapping table. They
are not a comparison of complete songs or authored harmonic output. The portable
private test matches the original tail's canonical FNV64 `7927161452939007316`
and SHA256 `204e4697dd20800d384271823e355187b284acdf91a0c44e25ee36a6a9b21008`.
[Results](evidence/m2-music-events-probe.json) retain only counts, hashes and
synthetic outputs. No DLL, disassembly or commercial payloads are imported.
Unicorn remains optional evidence tooling rather than a project dependency.

## Integration and limits

The new function uses fixed value output and preflights types, channels, mapping
size/index, draw count/range and final tick bounds. Both event messages are
prepared together; failure clears the complete result. No global RNG, music clock,
file I/O, MIDI send or original game service is altered. Input maps are borrowed
for the call only, and returned events remain valid after those maps disappear.

A nonempty grid's inspector includes a probe with source-note selection and
explicit resolved pitch, tick, transpose and random values. It shows the prepared
events, suppression or error and labels them as unsent. The existing GPU sound
smoke checks the event helper against fixed diagnostic inputs for loaded Cabal
notes, during hidden stepping/reload and with the inspector absent in Release.
The diagnostic output is not treated as successful live music playback.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 90/90 passed | 16.23 s |
| release | 90/90 passed | 1.46 s |
| sanitize | 90/90 passed | 63.38 s |
| debug NVIDIA Vulkan | 2/2 passed | 10.77 s |
| release NVIDIA Vulkan | 2/2 passed | 3.71 s |

The [validation ledger](evidence/m2-music-events.json) records commands and raw
build/test results. The 90 cases cover note events and music data, DLS synthesis,
catalogue/object/world lifetime, streaming/audio and configuration. Clang ASan/UBSan
includes leak checks. Both GPU tests verify the NVIDIA device in each build;
Release runs without the inspector. Five changed C++ files pass formatting;
10,923 reference files and six repositories remain unchanged. Local Markdown
links were checked. The sound capture shows music status, but the nested event
probe widgets were not captured. Dummy audio verifies submission, not listening.
Native Metal, Windows and mobile execution were not tested.


Tests additionally cover unsupported type/channel, wrong map length, negative and
large mapping indexes, raw zero versus mapped zero, zero/signed velocities,
exact draw count/range, negative start times, minimum duration, tick boundaries
and atomic clearing. Existing music catalogue and world/object lifetime tests
remain required. The user's README changes are preserved.

Remaining music work includes actual harmonic pitch conversion, pattern/band
selection, scheduling/cancellation, curves, controllers and playlist playback.
Missing style references and invalid effective DLS loops remain explicit. Required
MUSIC and other services keep the unchanged M2 gate open.

Implementation commit: `37d34f571ce05f974514f16d704a6f1e18e96b9b`.
