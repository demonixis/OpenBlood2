# Save-message primitives (save/load lot 1)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 1 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The
original `CSBase` calls that Save/Load bodies use now exist on bounded, owned
messages, together with a one-object `MID_SAVEOBJECT`/`MID_LOADOBJECT` payload
dispatch. Nothing writes or reads a save file and no game code calls the dispatch
yet; gameplay is unchanged. M2 closure and M2-GPU-001 are unchanged.

## What the original does

- `CSBase.h` declares the message set: Byte, Word, DWord, Float, String, Vector,
  CompVector, CompPosition, Rotation, HString, nested HMessageWrite/HMessageRead,
  FormattedHString, plain object references and the save-only
  `Write/ReadFromLoadSaveMessageObject`. It forbids plain object writes in
  `MID_SAVEOBJECT` and limits LoadSave references to save/load callbacks. Readers
  rewind automatically at the end of a message.
- Save bodies nest the client blob (`PlayerObj.cpp`), copy strings into fixed
  buffers without a size check (`corpse.cpp`), and use plain object references
  anyway (`PlayerObj.cpp` camera, `Naga.cpp` spike).
- The 1998 byte encoding is undocumented. The later runtime turns references into
  save-list indices and NULL for going-away or unlisted objects.

## Delivered behavior

- `legacy/server_de.h` declares the original set except CompVector, CompPosition and
  FormattedHString (effect-message only; a save body calling them does not
  compile), plus the OpenBlood2 `ReadFromMessageStringBuffer` checked copy.
- `game/server_messages.cpp` implements every call with the contract's encoding:
  little-endian integers, raw binary32 floats (NaN and -0 preserved), rotations as
  x, y, z, w, `u32`-length strings ≤ 4,096 bytes, nullable HStrings, nested
  messages ≤ 16 KiB and `0xFFFFFFFF` NULL references. A field is appended whole or
  not at all; the 64 KiB message bound is unchanged.
- `save_object_payload`/`load_object_payload` run one dispatch with a borrowed
  payload message and a `SaveIndex` table, and return a per-object report (class,
  bytes, consumed, references, NULL references, plain-object NULLs, nested
  messages, first error). Loads disable the end-of-message rewind and require exact
  consumption (D5).
- LoadSave calls on any other message fail, as do plain object calls outside saves
  (recorded as unavailable). Inside a save, plain references store NULL and are
  counted (D7); inside a load they must hold that NULL.
- Owned nested readers are read-only, count toward the live-message limit and are
  released only by `EndHMessageRead`. `EndHMessageWrite` and `EndMessage` refuse
  them. The service catalogue now lists the 20 new calls and marks
  `EndHMessageRead` as implemented.
- The message code moved from `server_adapter.cpp` to `server_messages.cpp`.
  Ordinary messages keep their behavior; `objects.messages_strings`,
  `message_churn` and `message_limit` pass unchanged.

## Tests

`save_message_tests` (labels `headless;objects;save`):

| Case | Checks |
| --- | --- |
| `golden_layout` | Exact bytes of every primitive in one save payload; dwParam 0 and 1 reach `fData`; report counters; no leaked messages or strings |
| `round_trip` | The payload loads in a fresh session; values, NaN/-0 bits, strings, NULL HString, nested reader, and references remapped through a new index table |
| `truncation` | Every strict prefix of the golden payload fails as an over-read with no leak; one trailing byte and an unread payload fail as under-reads; the first error latches; ordinary messages still rewind |
| `overflow` | 64 KiB payload bound (accepted at exactly 64 KiB, refused one field later), 4,096/4,097-byte strings, oversized and NUL-bearing string reads, the fixed-buffer copy, 16 KiB nested messages on write and read, an oversized load payload |
| `references` | Pending targets resolve until reclaimed, then fail; out-of-list indices fail; the NULL sentinel; null destinations; plain reads require NULL; a stale target saves as NULL |
| `context` | LoadSave and plain calls refused outside dispatch; a nested message inside a save cannot carry references and fails the save; the payload cannot be released, routed or written after dispatch; load payloads are read-only; nested, dead-object and unbound dispatches are refused |
| `nested` | Writer and reader nesting, forwarding a reader, wrong release calls, routed messages, nested readers inside a load, references refused inside nested bytes |

## Execution evidence

macOS (Apple Clang), from the lot-1 branch before merging lot 2:

- **Dev** (`dev` preset, private data configured): the new `save_messages.*` cases,
  `objects.*`, `object_snapshots.*` and `coverage.*` pass. The full headless label
  runs 1,220 tests; 15 fail, and the same 15 fail on an unmodified build of the
  parent commit `3434bd50`: weapons.inventory, locomotion.contacts, doors.catalog,
  pickups.catalog, ai_recovery.crouch/catalog, ai_escape.failures/catalog,
  weapon_drops.protocol, inventory_state.malformed, shield.failures,
  assault.collect/ai, mad_scientist.wounds and lightning.catalog. None involves the
  message layer. `coverage.observations` was updated, because `EndHMessageRead` is no
  longer an unavailable service.
- **Release** (inspector-free `release` preset): builds; the new, object and
  coverage cases pass (23/23); its headless run fails only the subset of the same
  pre-existing cases that does not need private data.
- **Clang ASan/UBSan** (`testing.md` recipe with `ASAN_OPTIONS=detect_leaks=0`,
  because LeakSanitizer is unsupported by Apple Clang): the object contract suite,
  object snapshots, `save_messages.*` and `coverage.*` pass (27/27). The full
  headless label reports the same 15 pre-existing failures, plus a timeout of the
  slow private `locomotion.cityhub_roam` under ASan, and no sanitizer report in any
  test output. Leaks are checked by the tests' live-message and string counts, not
  by LeakSanitizer.

## Limits

- The primitives are not the 1998 record layout; no original `.sav` interoperability
  is claimed.
- No purity check, engine record, index construction, `SaveObjects` or
  `RestoreObjects` exists yet (lot 3). The dispatch is tested only with a synthetic
  probe class.
- Ordinary (non-save) object references in messages remain unavailable; existing
  callers use typed messages.
