# Save/load lot 2: container and fingerprint — 2026-10-20

Roadmap task 3.3, lot 2 of the [save/load contract](../../contracts/save_load_contracts.md#implementation-plan).
The encodings are recorded in the contract's
[version-1 encodings](../../contracts/save_load_contracts.md#version-1-encodings). The
[source ledger](../sources/save_load_container_sources.json) records that no original
or reference-tree code was used.

## What landed

- `src/engine/save_file.*`: the version-1 container. It writes and reads the header
  and the nine sections in their one fixed order, with a CRC-32 per section,
  section and field bounds, exact payload consumption, and the nested `AUTO`
  level-entry container. META, FPRT and CONS are fully typed. CLCK, OBJS, LINK and
  ATCH are bounded opaque payloads for lot 3. Every failure carries a category
  (file error, bound, truncated, format, version, CRC, section order,
  fingerprint) and a message.
- META quick read from bytes or from disk. It reads at most 4,140 bytes and still
  checks the declared file length.
- `src/engine/save_fingerprint.*`: the edition, the ordered mounts (role,
  normalized archive name, size, directory digest), the effective-index digest,
  the world DAT digest and the class schema digest. `check_fingerprint` refuses
  every difference (decisions 5 and 6) and lists all of them in one message: edition,
  world DAT, unregistered classes, payload versions, and added, removed or changed
  archives. It also reports a differing mount order or effective index.
- `src/engine/sha256.*`: portable FIPS 180-4 SHA-256 (in `blood2_startup`, beside
  the REZ reader).
- `Resources::mount` takes a mount role. `mount_game` marks overlays as mods. Each
  `Mount` records the archive size and the SHA-256 of the REZ header and directory
  blocks, hashed while the bounded index is read; no extra I/O and no whole-archive
  hashing.
- `src/engine/atomic_file.*`: `write_file_atomic` (unique sibling temporary,
  `SDL_RenamePath`, temporary removed on failure, optional stale-temporary
  cleanup for saves) and `read_file_bounded`. `save_menu_preferences` now uses it.
  The bytes and error messages are unchanged, and a new assertion pins the exact
  bytes.

Not in this lot: save locations and slot files (lot 6), the CLCK/OBJS/LINK/ATCH
contents and save-list dispatch (lot 3), and the registered save-support schema
(lot 4; `check_fingerprint` takes the build schema as a map).

## Tests

`save_file_tests`, labels `headless;save`, eleven cases:

| Case | Coverage |
| --- | --- |
| `digests` | SHA-256 FIPS vectors (empty, `abc`, two-block, one million `a`), random update splits, the CRC-32 check value |
| `golden` | Byte-exact layout of a minimal file against an independent encoder in the test; tags, versions, CONS bytes and nested AUTO framing of a full file |
| `roundtrip` | With and without AUTO; all save kinds and boundary values (255-byte name, 256 maximal console pairs, maximal CLCK); byte-identical re-encoding; META quick read from a prefix |
| `truncation` | Every prefix of both files fails (truncated once the header is present), also for the quick read; a length-patched file missing END |
| `bitflips` | Every single-bit flip of both files is refused (4,712 flips of the 589-byte file, 9,584 of the 1,198-byte file with AUTO) |
| `bounds` | Oversize section lengths, END with payload, oversize and overrunning strings, mount and console counts, inexact payload, the 64 MiB file bound; 36 invalid containers refused by the encoder with no output |
| `versions` | Container versions 2 and 0, a newer version in every section (CRC first, then version once the CRC is recomputed), a newer version inside AUTO |
| `order` | Swapped, unknown, duplicate and missing sections, bad counts, trailing bytes, bad magic, a slot inside AUTO, a level entry carrying AUTO |
| `fingerprint` | Directory digest against an independent hash, determinism, host-path independence of the index digest, added/removed/changed mods, mount order, index, world DAT, edition, unregistered and re-versioned classes, combined differences, missing world, invalid schema, demo edition |
| `atomic` | Atomic replacement, failure keeps the previous file and leaves no temporary, stale-temporary cleanup only on opt-in and only for the same file, bounded reads, save file write/read/quick read, refused encodes keep the file, truncated files on disk |
| `fuzz` | 20,000 deterministic mutations (byte edits, insertions, deletions, truncations, large lengths) of both files; every input is refused with a category, or round-trips byte for byte |

`menus.preferences` additionally checks the exact preference file bytes, the
unchanged directory error message and that no temporary is left behind.

## Results

Run on macOS (Apple clang) in this worktree:

- Dev (`dev` preset): full build; `ctest -L headless`: 876 of 884 passed, including
  all eleven `save_file.*` cases and `menus.preferences`.
- Inspector-free Release (`release` preset): full build; `ctest -L headless`: the same
  876 of 884.
- Clang ASan/UBSan (testing recipe, inspector and launcher off) on `save_file.*`,
  `menus.preferences` and `world.rez_*`: 14 of 14 passed. LeakSanitizer is not
  supported by Apple clang on macOS, so `detect_leaks=0`; leak detection was not
  run. The sanitizer found an aliasing bug in one test fixture
  (`vector::assign` from its own element), which is fixed. It found nothing in the codec.

The eight failures in both presets are `weapons.inventory`, `locomotion.contacts`,
`ai_recovery.crouch`, `ai_escape.failures`, `weapon_drops.protocol`,
`inventory_state.malformed`, `shield.failures` and `assault.collect`. They
fail identically in an existing Dev build of `db179d1c`, which predates this lot,
and pass in an older `610ead8` build. They are pre-existing and outside this lot.

No GPU, private-data or interactive validation was needed for this lot. M2
closure, M2-GPU-001 and the disabled Save/Load screens are unchanged.
