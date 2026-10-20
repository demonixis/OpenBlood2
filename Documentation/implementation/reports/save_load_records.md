# Engine records and the clock (save/load lot 3)

Date: 2026-10-20. Roadmap task 3.3 (M3), lot 3 of the
[save/load plan](../../contracts/save_load_contracts.md#implementation-plan). The rules
are recorded in the contract's
[implemented engine records](../../contracts/save_load_contracts.md#implemented-engine-records-lot-3);
the [source ledger](../sources/save_load_records_sources.json) lists the references
consulted. `SaveObjects` and `RestoreObjects` now work over an in-memory store, tested
with synthetic classes. Every original class keeps the Unsupported default, so no game
save or load exists yet and gameplay is unchanged. M2 closure and M2-GPU-001 are
unchanged.

## What the original does

- `CBloodServerShell::SaveGame` lists every `USRFLG_SAVEABLE` object, active then
  inactive, and calls `SaveObjects("Save\Current\Current.sav" or "Auto.sav", list, 0,
  SAVEGAMECONSOLE | SAVEPORTALS)`. `RestoreGame` calls `RestoreObjects(path, 0,
  RESTORETIME)` after a world load without its objects. Level changes use `Keep.sav`
  with dwParam 1 and no flags.
- The 1998 record layout is undocumented. The later runtime (corroboration only) saves
  the engine fields with a relative NextUpdate, restores in two passes
  (PRECREATE(3)/INITIALUPDATE(3)/engine overwrite, then every OnLoad), resolves
  references against the save list (NULL otherwise) and recreates interlinks
  idempotently after the Loads. It never rolls back a failed restore.

## Delivered behavior

- **Engine records** (`src/engine/save_objects.*`): bounded codecs for CLCK, OBJS, LINK
  and ATCH with the field set of the contract's engine record, plus a world model's
  authored basis rotation. Field rules are shared by capture, encode and decode; decoders
  change their output only on success.
- **Registry** (`src/engine/objects.*`): `ObjectRecord::creation` (creation order over
  reused slots), `restore_clock` on an empty idle registry, `create_restored` (saved
  delay in double precision unless precreate changed the float hint, world-model base
  shell at the saved basis with the saved rotation posed once), a pure-callback scope
  that refuses creation, removal, scheduling, links, attachments, stepping and nested
  dispatch, and the per-class save-support declaration (`ClassInfo::save`,
  `payload_version`, inherited AlwaysLoad).
- **Adapter** (`src/game/server_saves.cpp`): `save_objects`/`restore_objects` and the
  SDK thunks over `SaveStore` (Current, Auto, Keep), `restore_image_state` for the
  pre-object phase (CONS then CLCK), engine-record capture and overwrite (sky
  membership through `SkyState::restore`), reports with per-class D7 counts, and the
  purity gate in `trace_service` with its reviewed pure set. Message sends, activity,
  audio, effects, sky changes and gameplay random draws refuse during a save.
- `RESTOREOBJECTS_RESTORETIME`, `SAVEOBJECTS_SAVEGAMECONSOLE` and
  `SAVEOBJECTS_SAVEPORTALS` are declared with their 1998 values. The service catalogue
  marks `SaveObjects` and `RestoreObjects` as bounded.

## Tests

`save_objects_tests` (labels `headless;objects;save`):

| Case | Checks |
| --- | --- |
| `codec` | Golden bytes of CLCK, a record with every field nondefault, LINK and ATCH against an independent encoder; every prefix truncated; trailing bytes; each of 36 field rules refused on encode and on decode; non-0/1 booleans, count bounds, two local-client records, duplicate links, a child attached twice |
| `deadlines` | 200,000 random clocks and deadlines: the restored deadline yields the saved remaining delay exactly (12 rounded sums needed a neighbour); `restore_clock` refused with records, pending removals, invalid values or inside a callback |
| `roundtrip` | A model, sprite, world model (unrotated with a pushable dynamic body, and rotated), container, plain objects, an attached child and a client-associated player in nondefault states, saved and restored into a fresh session: clock, world-time origin, global force, generator sequence and console restored; precreate receives the saved fields with reason 3; every pass-1 call precedes every Load, links and attachments follow; every engine field checked; LoadSave references remapped or NULL; the rotated world model's posed shell bounds bit-identical; a save right after the load reproduces the OBJS, LINK, ATCH and CLCK bytes |
| `order` | Creation order over a reused slot, duplicates written once, pending and always-load (also inherited) objects skipped and their references NULL; Unsupported, NotSaved, versionless and original classes refuse the save by name and keep the previous image; a stale handle is refused |
| `dispatch` | dwParam 1 reaches save and load; D7 plain references NULL and counted per class; keep-alive restore into a populated world keeps its clock; initial update wins for flags and the delay while user flags are overwritten; forward references resolve during Loads; a link recreated by a Load is not duplicated; RESTORETIME with an advanced clock is refused |
| `phases` | The lot-4 order: CONS and CLCK restored on the empty registry, an always-load object created at the restored time, then RestoreObjects with RESTORETIME; refused once the clock has advanced or when objects already exist |
| `purity` | 20 impure services inside `MID_SAVEOBJECT` (creation, removal, scheduling, activity, messages, links, attachments, random draws, audio, effects, setters, console, rays, lists, nested saves) each fail the save and leave records, clock, generator, handles, strings, messages, voices and the stored image unchanged; strings, queries and nested messages stay usable without residue; consuming a pending owned reader (the client blob) is allowed |
| `failures` | Refusals before any change for unknown names, missing images, flags, pending removals, truncated or corrupt payloads, record-count and schema mismatches, a changed BSP binding and an unregistered class; rollback of every created object (including spawned children), the clock and console after a refused precreate, an under-read, an over-read and a link whose end was removed; a precreate that changes the delay hint keeps its own value |
| `store` | Logical names, dwParam and flag refusals, CONS bounds, console only with its flag, the SDK thunks over a shell `ObjectList` (released lists refused), the store surviving world unload |

`coverage.catalog`, `coverage.observations` and `coverage.private` now expect both
services to be bounded; the unavailable-request probes use plain object references
outside saves instead.

## Execution evidence

macOS (Apple clang), on the branch rebased onto `c7b2c06a`:

- **Dev** (`dev` preset, private data configured): all nine `save_objects.*` cases,
  `objects.*`, `object_snapshots.*`, `save_messages.*`, `save_file.*`, `containers.*`
  and `coverage.*` pass. The full headless label runs 1,240 tests; 7 fail:
  doors.catalog, pickups.catalog, ai_recovery.catalog, ai_escape.catalog, assault.ai,
  mad_scientist.wounds and lightning.catalog. All seven are in lot 1's pre-existing
  list (private-data content checks such as BSP topology and missing AI clips); the
  other eight from that list were fixed on main by `c7b2c06a`.
- **Release** (inspector-free `release` preset): builds; its headless run gives the same
  1,233 of 1,240 with the same seven failures.
- **Clang ASan/UBSan** (`testing.md` recipe with `ASAN_OPTIONS=detect_leaks=0`, because
  LeakSanitizer is unsupported by Apple clang): the object contract suite, object
  snapshots, save messages, records and container, coverage, containers and attachments
  pass (78/78) on the final commit. The full headless label (before the rebase and the
  last test additions) reports the 15 pre-existing failures plus the known ASan timeout
  of the slow private `locomotion.cityhub_roam`, and no sanitizer report in any test
  output. Leaks are checked by the tests' object, string and live-message counts, not by
  LeakSanitizer.
- Mutation checks: an inexact restored deadline fails `save_objects.deadlines`; dropping
  the velocity overwrite fails `save_objects.roundtrip`.

No GPU, interactive or private-data validation is specific to this lot. M2 closure,
M2-GPU-001 and the disabled Save/Load screens are unchanged.

## Limits

- No original class declares save support yet; lot 4 declares the registered classes,
  marks `USRFLG_SAVEABLE` and `CF_ALWAYSLOAD`, and adds restore-mode world loading and
  the candidate transaction.
- A failed restore removes every object it created and reinstates the clock and console.
  Effects of original callbacks on objects or adapter state that existed before the
  restore are left to lot 4's candidate destruction.
- Impure services outside the refused categories (setters, for example) fail the save
  but are not undone.
- The world-model shell is rebuilt, not saved: an unrotated model reuses its base, a
  rotated one is posed once at its saved transform. Collision results match within float
  rounding; this is replacement physics, not retail parity.
- The record layout is OpenBlood2's; no 1998 `.sav` compatibility is claimed.
