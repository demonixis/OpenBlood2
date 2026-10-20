# M2 — Original IMA chord preparation and personality metadata

M2 remains **incomplete**. Section chords now receive the original loader pitch
flags and post-meter variation classification, with their actual personality
scale metadata. The catalogue owns these prepared values and exposes successful
and failed bindings in-game. This closes the earlier gap between raw section
chords and explicit effective pitch inputs; it does not choose a live chord or
start the soundtrack. MUSIC remains unsupported.

## Original evidence and implementation

The installed `IMUSIC25.DLL` provides the loader flags at RVA `0x16654`, chord
reconciliation at `0x18dc3` and template classification at `0x18f15`. The section
loader derives its base root/flag from the header and scale from the personality.
Pitch conversion selects a nonzero chord scale override before the base scale.
The portable implementation preserves signed remainders, masked shifts, retained
source order, next-chord beat-zero conditions, separate roots and flag ordering.
[Contracts](../../contracts/music_harmony_contracts.md) describe the
exact rules, bounds and deliberate differences.

A new bounded REPs reader extracts scale and names from external `.PER` files.
The five installed files share the same header scale, 0x00ab5ab5. Pointer fields
and personality graph payloads remain opaque. Child envelopes are bounded, but
metadata success does not certify the nested graph. The catalogue's explicit
same-directory filename/name profile accepts zero personality identifiers and
refuses nonzero identities, mismatches, missing files, ambiguity and traversal.
Original asset requests are unchanged; dependencies are tracked separately and
share the existing byte budget. Nothing is read by the rendering loop.

## Executable comparison

The [probe](evidence/m2-music-harmony-probe.py) uses the hash-pinned external DLL
in bounded isolated x86 emulation. No native library loading, OS imports,
disassembly or commercial payloads are added to either repository. It executes
the original chord arithmetic and the personality header read/skip plus field
projection. Opaque graph traversal is deliberately bypassed and is not claimed
as tested. Optional Unicorn remains evidence tooling only.

| Dataset | Cases | FNV64 | SHA256 |
| --- | --- | --- | --- |
| Generated chord inputs | 4,096 | 12106312165928785022 | `c9429a640d80e7e0018a6f2ef4be1aa61f95d3245fc280cf1ba7a2bca14da2f5` |
| Installed section chords | 156 | 11061850450855354925 | `2e631dc91e08ead5586d39526db135eee7bac03944b674ff2ef50f7ffa881b9a` |

[Results](evidence/m2-music-harmony-probe.json) also record six synthetic examples
and five header observations. Standalone private chord tests supply explicit
4/4 meter; they are not a complete song comparison. Catalogue tests bind actual
Cabal styles for SubwayA and CityHub, retain immutable harmony while stepping,
and prove that old personality/harmony ownership expires on unload/reload.

## Inspection and verification

The World sounds inspector shows dependency counts, prepared/unresolved sections,
personality scale metadata, base flags, retained source indices/times and derived
pitch/variation flags. Activation and current-chord selection remain explicitly
unavailable. GPU sound checks convert diagnostic notes using actual prepared
chords, with the harmonic gate explicitly enabled only for that diagnostic call.
They run during hidden stepping and reload, including inspector-free Release.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 98/98 passed | 17.95 s |
| release | 98/98 passed | 1.41 s |
| sanitize | 98/98 passed | 68.60 s |
| debug NVIDIA Vulkan | 2/2 passed | 11.93 s |
| release NVIDIA Vulkan | 2/2 passed | 5.41 s |

The [validation ledger](evidence/m2-music-harmony.json) retains exact commands,
raw build/test results and the frozen source hashes. The 98 cases include original
chord observations, malformed metadata, failed bindings, retained-order behavior,
private catalogue lifetime and existing music/world/object/audio/configuration
checks. Clang ASan/UBSan uses leak detection and halt-on-error. NVIDIA sound and
player keep-alive tests pass in Debug and inspector-free Release. No build warnings
were reported. Nine changed C++ files pass formatting; 10,923 reference files and
six repositories are unchanged. Local documentation links pass. The user's README
is unchanged.

The reviewed sound capture shows 52 original requests, 45 original assets and
2,005,696 total bytes including one personality dependency. The summary reports
32 prepared sections and zero unresolved bindings, at the bottom of the viewport;
nested prepared-chord fields were not visually captured. Dummy audio verifies
submission rather than listening parity. Native Metal, Windows and mobile were
not exercised for this increment.


Next: connect current-chord and pattern/band selection to a bounded live sequence
clock, then controller/curve application, note ownership and playlist transitions.
Personality graphs, missing style references, invalid DLS loops and remaining
required services keep the actual M2 exit gate open. This increment is preparation
and inspection, not audible music parity.

Implementation commit: `08e3978f76c4de977a00e20497b41ff17f5fa00f`.
