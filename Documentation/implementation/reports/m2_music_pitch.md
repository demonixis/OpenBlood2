# M2 — Original IMA harmonic note conversion

M2 remains **incomplete**. Note pitch now follows the original mode dispatcher,
chord/scale arithmetic, octave rules and channel ranges before the previously
implemented event preparation. The in-game inspector exposes explicit harmonic
inputs and the resulting note-on/off pair. These diagnostic inputs do not select
an actual song chord or enable MUSIC playback.

## Original behavior and bounded adaptation

The external installed `IMUSIC25.DLL` dispatcher at RVA `0x1714d..0x173ee` calls
the harmonic core at `0x11b82..0x11c7a`. The implementation preserves raw bypass,
mode inheritance, separate chord/scale roots, signed nibble/byte quirks, flags,
low-word arithmetic and upper-before-lower octave adjustment. The converted
pitch feeds `prepare_ima_note_events` without replacing its zero-pitch gate,
transpose or final octave fold. [Contracts](../../contracts/music_pitch_contracts.md)
record exact rules and ownership.

Mode 4 contains an original loop that can exhaust its scale while its countdown
remains nonzero, then never terminate. The portable function rejects that state
explicitly. Finite long walks, including negative signed countdowns with a
sign-extended mask, keep their original result using a bounded arithmetic shortcut.
No gameplay rules, RNG, world clocks, synth voices or authored records are changed.

## Executable comparison

The [probe](evidence/m2-music-pitch-probe.py) runs only the hash-pinned pitch blocks
in isolated x86 emulation, with explicit effective harmony, no native DLL loading,
OS imports or playback. Thirty-three fixtures cover modes, fallback, signed
roots, zero suppression, flag/range behavior, inverted bounds and long/infinite
scale walks. Another 2,048 generated cases match the original; 25 original
infinite walks become explicit refusals. Private comparison processes all
7,030 notes in 39 installed styles under three synthetic effective contexts:
21,090 decisions, no refusal. It does not infer runtime harmony from raw section
records or claim a complete-song comparison.

| Dataset | FNV64 | SHA256 |
| --- | --- | --- |
| Generated decisions | 15763711143549854140 | `d3cb6c9ec3f739f791b8dd6788e2c42f5132a937767e330c87aeab492faf6173` |
| Private explicit contexts | 809768487747410926 | `6bce959137f679868774a0eea2775deb9e59e4ccc6b50d4f7bb84fe499e15b62` |

[Probe results](evidence/m2-music-pitch-probe.json) retain only synthetic fixtures,
counts and hashes. No DLL bytes, disassembly or commercial asset payloads are
imported. Optional Unicorn is evidence tooling, not a project dependency.

## Integration and verification

The existing grid probe now accepts effective chord/scale masks, signed roots,
flags and the harmonic-mode gate, displaying converted pitch/mode or raw bypass.
Failed conversion visibly prevents event preparation. Manual resolved-pitch
inspection remains available. All stored UI controls are numeric; catalogues
and music state remain immutable. Sound GPU checks pass actual decoded Cabal
notes through conversion and event preparation with explicitly synthetic harmony,
including hidden stepping/reload and inspector-free Release.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 94/94 passed | 15.77 s |
| release | 94/94 passed | 1.51 s |
| sanitize | 94/94 passed | 63.29 s |
| debug NVIDIA Vulkan | 2/2 passed | 11.27 s |
| release NVIDIA Vulkan | 2/2 passed | 5.47 s |

The [validation ledger](evidence/m2-music-pitch.json) records exact commands and
raw build/test results. The 94 cases include original pitch observations, invalid
inputs and atomic clearing, private pitch-to-event integration, existing music
readers/synthesis and world/object/audio/configuration lifetime. Clang ASan/UBSan
runs with leak detection and halt-on-error. Both GPU cases report the NVIDIA
RTX 3060 Laptop GPU, in Debug and inspector-free Release. Five changed C++ files
pass formatting; 10,923 reference files and six repositories remain unchanged.
Local Markdown links pass, and the user's README remains untouched.

The captured sound inspector was reviewed; it shows music status, not the nested
pitch controls. Dummy audio tests submission rather than listening parity.
Native Metal, Windows and mobile execution were not tested.


The next music work is effective chord/personality reconciliation and selection,
pattern/band scheduling, curve/controller application and live note/playlist
ownership. Missing style bindings and invalid DLS loops remain explicit. Original
MUSIC and the other required services keep the M2 exit criteria unchanged.

Implementation commit: `835668e3afcc2a7ae35b0c0338241317edf9e767`.
