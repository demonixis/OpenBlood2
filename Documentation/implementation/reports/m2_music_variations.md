# M2 — Original IMA pattern channels and variations

M2 remains **incomplete**. All admitted pattern-header fields are now decoded,
original channel defaults are prepared, and a bounded function reproduces one
original variation decision. The in-game inspector shows authored and effective
channel values. No live sequence or MUSIC playback is claimed.

## Original evidence

The installed `IMUSIC25.DLL` copies the pattern header and supplies missing
variation flags at RVA `0x8748..0x8880`; mode defaults are applied by `0x81cd`.
The decision block `0x16e7e..0x16fd9` filters variants, fills an empty candidate
list with all variants, then applies random, cycle or held masks per channel.
Its local rand at `0x1b673` returns 0..32767. The portable function accepts these
draws explicitly, without copying the engine's global RNG or guessing scheduling.

The missing `0x0800` / `0x1000` flags are filled **per channel row**. A nonzero
play mode is never replaced. Random selection has precedence over cycling and
consumes a draw even with one eligible candidate. Cycling and holding ignore
the candidate filter; zero and multi-bit current masks retain original behavior.
See [contracts](../../contracts/music_variation_contracts.md) for
exact field offsets, signed values, atomic draw validation and ownership.

The [isolated probe](evidence/m2-music-variations-probe.py) runs the hash-pinned
external executable blocks in bounded x86 emulation, without native DLL
initialization or imported OS execution. It intercepts only rand calls with
explicit values. It checks three synthetic headers / eighteen decisions plus
all **131 installed patterns / 786 decisions**, across 39 styles. The original
outputs are hashed with raw header fields in source order. The independent C++
private test matches FNV64 `7572323464032181405`; the canonical SHA256 is
`e3f4cfb0badbdffc888b019611c4356f24fc21a8caa080befa8bfc33cdcb0f2a`.
[Probe results](evidence/m2-music-variations-probe.json) contain counts and hashes,
not commercial event payloads. Unicorn is optional evidence tooling only.
No DLL bytes, disassembly or commercial resources are imported into either repo.

## Integration

The bounded style reader retains the 573 remaining pattern bytes as typed
fields. Original authored values remain immutable, separately inspectable from
loader defaults. Preparation and selection operate on fixed arrays, with no
retained file spans, world handles, clock or external RNG. Bad draw counts/ranges
fail before output publication. Opening or hiding the UI never runs a live
musical decision. The existing sound smoke checks diagnostic decisions for all
loaded Cabal patterns with hidden steps/reload; lifetime remains world-owned.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 87/87 passed | 15.12 s |
| release | 87/87 passed | 1.43 s |
| sanitize | 87/87 passed | 68.53 s |
| debug NVIDIA Vulkan | 2/2 passed | 10.78 s |
| release NVIDIA Vulkan | 2/2 passed | 5.36 s |

The [validation ledger](evidence/m2-music-variations.json) records commands and raw
build/test output. The 87 cases cover variations and music readers, DLS synthesis,
catalogue/object/world lifetime, streaming/audio and configuration. Clang ASan/UBSan
includes leak checks. Both GPU tests verified NVIDIA device identity in each build;
Release runs without the inspector. Seven changed C++ files pass formatting;
10,923 reference files and six repositories remain unchanged. Local Markdown
links were checked. The sound capture shows music status; nested channel rows
were not captured. Dummy audio verifies submission rather than listening.
Native Metal, Windows and mobile execution were not tested.


The new tests cover exact signed field preservation, per-row missing-bit rules,
zero/nonzero mode behavior, high-bit requirements, empty/single candidate lists,
random-before-cycle precedence, draw order/count/range, zero/multi-bit cycles,
held ineligible masks, atomic output clearing and original private equivalence.
One initial synthetic assertion selected a positive byte while expecting a
negative one; the fixture check now uses its actual signed byte (-105). The
private original-equivalence test passed on that initial run as well.

The engine still needs actual pattern/band scheduling, chord/personality pitch
conversion, humanization, curves, MIDI/controller routing and playlist playback.
Missing style references and invalid effective DLS loops stay explicit. Required
MUSIC and other incomplete services keep the unchanged M2 gate open. The user's
README edits are preserved.

Implementation commit: `3fdd7cb4f16cbad309efb3a765f532654a541cd5`.
