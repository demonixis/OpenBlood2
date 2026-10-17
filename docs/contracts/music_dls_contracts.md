# DLS instrument data

Read [music asset contracts](music_asset_contracts.md) first. This extends the
original client's DLS instrument prerequisite with decoded data, **not music
playback**. `MUSIC LEVEL 3` still fails explicitly. No original game rule, music
state, RNG or clock is changed.

## Reader and representation

`read_dls_bank` is replacement engine code. It reuses the bounded music RIFF
reader and little-endian Reader; it never casts file bytes to platform SDK structs
or loads a DLL. Format facts come from Microsoft's published
[DLS header](https://raw.githubusercontent.com/microsoft/win32metadata/b07213e28bcc48221c155024f9c5e0e1a92c6497/generation/WinSDK/RecompiledIdlHeaders/shared/dls1.h)
and [WAVEFORMATEX documentation](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex).
No implementation or header is imported; see the
[source ledger](../implementation/sources/m2_music_dls_sources.json).

The admitted profile is embedded DLS1 `lins/ins /lrgn/rgn ` regions, `wvpl/wave`
samples, `ptbl` cues and `lart/art1` graphs. Required and optional singleton
records reject duplicates. Declared instrument/region/connection counts must
match their payloads. Cue offsets must identify actual wave-list boundaries,
relative to the start of the wave-pool contents. Cue order and aliases are
preserved: a cue index is not a physical wave index. Missing/null/external cues
fail. Duplicate bank/program pairs fail rather than picking an arbitrary entry.

PCM is mono, unsigned 8-bit or signed little-endian 16-bit, 8–96 kHz, with exact
block alignment, byte rate and nonempty complete sample frames. Conversion to
owned float samples preserves the source rate and uses 128/32768 divisors; no
resampling, pitch adjustment, gain or envelope is applied. `fmt ` must be exactly
16 or 18 bytes. PCM ignores `cbSize` per the Windows specification: all 156
installed DLS waves have an 18-byte format with `cbSize=18`. This field never
extends the bounded payload or admits compressed codecs.

Instrument bank/program values, authored region order/overlaps, inclusive key and
velocity ranges, key groups, option bits, phase group/channel, unity note, signed
fine tuning and raw signed attenuation remain owned values. Region `wsmp`
replaces the wave default; otherwise the region inherits the wave settings.
There must be a source of settings for every region. Supported sample loops are
single, nonempty, forward loops. An effective region loop must fit its linked
sample exactly; no truncation, wrapping or repair is invented.

**Unused invalid defaults:** a wave may retain an out-of-range default loop,
marked `loop_in_bounds=false`, if its regions supply valid overriding settings.
Consumers must use the validated region settings, never unconditionally play a
wave default. Cabal's wave 8 demonstrates this case: 13,277 frames, default start
7,000 / length 13,277, with valid region overrides. The inspector exposes the
invalid default. Structural/unsupported settings (bad sizes/type/zero length)
still fail, even if a region would override them.

Articulation graphs retain presence (including an authored empty graph), order,
source/controller/destination/transform and signed raw scale. They are **not
evaluated** or merged with default/instrument/region graphs. Unknown connection
codes remain inspectable values, not implemented modulation. Unknown chunks
(including names/INFO, `vers` and `msyn`) are counted as opaque. Other articulation
list entries, DLS2 regions, external waves and non-PCM samples are not supported.
Decoded data does not establish synthesizer compatibility for opaque features.

## Bounds and ownership

The underlying 32 MiB input, 8,192 chunk and 16-level recursion limits remain.
Additional caps are 256 instruments, 1,024 waves, 4,096 cues, 4,096 regions and
8,192 connections per bank; counts are checked before allocation. Decoded PCM
has a 32 MiB per-bank ceiling which callers may lower. Wave frames are checked
against the remaining budget before allocating. On any failure the public bank
is empty, with a diagnostic; no partial instrument graph or borrowed byte span
escapes. Synthetic validation exercises cumulative exhaustion on a later wave.

The existing catalog performs this decode once during world loading, retaining
`shared_ptr<const DlsBank>` beside the resource's structural result. All retained
banks together have a 64 MiB PCM ceiling, separate from the encoded-read budget.
Structural validity and decoding failure are separate fields: an invalid region
loop does not change a true RIFF result into a false claim of playable data.
Missing/rejected banks do not newly block world entry, since music playback
remains explicitly unavailable. No frame or inspector draw reloads or decodes
samples. Unload releases the catalog; a new world owns its own survey. Tests
verify release using weak ownership and stable sample addresses across updates.

The World sounds tab lists decoded instrument/wave counts and failures. Its DLS
tree exposes instrument and region mappings, raw articulation, effective settings,
all waves (including unreferenced ones), PCM sizes, pool offsets and invalid
wave defaults. This tree reads existing state; it cannot start a voice.

## Verified content and limits

Ancient, Bloat and Cabal decode: 34 instruments, 174 regions, 118 waves,
3,117,957 PCM frames and 91 raw articulation connections. Every decoded private
sample is compared with an independent direct signed-16-bit interpretation.
All 174 regions have explicit settings overrides. Two other banks are refused:
Crudux program 2 has a 664,633-frame loop in a 64,633-frame sample; Track1
program 4 has a 32,025-frame loop in a 23,025-frame sample. These are observed
file values, not verified retail handling. Keep them explicit until behavior is
evidenced; do not silently clamp to make tests pass.

SubwayA and CityHub retain Cabal's 14 instruments, 38 regions, 35 samples and
3,653,612 decoded PCM bytes. No music is emitted. Envelopes/modulation, voice
synthesis, musical events, personalities, scheduling, original playlist/state
transitions and the complete M2 gate remain open. NVIDIA tests do not establish
native macOS/Windows/mobile execution or audible fidelity.
