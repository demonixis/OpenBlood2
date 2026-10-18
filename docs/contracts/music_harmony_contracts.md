# IMA personality metadata and prepared harmony

`read_ima_personality` reads bounded REPs metadata. `prepare_ima_harmony` combines
that metadata with a decoded section and its reconciled retained chord timeline.
The results supply effective pitch inputs and original variation flags. They do
not select a current chord, activate harmonic processing, decode the personality
graph, schedule a sequence or implement MUSIC.

## External metadata and binding

REPs has an eight-byte envelope with a big-endian payload length, then a
big-endian header length. The installed header is 1,214 bytes; the original
loader copies at most 1,164 and skips the remainder. The scale is a little-endian
word at header offset 16. Bounded terminated ASCII fields at offsets 20, 40 and
120 occupy 20, 80 and 20 bytes. Embedded pointer fields, chord templates and the
remaining header bytes are opaque. They are never dereferenced as host addresses.
Child records have four-byte tags and big-endian payload lengths, with no RIFF
padding. Their ranges are retained; nested NEC/PNS graph contents are not validated
or interpreted. Metadata success is explicitly not full personality decoding.

The portable reader requires exact envelope size, 1,164..65,536 header bytes,
a file at most 1 MiB, at most 256 top-level children and bounded printable strings.
Truncated headers, escaping child ranges, malformed strings and excess counts
fail atomically. The original's short-header/unbounded-string behavior is not
emulated. Zero masks and unknown child tags remain valid metadata.

The catalogue supports an explicit installed-file profile: zero section
personality identifier, a nonempty single-component personality label, and
`label.per` in the same music directory. Existing case-normalized exact-path
resolution rejects ambiguity, symlinks and traversal. The embedded name must
match the label after normalization. Nonzero identifiers fail: this is not the
original general UUID resolver. Dependencies are deduplicated separately from
the unchanged original playlist request list. At most 64 personalities share
the existing 64 MiB survey read budget; byte accounting includes these reads.
A missing timeline or personality leaves `harmony_error`, preserving authored
section data. No silent default scale or cross-directory lookup is introduced.

## Original preparation

- Base mask/scale come from personality metadata. Both base roots are section
  `default_chord & 127`; its high bit becomes the separately retained base flag
  (original runtime byte 27), copied to the section chords by reconciliation.
  This flag is not assumed to be the live harmonic-enable gate.
- Loader pitch flags count all 32 chord-mask bits. Start with 0x10; add 0x20 for
  more than three bits and 0x40 for mask bits in 0x003c0000. OR the entire byte
  count, without reducing it to a nibble. Authored chord flag 8 then adds 0x80.
- Use only the timeline's retained chords, in source order, including negative
  beats. Their source indices and times must agree with the section and effective
  meter. The successor used for classification is the next retained chord.
- Effective scale is the nonzero chord override, otherwise the personality base
  mask. Roots remain separate signed chord/base values. Reconciliation does not
  rewrite the authored mask, root or pitch flags.
- Classification starts with 0x100 for mask containing 0x91, otherwise 0x200
  for 0x89, otherwise 0x400. Walk scale bits through signed
  `(chord_root - base_root + 36) % 12`, count degrees from -1, and clamp to zero.
  An off-scale final bit adds 0x80; otherwise add the degree bit. Negative
  remainders skip the loop, retaining the original initial state.
- Only a next chord with beat zero contributes cadence flags. Walk the current
  effective scale through its root remainder, counting from zero; degree one
  adds 0x800 and degree five adds 0x1000 when on-scale. Do not use the next
  chord's override. Negative remainders preserve the prior off-scale state.
- Compatibility classification uses the base mask, ignoring the chord override.
  Rotate low 24 bits using signed root remainders and x86 shift-count masking.
  Twelve steps construct four templates from base degrees 1/3/5, with sixth,
  seventh or lowered seventh alternatives. A chord subset of any template adds
  0x2000; otherwise add 0x4000. Empty chords remain subsets. Preserve unusual
  negative-root behavior rather than replacing it with normalized music theory.

The typed result owns values and source indices. Inputs are immutable; no RNG,
callbacks, sound or clock advancement occurs. Both readers clear output on
failure. Preparation accepts at most 4,096 chords. Prepared pitch contexts keep
`enabled=false`; a later caller must make the actual activation/selection decision.

## Inspection, lifetime and evidence

World sounds shows personality dependencies, unresolved bindings, base scale/root
and flags, then each retained chord's source/time, masks, roots, pitch flags and
variation flags. This state exists independently of inspector visibility. Shared
immutable resources are released on catalogue replacement/unload; no UI control
retains pointers into them. The summary counts prepared and unresolved sections.

Original isolated executable observations cover 4,096 generated chord cases,
156 installed section chords and all five installed personality headers. Header
emulation executes the read/skip and field projection, deliberately bypassing
opaque graph traversal. The private standalone chord test uses an explicit 4/4
meter; catalogue tests use actual bound styles in SubwayA/CityHub. NVIDIA checks
explicitly activate prepared chords for diagnostic pitch conversion with the UI
hidden and after reload, including inspector-free Release. They do not certify
soundtrack playback or musical timing. See [evidence](../../../Documentation/Implementation/m2_music_harmony.md).

Current-chord/pattern/band selection, personality graphs, curves, MIDI/controller
application, live note ownership and playlist transitions remain incomplete.
Missing styles and invalid effective DLS loops remain failures. M2 stays open.
