# Save and load

Status: **roadmap task 3.3, complete (lot 14, 2026-10-21). Lots 1–14 are implemented; see their
evidence ([lot 1](../implementation/reports/save_load_messages.md),
[lot 2](../implementation/reports/save_load_container.md),
[lot 3](../implementation/reports/save_load_records.md),
[lot 4](../implementation/reports/save_load_session.md),
[lot 5](../implementation/reports/save_load_player.md),
[lot 6](../implementation/reports/save_load_slots.md),
[lot 7](../implementation/reports/save_load_world_logic.md),
[lot 8](../implementation/reports/save_load_movers.md),
[lot 9](../implementation/reports/save_load_pickups.md),
[lot 10](../implementation/reports/save_load_effects.md),
[lot 11](../implementation/reports/save_load_ai.md),
[lot 12](../implementation/reports/save_load_bosses.md),
[lot 13](../implementation/reports/save_load_native.md),
[lot 14](../implementation/reports/save_load_certification.md)). Every class the shipped worlds
place is Original, AlwaysLoad or NotSaved. All 36 single-player worlds save and load whole at
difficulties 0, 1 and 2, at entry and after combat, through the slot files that the Save and
Load menus, F5/F6 and Restart use ([lot 14](#certified-whole-worlds-lot-14)). Multiplayer and CTF saves
stay out of scope and are refused. Lot 15 (NEW_LEVEL on `SaveObjects`) is optional and not
started.** The
[audit](../implementation/save_load_audit/readme.md) is the evidence for every
original rule cited here; its [source ledger](../implementation/sources/save_load_audit_sources.json)
records the audited files. Read [level replacement](level_change_contracts.md),
[player restoration](player_restore_contracts.md), [object boundary](object_contracts.md),
[message lifetime](message_lifetime_contracts.md) and [destination rehearsal](destination_trial_contracts.md)
before implementing any lot.

## Scope

In scope: single-player games of type SINGLE, ACTION and CUSTOM on the retail
edition, with the original eight slots, the Quicksave/Quickload pair and the
Restart row. A save is a whole world plus the player and the client data the
original carried.

Out of scope:

- **Reading or writing original `.sav` files.** The 1998 engine record layout is
  undocumented: the retail `Auto.sav` header starts with 1113, while the later
  runtime writes 2001/2002. Retail 2.1 already declared earlier saves
  incompatible. Original saves also hold raw engine state that OpenBlood2's
  replacement physics does not share.
- Multiplayer and CTF saves. The original disables the menu save item and the
  F5/F6 keys there; OpenBlood2 refuses all of them.
- Saves in the demo edition (the original compiles them out; see
  [decisions](#decisions)).
- Nightmares content.
- Converting the existing NEW_LEVEL transfer to the save path before it is
  proven equivalent (lot 15).

## Original behavior to preserve

These rules come from the original shells and the later LithTech runtime. They
define what "restored" means; the [departures](#deliberate-departures) list every
place OpenBlood2 differs.

1. **Saved set.** A save covers every live object with `USRFLG_SAVEABLE`, active
   or inactive, players included. Characters set the flag in their non-savegame
   initial update. 25 original files set it, and subclasses inherit it. The
   engine never saves `CF_ALWAYSLOAD` classes or objects already going away.
2. **World without its objects.** A restore loads the world's geometry and only
   the `CF_ALWAYSLOAD` classes from the DAT. Those are GameStartPoint, PathPoint,
   Key, SmellHint, ExitHint, WorldProperties, CameraSpot, Script, ParticleSystem,
   PolyGrid, SparksObj, LightFX, WreckingBall, SoccerGoal, and the engine's Model,
   WorldModel, Container, Sprite, Sound, DemoSkyWorldModel, SkyPointer and
   FogSphere. They come back in their authored state.
   - Every other object comes from the save or does not exist.
   - Objects destroyed before the save stay gone.
   - A built-in `Sound` replays its loop.
   - Keys a KeyFramer consumed reappear, but the restored KeyFramer does not
     consume them again.
3. **Two passes.** Pass 1 runs for each record, in order:
   1. `MID_PRECREATE` with `PRECREATE_SAVEGAME` (3). Properties are not read.
   2. `MID_INITIALUPDATE` with `INITIALUPDATE_SAVEGAME` (3).
   3. The engine overwrites user flags, dims, colour, physics, animation,
      activity and position. Flags, the next-update delay and the name keep
      whatever initial update set.

   Pass 2 starts only when every object exists: `MID_LOADOBJECT` for each record
   in the same order. Interlinks and attachments are restored last. Original
   code depends on this order; for example, `CInventoryMgr::Load` first discards
   what initial update created.
4. **References.** `WriteToLoadSaveMessageObject` stores the target's index in
   the save list. A target outside the list reads back as NULL; that includes
   always-load objects, non-saveable objects and, in a keep-alive save, anything
   but players. Plain `WriteToMessageObject` is forbidden inside
   `MID_SAVEOBJECT`. Naga's spike and the player's camera use it anyway.
5. **Clock.** A full restore restores the game clock
   (`RESTOREOBJECTS_RESTORETIME`), so absolute deadlines saved raw stay valid. A
   keep-alive restore keeps the new world's clock, and the player's times are
   relative or durations. The next-update delay is a relative countdown.
6. **dwParam.** It is 0 for a full save and 1 for keep-alive, passed as `fData`
   to `MID_SAVEOBJECT`/`MID_LOADOBJECT`. Only `CInventoryMgr::Load` reads it:
   1 drops keys.
7. **Byte order within an object.** The most derived class writes first, then
   its parents, then the aggregates. Aggregates run in reverse order of addition;
   an NPC writes subclass, AI_Mgr, CBaseCharacter (0 bytes), CMovement,
   CInventoryMgr, CDestructable. The layout follows the **original** class
   chain, not OpenBlood2's C++ parents:
   - Thief and TheHand derive from BoneLeech in the port, but the original does
     not write BoneLeech's block twice.
   - The original KeyPickup also writes PickupObject's block.
   - SeeingEye does not write CProjectile's fields.

   See the [class catalog](../implementation/save_load_audit/class_catalog.md#3g-openblood2-hierarchy-divergences-relevant-to-chained-saveload).
8. **Sounds and client effects.**
   - Sounds are never saved. A class's Load or initial update restarts its own
     loops (SoundFX, TripLaser, DrudgePriest, DeathShroud); every other voice is
     lost.
   - Persistent client effects are rebuilt from the object's saved effect
     message.
   - One-shot effects are lost.
9. **Client data.**
   - The client's `SaveClientData` blob travels inside the player's record. It
     holds night goggles (discarded on load), wonky, blind and fade state, the
     music level, the last load type (discarded), the game type and the
     objectives title and text.
   - `CPlayerObj::Load` re-broadcasts it as `SMSG_CLIENTSAVEDATA`.
   - The restored player is reattached through its client reference.
   - It is not sent to a start point. The server sends `SMSG_FORCEROTATION` and
     `SMSG_EYELEVEL` instead, and `CMSG_SINGLEPLAYER_INIT` is not re-sent.
10. **Shell variables after a restore.** `GAME_GOTOSTARTPOINT`, `GAME_REVISITING`
    and `GAME_DOAUTOSAVE` are forced to 0, overriding the restored console.
    Difficulty and game type come from the saved console, not from the slot
    metadata.
11. **Restart.** Restart restores the level-entry snapshot (`Auto.sav`). It is
    taken after single-player initialization of every NEW_GAME and NEW_LEVEL
    entry, never after a restore.
    - It travels with each slot, so after loading slot N, Restart returns to the
      start of slot N's level with the inventory carried into it.
    - New Game clears it.
12. **Original losses.** A faithful port keeps the per-class losses and quirks of
    [characters.md §23](../implementation/save_load_audit/characters.md#23-cross-cutting-findings)
    and [world_objects.md §27](../implementation/save_load_audit/world_objects.md#27-cross-cutting-summary).
    Each one is either preserved or fixed through a recorded departure (decision
    4 below).

## Deliberate departures

| # | Original | OpenBlood2 |
| --- | --- | --- |
| D1 | Engine writes `Save\Current\*.sav`; slots are directory copies | One self-contained file per slot, written by temporary file and atomic rename; the previous file survives any failure |
| D2 | Load copies the slot over `Save\Current` before knowing whether it works | Nothing on disk changes until the load has been promoted |
| D3 | A failed save sends no ACK and soft-locks the client; restore results are ignored; file errors are silent | Every failure is reported to the player and logged; the game continues |
| D4 | A failed restore runs the world with only always-load objects and a fresh player at the origin | A failed load leaves the previous session (or the menu) untouched; there is never a partial world |
| D5 | Over- or under-read payloads go undetected | Every `MID_LOADOBJECT` must consume exactly its payload |
| D6 | Without a matching client reference, a fresh default player is created | Exactly one saved local player is required, otherwise the load fails |
| D7 | Plain object writes in a save store a runtime ID (later runtime; 1998 unverified) that is stale after a restore | They write NULL and are counted per class |
| D8 | Save-list order comes from the engine's object lists (unknown for 1998) | Records follow object creation order; see [ordering](#save-order) |
| D9 | LoadWorld resets the clock to 0 and creates always-load objects; the restore then jumps the clock (later runtime) | The saved clock is set first, so always-load objects start from it |
| D10 | Gameplay randomness is the unsaved CRT `rand()` | The server gameplay generator state is saved |
| D11 | No version or data check | Versioned container, game-data and mod fingerprint, per-class payload versions |
| D12 | F2/F3 and the server handlers have no multiplayer guard | Saving and loading are refused outside single-player game types |
| D13 | A restored model keeps the file precreate chose; the later runtime applies the saved animation trackers to it | The engine record's model and skin replace it before the animation track is checked and applied. CPlayerObj's precreate forces `Caleb.abc`, so another character would otherwise lose or refuse its track ([lot 5](#implemented-player-and-client-lot-5)) |
| D14 | A failed autosave leaves the previous `Auto.sav`, so Restart can reach an earlier level | A refused level-entry snapshot removes `current.ob2sav` and the in-memory snapshot; Restart is then empty ([lot 6](#implemented-slots-quick-and-restart-lot-6)) |
| D15 | CProjectile never saves `m_hFiredFrom`; a restored projectile has no shooter and the player loses its remote-charge list | The CProjectile block ends with the shooter as a LoadSave reference; a restored remote charge rejoins its player's list ([lot 9](#implemented-pickups-and-projectiles-lot-9)) |

## Save file format

A save is one little-endian file of at most 64 MiB. Readers check every length
against both the remaining bytes and a fixed limit before allocating. Strings
carry a `u32` length and no terminator, may not contain NUL, and are not
re-encoded. Floats are stored as IEEE-754 binary32 or binary64 bit patterns.
No section depends on host structure layout, pointer size or byte order.

```
header   magic "OB2SAVE" 0x1A | u32 container version (1) | u32 section count
         | u64 total file length
section  u32 tag | u32 section version | u64 payload length | payload | u32 CRC-32 (tag..payload)
order    META FPRT CONS CLCK OBJS LINK ATCH [AUTO] END
```

Container version 1 accepts exactly this section order. An unknown tag, a
missing section, a duplicate, a newer version or any trailing byte is a refusal.
There is no forward-compatible section skipping. The `AUTO` section is the
level-entry snapshot; it is itself a complete container without its own `AUTO`.

| Section | Content and bounds |
| --- | --- |
| META | Save kind (slot 0–7, quick, level entry). Display name (world title plus the original `[Mmm dd hh:mm:ss]` suffix, ≤ 255 bytes). UTC seconds. World path (the normalized DAT path). World title. Game type, character, difficulty. OpenBlood2 version string. Simulated seconds. The menu reads only the header and META. |
| FPRT | Edition. Ordered mounts: role, normalized archive name, size, SHA-256 of the archive's directory block. SHA-256 of the effective resource index. SHA-256 of the world DAT bytes. Class schema: (class name, payload version) for every class present in OBJS. |
| CONS | Game console variables (≤ 256 pairs, names ≤ 64 bytes, values ≤ 1,024 bytes): difficulty, game type, world name, world properties. Restored before any object is created. |
| CLCK | Simulation clock and frame time (binary64), world-time origin, global force, server gameplay generator state. |
| OBJS | Record count (≤ 65,536). Each record: class name, engine record, then the game payload (`u32` length ≤ 64 KiB, the existing message bound). |
| LINK | Interlinks whose two ends are both saved: (owner index, target index), in link-creation order. |
| ATCH | Attachments whose parent and child are both saved: parent and child indices, node name (the 1998 API attaches by name), offset, rotation. |
| AUTO | Optional nested level-entry container (≤ 32 MiB). |
| END | Empty; the CRC covers the tag. |

### Engine record

The engine record is OpenBlood2's equivalent of the LithTech per-object fields,
mapped onto `ObjectRecord`:

- object type, flags, user flags;
- position, rotation and scale;
- name, filename and skin;
- container code;
- for world and container models, the BSP name and the resolved model index (it
  must equal the first same-named model in file order), plus the authored basis
  rotation the base shell is built with (a container's equals its rotation);
- colour and alpha, collision dimensions;
- velocity, acceleration, mass, friction, the body properties and blocking
  priority;
- activity, automatic deactivation and the remaining time to deactivation;
- the remaining next-update delay (or "unscheduled");
- the animation track and hidden nodes;
- the sprite track;
- the sky slot;
- the local-client flag (OpenBlood2's equivalent of the `CIF_LOCAL` client
  reference).

Derived state is not saved and is rebuilt:

- posed world-model shells, container shapes and the container index;
- contacts and support;
- collision work counters;
- every inspection counter.

Positions must be finite and rotations unit within the existing ±0.001
tolerance; delays must be finite and nonnegative.

### Implemented engine records (lot 3)

`src/engine/save_objects.*` encodes the CLCK, OBJS, LINK and ATCH payloads;
`src/game/server_saves.cpp` implements `SaveObjects`, `RestoreObjects` and the
engine-record capture and application.

- **Encodings.** Little-endian; floats are raw binary32/binary64 bits; booleans are one
  byte, exactly 0 or 1; strings are a `u32` length without terminator.
  - CLCK (40 bytes): binary64 time, frame time and world-time origin, 3 × binary32
    global force, `u32` generator state (1–2147483646). The origin lies in 0..time.
  - OBJS: `u32` count (≤ 65,536), then per record: class name (1–64), `u16` type
    (OT_NORMAL, MODEL, WORLDMODEL, SPRITE, LIGHT or CONTAINER), `u32` flags and user
    flags, position, rotation (x, y, z, w), scale, name, filename and skin (≤ 100 each),
    `u16` container code, `u32` BSP model index (`0xFFFFFFFF` unless a world or container
    model) and its basis rotation (identity otherwise), colour and alpha (0..1),
    dimensions (≥ 0), velocity, acceleration, `u8` body kind (world, character or model,
    at most one), binary32 mass and friction, pushable, rotating-debris and
    polygon-bounds bytes, `u8` blocking priority, `u32` activity (with state 3 and
    automatic deactivation needing a positive period), automatic-deactivation byte,
    binary64 period (≤ 10⁶ s) and remaining time to deactivation, scheduled byte,
    binary64 remaining update delay (zero when unscheduled; both delays ≤ 10⁹ s),
    `u32` animation, binary64 animation milliseconds, looping, done and first bytes,
    hidden nodes (`u32` count ≤ 4,096 of 0/1 bytes), binary64 sprite milliseconds,
    `u32` sprite frame (< 4,096), has-frame byte, INSKY byte, `u32` sky slot (0–29, or
    `0xFFFFFFFF`; a slot needs INSKY), local-client byte (at most one record), then the
    game payload (`u32` length ≤ 64 KiB).
  - LINK: `u32` count, then owner and target indices; distinct ends, no duplicate.
  - ATCH: `u32` count, then parent and child indices, node name (≤ 100), offset and
    rotation; a child is attached at most once.

  Each decoder checks every length against its bound and the remaining bytes, rejects
  trailing bytes and changes its output only on success.
- **Delays.** A deadline is saved as `max(0, deadline − clock)`: an update that fell due
  while its object was inactive is saved as zero and runs at the next active step,
  as before. The restored deadline is chosen among `clock + remaining` and its nearest
  neighbours so that it yields the same remaining delay again; a save taken right after
  a full restore therefore reproduces the bytes. `MID_PRECREATE(3)` receives the
  delay as the float `m_NextUpdate`; the engine keeps the double value unless precreate
  changed that float, and a `SetNextUpdate` in initial update still wins.
- **Clock.** `Objects::restore_clock` works only on an empty, idle registry, so no
  deadline is rebased. `restore_image_state` applies CONS, then CLCK, before any object
  exists; lot 4 runs it after the restore-mode load's world-time reset and before the
  first always-load object. `RestoreObjects` runs it itself on an empty registry. With
  objects present, `RESTOREOBJECTS_RESTORETIME` requires the saved clock to be current
  (restored and not advanced); without it the clock is kept. With objects present the saved
  CONS is applied again, so it wins over what the always-load objects set
  ([lot 4](#implemented-restore-mode-world-and-transaction-lot-4)).
- **Logical store.** The original paths `Save\Current\Current.sav`, `Auto.sav` and
  `Keep.sav` (case and separator insensitive) name the three images of the adapter's
  `SaveStore`. An image holds dwParam, flags, CONS (only with
  `SAVEOBJECTS_SAVEGAMECONSOLE`, within the CONS bounds), the class schema and the four
  encoded payloads. It is replaced whole on success and kept on any failure, and it
  survives world unload. `SAVEOBJECTS_SAVEPORTALS` is accepted and has nothing to save.
  dwParam is 0 or 1; other flags are refused.
- **Save list.** `SaveObjects` writes each listed Alive object once, in creation order
  (`ObjectRecord::creation`; handles are append-only, so this also orders reused
  slots). It skips pending objects and AlwaysLoad classes, including subclasses. A
  stale handle, or a listed NotSaved, Unsupported or versionless Original class,
  refuses the whole save before any dispatch and names the classes: a listed NotSaved
  object would otherwise lose state silently. Links and attachments whose ends are both
  listed follow in link-creation and attachment-creation order.
- **Save support.** `ClassInfo::save` and `payload_version` hold the declaration;
  AlwaysLoad is inherited like `CF_ALWAYSLOAD`. Every class defaults to Unsupported;
  lot 4 declared the registered classes
  ([lot 4](#implemented-restore-mode-world-and-transaction-lot-4)).
- **Purity.** While `MID_SAVEOBJECT` runs, the registry refuses creation, removal,
  scheduling, links, attachments, stepping and nested message dispatch, and
  `trace_service` admits only a reviewed set: the message primitives, strings and
  read-only queries. Every other service, including an unknown name, fails the save.
  Message sends, activity changes, audio, effect publication, sky changes and
  gameplay random draws also refuse to act; any other impure call (a setter, for
  example) still fails the save but is not undone. After the last record, the record
  count, handles, links, attachments, strings, lists, voices, clock and generator must
  be unchanged and live messages must not grow, or the save fails. Releasing an owned
  reader stays allowed: the original player Save consumes its pending client blob. Attachment refresh during a position
  query stays allowed: it recomputes the same pose.
- **Restore.** The whole image is decoded and checked before anything changes: every
  class registered, Original, with the schema's payload version; the schema naming
  exactly the classes present; world and container models binding the same first
  same-named model in this world; a container's basis equal to its rotation; no sky slot
  held twice. Then pass 1 (`Objects::create_restored`, the engine overwrite), the index
  table, pass 2 and links and attachments, as in the [load sequence](#load-sequence).
  The overwrite covers the model and skin of a model object (D13, lot 5), user flags,
  dimensions, colour, physics, animation and hidden nodes (checked against the restored
  model), the sprite track (checked against the restored sprite), activity and
  deactivation, position and sky membership.
  - A world model's base shell is rebuilt at the saved basis; a saved rotation is posed
    once about the BSP origin, before initial update, and the saved translation is then
    the usual offset. A container must stay at its BSP origin.
  - The local-client record is reported, not associated: `load_game` attaches the client
    ([lot 5](#implemented-player-and-client-lot-5)).
  - An existing link, or an existing attachment of the same parent and child, is kept.
    Any other attachment goes through the engine's attachment rules.
  - Deferred removals requested by Loads stay for the owner's frame end.
- **Restore failure.** No deferred removal may be pending when a restore starts. On a
  failure after objects exist, every record the restore created is removed and
  reclaimed, including children its callbacks created (bounded rounds). The clock,
  generator, global force, world-time origin and console variables are reinstated if
  this call changed them. Effects of original callbacks on objects or adapter state
  that existed before (a detached voice, a removed always-load object) are covered by
  lot 4's candidate destruction, not here.
- **Services.** `SaveObjects` and `RestoreObjects` are bounded services; a refusal is
  an object diagnostic and a report (`last_save`, `last_restore`), not an unavailable
  service.

### Message primitives

Game payloads use the original `CSBase` calls on a bounded writer. Each call has
one encoding:

| Call | Encoding |
| --- | --- |
| Byte / Word / DWord | u8 / u16 / u32 |
| Float | binary32 bits |
| Vector / Rotation | 3 / 4 × binary32 (rotation as x, y, z, w) |
| HString | u32 length, `0xFFFFFFFF` for a null string, ≤ 4,096 bytes |
| String | u32 length ≤ 4,096; a Load copies it into a fixed buffer only after checking the destination size (the original corpse copy is unchecked) |
| LoadSave object | u32 record index, `0xFFFFFFFF` for NULL or a target outside the list |
| Plain object inside a save | NULL sentinel plus a per-class diagnostic (D7) |
| Nested message | u32 length ≤ 16 KiB, then bytes (the client blob) |

`WriteToMessageCompVector`, `WriteToMessageCompPosition` and
`WriteToMessageFormattedHString` appear only in original effect messages, never in
Save bodies. They are not declared, so a save body that calls them does not
compile; effect messages stay typed. The reader latches its first error. The
original reader rewinds automatically at the end of a message; inside
`MID_LOADOBJECT` that rewind is disabled, so a read past the end is an over-read.
Over-reads and under-reads are failures (D5). An original read whose type differs
from its write is reproduced literally; Naga's DWord-written, Float-read counter
keeps that original behavior unless a departure is recorded.

#### Implemented primitives (lot 1)

`ServerAdapter::save_object_payload` and `load_object_payload` dispatch
`MID_SAVEOBJECT`/`MID_LOADOBJECT` to one object. `pData` points to the message
handle and `fData` carries dwParam. Lot 3 builds `SaveObjects`/`RestoreObjects` on
them; nothing calls them from the game yet.

- **Dispatch context.** The payload message exists only for the call and is
  borrowed: the callback cannot release, route or (on load) write it. A dispatch is
  refused unless the adapter is the bound session, no object callback is running,
  no other payload dispatch is active and the object is alive.
- **Index table.** `SaveIndex` is the dense save-list table. It maps a handle to its
  record index when saving and an index to the restored handle when loading. A
  LoadSave write stores the index of a listed, alive target. A NULL, stale,
  going-away or unlisted target stores `0xFFFFFFFF`. A load resolves the sentinel to
  NULL; an index past the table, or naming an object that no longer exists, fails.
- **Context refusal.** LoadSave writes and reads work only on the active payload
  message of the matching direction. On any other message, including a nested
  message created inside the callback, they fail and latch that message. Plain
  `WriteToMessageObject` writes the sentinel inside a save and counts it (D7). Plain
  `ReadFromMessageObject` inside a load requires the sentinel and returns NULL.
  Outside saves, both plain calls are refused and recorded as unavailable;
  ordinary object references stay typed.
- **Failure scope.** Any message refusal during the dispatch (a stale handle, a
  borrowed release, a bound, a wrong context) fails that dispatch. The report
  keeps the first error.
- **Strings.** `ReadFromMessageString` returns text owned by the message until it
  is released, and `""` after a failure. `ReadFromMessageStringBuffer` copies into
  a fixed buffer and fails, without overrunning, when the text and its terminator
  do not fit. A null `WriteToMessageString` pointer writes an empty string
  (`InventoryMgr.cpp:1685` passes `DNULL`).
- **Nested messages.** `WriteToMessageHMessageWrite` nests a standalone writer.
  `WriteToMessageHMessageRead` nests an owned nested reader or a borrowed callback
  reader; it forwards the whole payload regardless of the read cursor. Routed sends,
  typed side-car messages and payload messages never nest.
  `ReadFromMessageHMessageRead` returns an owned, read-only reader that only
  `EndHMessageRead` releases. Nested bytes are ordinary, so references inside them
  are refused.
- **Report.** Each dispatch reports the class name, payload and consumed bytes,
  resolved and NULL references, plain-object NULLs, nested messages and the first
  error. Counters never change payload bytes.

### Fingerprint

| Mismatch | Result |
| --- | --- |
| Container or section version newer than the build | Refused |
| Any other container or section version | Refused; there is no migration before 1.0 (decision 6) |
| World DAT digest differs from the mounted file | Refused; object records and BSP bindings depend on the exact DAT |
| A class in the file is unregistered, or its payload version differs | Refused, naming the classes |
| Edition differs | Refused |
| Mount set or effective index differs (mods added, removed or changed) | Refused, listing every added, removed or changed archive (decision 5) |

Directory-block digests reuse metadata the REZ reader already reads and bound.
They do not detect a same-size in-place content change, which the
[world contracts](world_contracts.md) already exclude. Whole-archive hashing is
not required.

All differences are reported together in one refusal, so the menu can show them.

### Version-1 encodings

Lot 2 implements the container in `src/engine/save_file.*` and the fingerprint in
`src/engine/save_fingerprint.*`; both are engine code with no game dependency.

- **Header.** 24 bytes: the 8-byte magic, `u32` container version, `u32` section
  count (8, or 9 with `AUTO`) and `u64` total length, which must equal the file
  size. The header has no CRC; every field is checked exactly.
- **Tags** are their four ASCII bytes in file order; `END` is written `"END "`.
- **CRC** is the standard reflected CRC-32 (zlib, check value `0xCBF43926`) over
  the section's tag, version, length and payload.
- **Read order.** For each section: known tag, expected position, length within the
  section bound, length within the remaining bytes, CRC, then version. A version
  edit therefore reads as a CRC failure unless the CRC was also recomputed.
- **Section bounds.** META 4 KiB, FPRT 64 KiB, CONS 280,580 bytes (256 maximal
  pairs), CLCK 16 KiB, OBJS the 64 MiB file bound, LINK and ATCH 16 MiB each,
  AUTO 32 MiB, END 0. Lot 3 may tighten the opaque CLCK, OBJS, LINK and ATCH
  payloads; lot 2 stores them as bounded bytes.
- **Payloads are exact.** A section parser must consume its whole payload.
- **META.** `u8` kind (0 slot, 1 quick, 2 level entry), `u8` slot (0–7 for a slot,
  else 0), display name (1–255 bytes), `u64` UTC seconds, world path (a
  normalized resource path, ≤ 1,024 bytes), world title (≤ 255), `u32` game type
  (SINGLE, ACTION or CUSTOM only), `u32` character (0–3), `u32` difficulty (0–3),
  version text (1–64 bytes), binary64 simulated seconds (finite, nonnegative).
- **FPRT.** `u8` edition (0 retail, 1 demo); `u32` mount count (1–32) and per mount
  `u8` role (0 base, 1 mod), normalized archive filename (never a host path), `u64`
  archive size and the 32-byte directory digest; the 32-byte index and world
  digests; `u32` class count (≤ 1,024) and per class its name (1–64 bytes,
  strictly ascending byte order) and `u32` payload version (≥ 1); then the
  32-byte class schema digest, which must match the list.
- **CONS.** `u32` pair count, then name and value strings; names are unique and
  keep file order.
- **AUTO** holds the complete level-entry container bytes. Its META kind must be
  level entry and it has no `AUTO`. A top-level level-entry file cannot carry one.
- **Directory digest.** SHA-256 of the 168-byte REZ header followed by every
  directory block in the reader's visit order, computed while indexing.
- **Index digest.** SHA-256 of `u32` entry count, then per entry in path order:
  `u32` path length, path, `u32` mount ordinal, `u32` offset, `u32` size. Host
  paths never enter it.
- **World digest.** SHA-256 of the DAT bytes read through the mounted index.
- **Class schema digest.** SHA-256 of `u32` count, then per class `u32` name
  length, name and `u32` payload version.
- **Refusal categories.** Every failure carries one of: file error, bound,
  truncated, format, version, CRC, section order or fingerprint, plus a message.
  The menu shows them for unreadable slots.
- **META quick read** reads the header and the META section only (at most
  4,140 bytes from disk) and still checks the declared length against the file size.
- **Atomic replacement.** `write_file_atomic` creates the parent directory, writes a
  unique `<name>.tmp-<ticks>` sibling and renames it over the target; any failure
  removes the temporary and keeps the previous file. Preferences use the same
  helper with unchanged bytes and messages. Save files also opt in to removing
  stale temporaries of the same file after a successful rename.

## Engine and game responsibilities

| Engine and session layer (`src/engine`, adapter services, `GameSession`) | Game (`src/game/original`, shells, menus) |
| --- | --- |
| Bounded message primitives and the save/load dispatch context | Which objects carry `USRFLG_SAVEABLE`, per original class |
| Container, CRCs, bounds, fingerprint, atomic file replacement | Per-class `Save`/`Load` bodies in original order, with their quirks |
| Engine records, the index table, links, attachments, sky membership | `INITIALUPDATE_SAVEGAME`/`PRECREATE_SAVEGAME` branches |
| Restoring the clock and scheduling, activity and deactivation | Restarting sounds and republishing effects in Load, as the original does |
| Restore-mode world loading and the `CF_ALWAYSLOAD` class flag | The client blob, `SMSG_CLIENTSAVEDATA`, the restored-client branch |
| Two-pass dispatch, exact consumption, purity checks | Shell console overrides, autosave, Restart and Quick rules, slot metadata |
| The candidate session, isolation of original globals, promotion and failure cleanup | The menus, F-keys, death-to-Load-menu, notices |
| Native codecs for engine-implemented classes and owner-bound effect publishers | Per-class payload versions and the save-support declaration |

Every registered class declares its save support:

- **Original** with a payload version: its chain has original Save/Load bodies.
- **AlwaysLoad**: rebuilt from the DAT and never saved.
- **NotSaved**: not saveable in the original and lost on restore, like gibs or
  transient effects.
- **Unsupported**.

A save that would include an Unsupported saveable object is refused before any
file is written, and the refusal names the classes. This lets slots and menus
ship before every class family is complete. Payload versions start at 1 and
change whenever a class's byte layout changes.

## Save sequence

A save request is an owned value, consumed at the application's owning frame
boundary or while the menu pauses simulation, never inside an SDL or object
callback.

1. **Admission.** The session must be bound and idle, with no pending level
   change, no live messages and no pending removals. The game type must be
   single-player and not the demo edition. The player must be alive, in the
   world, and associated with the local client.
2. **Client data.** The client shell builds its original `SaveClientData` blob.
   The server shell attaches it to the player, as `CMSG_SAVEGAME` did.
3. **Save list.** The server shell builds the list as the original `SaveGame`
   does: every Alive object with `USRFLG_SAVEABLE`, active then inactive,
   players included. `SaveObjects` skips always-load classes and pending
   objects, then writes the rest in creation order (D8). Any Unsupported class
   refuses the save.
4. **Write.** The engine writes CONS and CLCK. For each object it writes the
   engine record, then dispatches `MID_SAVEOBJECT` with dwParam 0 into a bounded
   writer bound to the index table. It then writes LINK and ATCH.
5. **Purity.** During `MID_SAVEOBJECT` the engine refuses object creation and
   removal, message sends, scheduling and activity changes, and audio or effect
   requests; any attempt fails the save. Consuming the pending client blob is the
   original's only mutation and is allowed.
6. **Commit.** The client side adds META, FPRT and the current level-entry
   snapshot as AUTO, then writes the slot file atomically.
   - A failure keeps the previous slot file and reports the error.
   - A save never changes simulation state, clocks or identities.

The level-entry snapshot is produced by the same sequence, with save kind "level
entry", at the frame boundary after the single-player initialization of a NEW_GAME
or NEW_LEVEL entry (the original `CMSG_AUTOSAVE` → `SMSG_DOAUTOSAVE` round trip).
A load never produces one, because `GAME_DOAUTOSAVE` is 0. It is kept in memory and
in the current-level file, which Restart reads. New Game deletes that file.

### Save order

Records follow creation order. Object handles are append-only within a session,
so restore recreates objects in the same relative order. Constructors, initial
updates and Loads that insert into original intrusive lists therefore rebuild
them in the same relative order. This covers the Cabal, Monster, Destructable,
Smell, Exit, AmmoPickup and Camera lists.

A save made immediately after a load, before simulation advances, must
reproduce the loaded OBJS, LINK and ATCH bytes. The only allowed differences are
the fields an original `PRECREATE_SAVEGAME`, `INITIALUPDATE_SAVEGAME` or Load
deliberately rewrites. Examples: ClientLightFX resets its flags and next update,
the scene explosion, lightning and warp gate initial updates reschedule their
polling, and CPlayerObj hard-sets flags and zeroes velocity. Each class lists its
expected rewrites. This is the primary round-trip property tested
below.

## Load sequence

1. **Validate the file.** Read it into memory (≤ 64 MiB) and parse every section:
   - check CRCs, bounds, the section order and string limits;
   - check registered classes and their payload versions;
   - check the fingerprint;
   - check that META's world exists in the mounted index.

   Nothing in the game has changed yet.
2. **Isolate.** Use NEW_LEVEL's [boundary](level_change_contracts.md):
   - consume the request once at the owning frame boundary;
   - suspend the source's original globals: the seven intrusive heads and counts,
     the active camera, the AI table owners, `g_ctf` and the server binding.

   A load from the menu with no world has no source to retain.
3. **Candidate session.** Allocate a candidate session at a stable address, then
   copy only the mounted resource index. Restore CONS, then CLCK (clock,
   generator, global force), **before any object exists** (D9).
4. **Restore-mode world.** Decode the DAT and collision and run `PreStartWorld`.
   - Create only the DAT rows whose class is always-load, with `Creation::World`.
     The usual difficulty and game-type filtering uses the restored console.
   - Give every other row the `SavedState` disposition.
   - Startup accounting treats `SavedState` as covered; missing-BSP omissions
     keep their existing meaning.
5. **Pass 1.** For each record, in order:
   1. Create the object with `Creation::Restore`. `MID_PRECREATE(3)` receives the
      saved type, flags, transform, name, filename, skin, container code and
      next-update delay.
   2. Bind world models by their saved BSP name.
   3. `MID_INITIALUPDATE(3)`.
   4. Apply the remaining engine record fields. A world model moves through the
      existing atomic transform path.

   A precreate refusal fails the load; there are no NULL holes.
6. **Index table.** Map every record index to its new handle.
7. **Pass 2.** `MID_LOADOBJECT` with dwParam 0 for each record in order, through
   a reader that resolves LoadSave references against the table. Exact
   consumption is checked after each call.
8. **Links and attachments.** Restore LINK, then ATCH. Link creation is
   idempotent, so a Load that already recreated a link does not duplicate it.
9. **Shell.** Set `GAME_GOTOSTARTPOINT`, `GAME_REVISITING` and `GAME_DOAUTOSAVE`
   to 0. Then run `PostStartWorld` (path list, VoiceMgr reset) and rebuild the
   music catalog from the restored world properties.
10. **Player and client.**
    - Exactly one record must carry the local-client flag (D6).
    - Associate a new local client token with it through the existing
      [restored-client branch](player_restore_contracts.md): `SetClient`, the
      single-player init message, `SMSG_FORCEROTATION` and `SMSG_EYELEVEL`.
      There is no `GoToStartPoint` and no fresh `SINGLEPLAYER_INIT`.
    - Deliver the `SMSG_CLIENTSAVEDATA` that player Load queued to
      `LoadClientData`.
    - The client restarts music at the restored music level.
11. **Admission checks.** Run the same checks as checked startup and NEW_LEVEL:
    - dispositions;
    - NPC runtime failures;
    - unavailable services;
    - the strict presentation preflight (actual DTX, SPR, models and skins
      within the 4,096-image and 128 MiB limits);
    - music admission;
    - no pending level change and no live messages.
12. **Promote.** Promote exactly as NEW_LEVEL does:
    1. Flush the source's queued audio.
    2. Transfer audio output; this is the last step that can fail.
    3. Destroy the source under its own adapter.
    4. Bind the candidate.
    5. Advance the world revision.
    6. Clear UI selections, queued commands and input ownership.
    7. Exclude the preparation time from the next simulation delta.

    Only after promotion does the client update the current-level file from the
    loaded file's AUTO section, and slot metadata in memory.

A failure at any step from 2 to 12 destroys the candidate under its own adapter.
It then resumes the source's globals and binding and reports the error. The
source session's objects, identities, clocks, voices and revision are unchanged.
Process-wide counters (message, audio and effect IDs) are never rewound.

### Implemented restore-mode world and transaction (lot 4)

`GameSession::save_game` and `GameSession::load_game` (`src/game/saved_game.cpp`) work on an
in-memory `GameSave`: the normalized world path, the fingerprint, and the `SaveObjects` image
of `Current.sav`. [Lot 6](#implemented-slots-quick-and-restart-lot-6) wraps it in META and a slot file.

- **Save support.** `ClassInfo::save` is the declaration. AlwaysLoad is the original
  `CF_ALWAYSLOAD` and is inherited.
  - AlwaysLoad: StartPoint, GameStartPoint, PathPoint, Key, SmellHint, ExitHint,
    WorldProperties, CameraSpot, ParticleSystem, PolyGrid and SparksObj, and the engine's
    Model, Container, Sprite, Sound, DemoSkyWorldModel and SkyPointer. Script, LightFX,
    WreckingBall, SoccerGoal, WorldModel and FogSphere are not registered, and no shipped
    world places them.
  - NotSaved (the original never marks them saveable and loses them on a restore): BaseClass,
    B2BaseClass, CClientSFX, CClientSplatFX, CGib, CHandWeaponModel, FlagObject, FlagStand
    and NagaCeilingDebris.
  - Every other registered class is Unsupported. The build's class schema
    (`save_class_schema`, the Original classes with their payload versions) is therefore
    empty until a class lot declares one.
- **Marking.** The ports set `USRFLG_SAVEABLE` where the 25 original files do. Door and
  Trigger now add it to the existing user flags, as the original does, instead of replacing
  them. Two decision-4 departures mark what the original left unmarked: every light of the
  ClientLightFX family (the original marks only a light that starts on) and RandomSoundFX.
  Lot 10 supplies their payloads ([lot 10](#implemented-effect-publishers-and-audio-emitters-lot-10));
  until then a save that included them was refused by name, never silently lost.
- **Save list.** The original shell `SaveGame` lists every `USRFLG_SAVEABLE` object, first
  through `GetNextObject`, then through `GetNextInactiveObject`. The latter is the original
  inactive enumeration, now a bounded service. Players are included. It then calls
  `SaveObjects(Current.sav, list, 0, SAVEGAMECONSOLE | SAVEPORTALS)`. `RestoreGame` calls
  `RestoreObjects(Current.sav, 0, RESTORETIME)`.
- **save_game admission.**
  - The session must be bound, outside callbacks, with a loaded world, no pending level
    change and no live message or deferred removal.
  - The game type must be single-player (SINGLE, ACTION or CUSTOM; D12) and the edition retail.
  - A live object whose class is Original or Unsupported but lacks `USRFLG_SAVEABLE` refuses
    the save by name. The shell's list would otherwise drop it silently.
  - The fingerprint is built before `SaveGame`, so a refused save leaves the store unchanged.
  - Since lot 5 the live local player is required and its client blob is attached first
    ([lot 5](#implemented-player-and-client-lot-5)).
- **Restore-mode world.** `load_saved_world` takes `LOADTYPE_RESTORESAVE` (3, the original
  value) with the saved game type and difficulty. The message parser still refuses that load
  type.
  1. The DAT and collision are prepared as usual, then the world unload resets the generator
     (seed 1), the world-time origin, the sky and the effects.
  2. CONS, then CLCK, come from the candidate's `Current` image (`restore_image_state`). The
     always-load rows therefore start from the saved clock, origin and generator, and the
     saved difficulty and game type filter them.
  3. Only rows whose class is AlwaysLoad are created. Every other registered row is
     `SavedState`, which startup accounting counts as covered. A non-always-load row whose BSP
     is missing is also SavedState: it was never created, so no save can hold it.
     Unregistered, editor-only and absent classes keep their usual meaning.
  4. The always-load frame is finished. A deferred removal still pending then refuses the
     load.
  5. The shell's `RestoreGame` runs immediately, with no simulation step in between.
  6. `GAME_GOTOSTARTPOINT`, `GAME_REVISITING` and `GAME_DOAUTOSAVE` are set to 0. Then
     `PostStartWorld` runs and the music catalog is rebuilt from the restored console.
- **Console and global force.** This settles the open question from lot 3.
  - The original `LoadWorld` creates the always-load objects first; `RestoreGame` then calls
    `RestoreObjects`, which restores the saved console. In the later runtime this is
    `sm_RestoreConsoleVars`, inside RestoreObjects. The saved console therefore wins over what
    WorldProperties sets at precreate. `RestoreObjects` applies CONS again when objects
    already exist, and rolls it back on failure.
  - The original never saves the global force: the later runtime's save header holds only
    the clocks and the console. Only WorldProperties sets it, so its authored value stands.
    CLCK's force is applied before the objects and WorldProperties then overrides it, as in
    the original. A world without WorldProperties keeps the saved force.
  - Always-load objects that draw random numbers draw from the restored generator (D10). A
    save right after a load therefore differs in CLCK only by the generator, and in CONS only
    by the three shell variables above. OBJS, LINK and ATCH are byte-identical.
- **load_game.** It runs at an owning frame boundary on the bound session, which may have no
  world. The stages, as `game_load.stage` names them:
  1. *admission*: no pending level change, world revisions not exhausted, the retail
     edition, a normalized world DAT path, a full save (dwParam 0) whose console holds a
     single-player game type and a difficulty of 0–3, and a fingerprint class list equal to
     the image's.
  2. *fingerprint*: `check_fingerprint` against the installation and the build's schema (the
     original classes plus any extra classes).
  3. Isolation, exactly as NEW_LEVEL: the seven list/count pairs, the active camera, the five
     AI table owners, the CTF state and the binding.
  4. *candidate*: a new session with a copy of the resource index and its own copy of the
     source's `SaveStore`; the loaded image becomes its `Current`. The source's store is not
     touched (D2).
  5. *world* and *restore*: the restore-mode world above.
  6. *player*: exactly one restored local player is attached to a new client and receives
     its client blob ([lot 5](#implemented-player-and-client-lot-5)). A candidate is never
     promoted with an unattached player.
  7. *validation*: `check_world_initialization` (dispositions and unavailable services), NPC
     runtime failures, no pending level change or live message, music errors, and the strict
     presentation preflight.
  8. *audio*, then promotion as NEW_LEVEL. The source's queued audio is cleared and its output
     transferred; this is the last step that can fail. The source is destroyed under its own
     adapter, and the candidate is bound with `world_revision` one above the source's.

  A failure at stage 4 or later destroys the candidate under its own adapter, then resumes the
  source's globals and binding. The source's objects, identities, clock, generator, console,
  voices, store, lists and revision are unchanged. The extra-class parameter registers test or
  tool classes in the candidate; the game passes none. On success the caller clears UI
  selections, queued commands and input ownership, and excludes the preparation time from the
  next delta, as after a NEW_LEVEL commit (lot 6 wires this).
- **Real worlds.** 50 of the 58 mounted worlds bind their SkyPointers to Door or
  CDestructableBrush objects, which only a save can supply. Their always-load-only restore
  fails the sky preflight; lot 8 makes those objects restorable. SubwayA, SteamTunnels, SubwayB,
  SubwayC and PowerStation restore from a player-only image (since lot 5), then save and load
  again.

### Implemented player and client (lot 5)

CPlayerObj is Original (payload version 1). Its record is the original chain:
`CPlayerObj::Save`, `CBaseCharacter` (0 bytes), then the aggregates in reverse order of
addition, `CMovement`, `CInventoryMgr` and `CDestructable` (`src/game/original/PlayerSave.cpp`,
`Movement.cpp`, `InventorySave.cpp`, and lot 8's `CDestructable` block in
`DestructableSave.cpp`). Each aggregate saves for any owner, so the class lots that declare
NPCs and corpses reuse it.

- **Original layouts.** Every field is written in the original order with the original
  call: Byte, Word, DWord, Float, Vector, Rotation, HString, LoadSave object, plain object
  and nested message. The [evidence](../implementation/reports/save_load_player.md#byte-layouts)
  lists the blocks. Fields the port interprets come from its members. Original fields it
  keeps without interpreting (demo playback, control flags, scores, bindings, view and gun
  vectors, slow death, swim jump, burning and the idle clock) start at the original
  constructor values and are carried unchanged as `PlayerCarried`. Fields with no port
  storage are written with fixed values: inventory god mode 0 (no cheats), the fire-rate
  multiplier 1 and `m_bDropEye` 0 (their defaults), `m_WeapCount` 0, the melee multiplier 1
  and item `m_bDropped` 0 except for a thrown eye (uninitialized in the original), a plain
  NULL camera object (D7) and, for the player, a NULL `CMovement` owner because it never
  initializes that aggregate.
- **Typed validators.** Each byte body converts to the existing typed value
  (`PlayerState`, `InventoryState`, `DestructableState`) and calls the typed Save or Load,
  so their checks and staged, all-or-nothing commits apply. A refusal calls lot 8's
  `RejectPayload` service, which fails the dispatch. Strings are written with lot 8's
  `save_fields` helpers, which never allocate a session string token. Booleans must be 0 or 1, the `0xCECECECE` markers must be present, `(DBYTE)` words
  must fit a byte, the power sound stack holds at most 15 entries of 0–4 and a movement
  path at most 64 points. The original reads all of these raw.
- **Preserved quirks.** Relative clocks are saved as `x − GetTime()` and loaded as
  `saved + GetTime()`, including the −1 "not falling" sentinel; the load picks the nearest
  float that saves the same offset again (lot 8's `restored_time`). Countdowns and durations
  are raw. Attributes and maxima load without `ValidateAttributes`; the first update
  derives speeds from the raw speed attribute. A non-destructible owner saves the
  CanDamage 1.0e6 sentinel; an uninitialized `m_nLastDamageLocation` byte round-trips.
  Weapons are recreated from their types and ammunition is assigned raw afterwards. Keys
  are dropped only for dwParam 1. Every item loads inactive. The client blob is consumed
  by the first save that includes the player and broadcast by its Load. The Load tail calls
  `SetCharacter`, resets the named nodes, sets the original flags without SHADOW, zeroes the
  velocity and turns zoom (both bytes) off.
- **Port adaptations.**
  - The port keeps typed items, not the original intrusive list. Items are written in
    type order (medkit, flashlight, goggles, eye, binoculars, then keys in pickup order),
    and a load restores that order. The original writes pickup order.
  - Non-key items carry their original picture names but NULL item names: the port has no
    server `FormatString`. The current item is written as its type, and Load selects the
    first carried item of that type ([inventory cursor](inventory_cursor_contracts.md)).
    Earlier saves wrote `INV_NONE`.
  - An unknown item type is refused; the original skips the item's data and desynchronizes.
    A thrown eye (a non-NULL SeeingEye reference) was refused until lot 9, which binds it.
  - The original does not save the eye level. The crouch state and eye level are derived
    from the saved movement state, and the client reads the eye level from the player, as
    before, instead of an `SMSG_EYELEVEL` message.
- **Powers and imprisonment.** All five power flags, their relative expiries and the
  power sound stack are saved; the sound handles are not, and `CheckPowerups` restarts the
  top power's loop. Anger's ×3 travels in the inventory damage multiplier, Willpower's nigh
  invulnerability in the damage aggregate and Stealth's alpha in the engine colour.
  Imprisonment saves its flag, relative start and length. The typed `PlayerState` used by
  NEW_LEVEL now carries the same block, so the active-power refusal is removed from
  `SaveFields`, `LoadFields` and `RestoreModelState`; the CTF, Orb and weapon-callback
  refusals remain.
- **Client blob.** `CBloodClientShell::SaveClientData` writes the original fields: night
  goggles, wonky time and flags, blind time and flag, fade in, out and value, music level,
  `m_nLastLoadType` (1 new game, 2 new level, 3 restore), game type, and the objectives title
  and text (NULL until the first objectives message).
  - The port has no wonky vision: it writes the idle values and refuses a blob that holds
    any.
  - The blind time is an absolute client time, valid because a full restore keeps the clock.
  - `LoadClientData` discards the goggles flag and the load type, requires the game type to
    equal the restored console and the blob to be read exactly (new `MessageFullyRead`
    query), then restores fade, blind time, objectives (with no resource ID) and the music
    level through `PlayMusicLevel`. The original's trailing `maxfps 0` has no equivalent.
- **CMSG_SAVEGAME.** `save_game` builds the original request (file name, `SAVETYPE_CURRENT`,
  the nested blob). The shell's `OnSaveGameMessage` attaches the blob to the sender's player
  before `SaveGame`. A refused save releases the blob, which the original kept pending (or
  leaked without a player).
- **SMSG_CLIENTSAVEDATA.** The player's Load hands the blob to the new `SendClientSaveData`
  service, the typed replacement for the original broadcast. It holds at most one blob: a
  second fails the Load (D6). The load delivers it to `LoadClientData` after the client's
  restored entry. The original order is unverified; its probable order lets `OnEnterWorld`
  reset fade and wonky afterwards, while OpenBlood2 keeps the saved fade.
- **load_game's player step.**
  1. The image must hold exactly one CPlayerObj, carrying the local-client flag and alive
     (D6). The original Load restores a dead player, but the port has no corpse
     restoration, and a save of a dead player is refused anyway.
  2. A new local client is attached through `OnRestoredClientEnterWorld`: `SetClient`,
     `SMSG_PLAYERINIT_SINGLEPLAYER`, `SMSG_FORCEROTATION` and the first client state. There
     is no `GoToStartPoint` and no fresh `SINGLEPLAYER_INIT`.
  3. The client enters the world as restored, the blob is delivered, and the player
     messages, weapons, hand models, frame end and attachments are processed.

  The validation stage adds RunWorld's player checks (in the world, associated, alive,
  client music) and leaves the promoted session `Ready`.
- **save_game admission.** As the original `MenuSaveGame`: the live local player must be
  in the world, not dead, with its profile received.
- **Shell console.** A fresh or keep-alive `LoadWorld` now sets `GAME_GOTOSTARTPOINT` to 1,
  `GAME_REVISITING` to 0 and `GAME_DOAUTOSAVE` to 1. A restore sets all three to 0, then
  `RespawnPlayer` sets `GAME_GOTOSTARTPOINT` back to 1.
- **Model and skin.** The engine record's model and skin replace the precreate choice
  before the animation track is applied (D13).
- **Expected rewrites.** A save right after a load reproduces the player's record except:
  - the flags (no SHADOW) and the zeroed velocity;
  - hidden nodes reset by name;
  - zoom 0;
  - the blob's load type 3;
  - items written in type order.

Lots 6, 9 and 11 build on this: lot 6 wires `save_game`/`load_game` to files and menus and
clears UI state on commit; lot 9 makes SeeingEye Original and replaces the thrown-eye
refusal (its Load re-links the owner and sends `MID_ATTACH`); lot 11 declares the NPCs, whose
`CMovement` records carry their own owner, and CCorpse, which reuses the `CDestructable`
block and its owner reference.

### Implemented slots, Quick and Restart (lot 6)

`SaveSlots` (`src/game/save_slots.*`) turns the original `Save\SaveN`, `Save\Quick` and
`Save\Current` directories into one container file each. `SaveFlow` (`src/app/save_flow.h`)
connects them to the menus, F5/F6 and death; the frontend consumes its requests at the owning
frame boundary. The original rules are `LoadSave.cpp`, `MenuLoadGame.cpp`, `MenuSaveGame.cpp` and
`BloodClientShell.cpp` (`MenuSaveGame`, `MenuLoadGame`, `OnKeyDown`, `OnCommandOn`).

- **Files** (decision 1). `SDL_GetPrefPath("OpenBlood2", "Blood2")/saves/retail/` holds
  `slot0.ob2sav`–`slot7.ob2sav`, `quick.ob2sav` and `current.ob2sav`.
  - A slot or quick file is a full save: META (kind slot with its number, or quick), FPRT, CONS,
    CLCK, OBJS, LINK, ATCH and, when a level-entry snapshot exists, that snapshot as AUTO.
  - `current.ob2sav` is the level-entry snapshot itself: a top-level container of kind level
    entry with no AUTO. It is the Restart source.
  - Every write uses `write_file_atomic` and removes stale temporaries of that file after the
    rename. A refused or failed save keeps the previous file.
  - A file whose META kind or slot differs from its name is refused as a format error.
  - Smoke runs and the demo never use the directory.
- **META.** The world title is the original `GetWorldTitle`: the last `worldname` token of the
  DAT world info string (commands end at `;`, quotes group), else the world file name without
  its directory and extension.
  - A slot or the quicksave is named `title [Mmm dd hh:mm:ss]`, the original `SetSlotName`
    suffix (`ctime` + 4, local time, space-padded day). The snapshot keeps the bare title, like
    the original Current slot.
  - Game type and difficulty come from the console and the character from the player. They are
    informational: a load uses the saved console. The version text is the build's
    `PROJECT_VERSION`.
- **Listing.** At startup and after each save or load, the header and META of all ten files are
  read (original `LoadInfo`).
  - An empty slot reads `<EMPTY SLOT>`; an unreadable one reads `<UNREADABLE: fault>` with the
    lot 2 fault name (a kind mismatch is `format`). Fingerprints are checked only on load.
  - Load rows are enabled for readable and unreadable files, so activating an unreadable row
    shows its full reason. Empty rows are disabled; the original returned FALSE silently.
- **Save** (`MenuSaveGame`). `save_game` runs at the frame boundary, then the file is written.
  A refusal by `SaveObjects` lists the classes without save support, as its report names them:
  "This game cannot be saved yet. These classes have no save support: Class (Unsupported), …".
  Any other refusal shows `save_game`'s reason.
- **Level-entry snapshot** (the `CMSG_AUTOSAVE` round trip). It is taken after a menu New Game,
  the Credits world, a NEW_LEVEL commit, an inspector level start and a direct `--view-world`
  entry, and only while `GAME_DOAUTOSAVE` is nonzero, so never after a restore.
  - Deferred removals that initialization leaves (07_cathedral leaves one) are finished first, as
    the next frame boundary would; a snapshot taken with one pending was refused
    ([lot 14](#certified-whole-worlds-lot-14)).
  - `save_game` runs with `SAVETYPE_AUTO`: the request carries save type 0 and the image lands in
    the logical `Auto` store, as `Auto.sav` did.
  - The snapshot is kept in memory and written to `current.ob2sav`. New Game first removes both
    (`ClearSaveSlot(SLOT_CURRENT)`); Credits does not, as `MenuCredits` does not.
  - A refused snapshot is silent, like the original autosave: it is logged and shown in the
    inspector. It removes both copies (D14). A failed file write keeps the snapshot in memory
    and reports the error.
- **Load** (`MenuLoadGame`). The whole file is read and decoded, including AUTO, before anything
  changes. Its kind must match the slot. It becomes a `GameSave` (dwParam 0, console and portals,
  the FPRT class list as the image's), and `load_game` runs.
  - A fingerprint refusal also names the saved and the running OpenBlood2 versions (decision 6);
    the mount differences come from `check_fingerprint` (decision 5).
  - Only after promotion does the snapshot follow the file. Restart keeps its own. A slot or the
    quicksave brings its AUTO into memory and `current.ob2sav`, or removes both when it has none,
    so Restart always means the start of the loaded slot's level.
  - Restart restores the snapshot through the same `LOADTYPE_RESTORESAVE` path. The original's
    `LOADTYPE_RESTOREAUTOSAVE` differs only in the file `RestoreGame` reads; the client's load
    type (3 instead of 4) is written to the blob and discarded on load.
  - After a load the frontend clears the inspector selection, the controlled object, queued game
    input, the F5/F6 and dead-USE requests and input ownership, as after the NEW_LEVEL commit
    (`reset_after_promotion`), and excludes the preparation time from the next delta.
- **Menus.** The Load page lists `RESTART: %s` and `QUICKLOAD: %s` (cres.dll strings 547 and 548,
  with the `%1!s!` insertion), then the eight slot names; the Save page lists the eight names.
  - Rows use the original `MENU_CMD_LOAD_SAVE_GAME` (600) and `MENU_CMD_SAVE_GAME` (700). Load
    row 0 is `SLOT_CURRENT`, row 1 `SLOT_QUICK`, row n the slot n − 2.
  - Save rows are enabled in the world, alive, with saves available. The Single Player menu's
    Save Game row keeps its in-world and alive guard.
  - A successful save or load closes the menu and resumes play (`SMSG_SAVEGAME_ACK` →
    `GS_PLAYING`, `StartNewGame`). A failure keeps the page under the error modal (D3).
  - When saves are unavailable, both pages print the reason and every row is disabled.
- **Death.** `USE` while the death camera is active sets `dead_use_menu`. In a single-player game
  type the frontend opens the Load page as the top-level menu
  (`SetCurrentMenu(MENU_ID_LOAD_GAME, MENU_ID_LOAD_GAME)`), so Esc returns to the game. The USE
  request still reaches the server, as before. The demo keeps its Enter retry; the original
  demo's main menu on USE is not ported.
- **Keys** (decision 2), retail only, not over the menu, inspector or credits screen:
  - F5 needs a live player in the world. It saves the quick slot at the next frame boundary
    without pausing and overwrites without confirmation. The notice "Quicksave..." shows for
    1.5 s of UI time; the original drew its box for two frames.
  - F6 needs a player in the world, alive or dead. It loads the quick slot and shows
    "Quickload...", or "No quicksave to load." when the slot is empty (the original said nothing).
  - A refused F5 or F6 opens the menu, which pauses the game, under the error modal (D3).
  - F2 and F3 keep their OpenBlood2 bindings.
- **Demo** (decision 7). The demo has no slot directory, no Save or Load rows, and ignores F5 and
  F6. `save_game` and `load_game` still refuse the demo edition themselves.
- **Inspector.** The Session panel's "Saved games" node shows the directory, the counters, the
  snapshot state, `GAME_DOAUTOSAVE`, the last action and refusal (with refused classes and the
  load stage), file errors, the visible notice and the META of all ten files.

Real games are still mostly refused until the class lots (7, 9 to 13) declare their classes
Original; the screens and keys already report those refusals by name.

### Implemented movers and brushes (lot 8)

The original Save/Load bodies of Door, RotatingDoor, the Rotating aggregate, CDestructableBrush
with RotatingBrush and WorldModelDebris, CDestructableModel with RotatingModel, the CDebris and
CDestructable aggregates, VolumeBrush with Water, Blood, Acid, Ladder, Conveyor, Damage,
Minefield and FreeFall, TripLaser and Tchernotronic. The 18 registered classes are Original with
payload version 1; their `PRECREATE_SAVEGAME` refusals are gone.

- **Byte order.** Each class writes its block before its parent, then the aggregates run in
  reverse order of addition: a RotatingBrush writes CDestructableBrush, Rotating, CDebris,
  CDestructable; a WorldModelDebris writes its own block first. Fields follow the original
  order and widths: booleans are bytes, RotatingDoor's open directions are shorts written as DWords,
  the CDestructable block keeps its `0xcececece` markers. Empty strings are the original NULL
  HSTRING; the others are written with `WriteToMessageString`, which produces the same bytes
  without a temporary HSTRING (session string tokens are never reused, so repeated saves
  would exhaust them).
- **Restore branches.** `PRECREATE_SAVEGAME` reads no property and keeps only the original
  post-processing (BSP name, cleared skin, the constructor's flags, TripLaser's forced model
  and skin). `INITIALUPDATE_SAVEGAME` skips the initialization: aggregates bind their owner from
  their Load (the original self references), and the engine record restores bodies, blocking
  priority, schedule and user flags. Loads decode and check every field before changing a
  member; a value the port cannot represent fails the restore by class name through the new
  `RejectPayload` service. Payloads must still be consumed exactly (D5).
- **Clocks.** Relative times (`t - GetTime()`: Rotating's last and start times, TripLaser's
  damage time, VolumeBrush's last damage time, Tchernotronic's removal time) restore to the
  nearest float that yields the saved difference again, so a save right after a load
  reproduces the bytes. Where the original float subtraction rounded, the absolute value can
  land one ulp away. CDestructableModel's fade clock stays absolute, valid because a full
  restore restores the clock. Countdowns (Door's open time, Rotating's spin time left, the
  damage death delay) are durations. The state machines resume from the restored next-update
  deadline: a door mid-motion, a dying brush, flying debris, a flickering laser and a
  Tchernotronic counting down continue exactly as the saved world does.
- **Port-only state.** The Rotating block ends with the pose angles of the replacement world-model
  rotation; before its first move a rotator's pose is still the authored rotation while the
  source's angles are zero. Loads check that the pose (and a RotatingDoor's current angles)
  describes the restored rotation. A world model's shell is rebuilt by lot 3 at the saved
  rotation and position, so the atomic pusher/rider solver and actual bounds centers apply
  unchanged on the next move.
- **Values the port does not model.** Never-assigned original fields are written as the
  original's NULL or zero (CDestructableBrush's debris skin, CDebris's second texture and
  model, Door's `m_dwFlags`); Door's uninitialized `m_bLocked` carries the authored Locked
  value. Constructor constants (CDebris's explosion radius and damage, VolumeBrush's
  constructor flags) are written and must match on load. The port keeps the authored
  VolumeBrush viscosity, not the original's internally inverted value, and Chrome is refused at
  creation, so it is saved as false.
- **Losses kept.** Door and Rotating busy loops, the destructible model's sliding loop and
  every one-shot are not restarted; TripLaser restarts its loop in Load, as the original does.
  Destroyed objects stay gone. A door's self-spawned Trigger is saved by reference; since
  [lot 7](#implemented-world-logic-lot-7) the Trigger is saved with it.
- **Departures.** Decision 4: the original VolumeBrush forgets `m_hPlayerClass` on a restore
  and routes the player through the non-player branch; the port's engine marks the player in
  `ContainerPhysics`, so a restored volume keeps recognizing it. The saved container code is
  kept: the original reapplies the class constructor's code at precreate, which would turn a
  generic VolumeBrush authored with another Code into CC_NOTHING, while the port's subclasses
  take their code from properties.
- **Expected re-save rewrites.** Only one: CDestructableBrush's precreate ORs
  `FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE` back into a started WorldModelDebris, whose
  Start replaced every flag. Every other OBJS, LINK and ATCH byte of these classes is identical
  after a load.
- **Sky membership.** A restored Door or CDestructableBrush brings back INSKY and its slot from
  the engine record; the restore-mode world's always-load SkyPointer then adds the same object
  again at its first update, which the engine treats as no change. All 50 sky worlds restore
  their lot-8 objects through `load_game` with no sky failure. Since lot 9, nine multiplayer
  sky maps restore whole; every campaign sky world still holds classes of lots 10, 11 or 12.

### Implemented pickups and projectiles (lot 9)

The original Save/Load bodies of PickupObject (and through it every pickup), KeyPickup,
CProjectile (and through it the whole projectile family, grenades and bombs included) and
SeeingEye. Their 85 registered classes are Original with payload version 1
(`PickupSave.cpp`, `ProjectileSave.cpp`, `SeeingEye.cpp`); their `PRECREATE_SAVEGAME`
refusals are gone.

- **Byte order.** A pickup writes PickupObject's 15 fields; KeyPickup writes its own five
  (model, skin, both icons, use count) before them, as the original KeyPickup derives from
  PickupObject while the port's does not. A projectile writes CProjectile's 26 fields, the
  OpenBlood2 shooter tail below, then the CDestructable block. SeeingEye writes only its
  owner, stuck object and dropped flag, then CDestructable: the original eye derives from
  BaseClass and never writes CProjectile's fields.
- **Restore branches.** Pickup precreate reads no property but keeps `PostPropRead`: the
  model from the class's file and `TOUCH_NOTIFY | VISIBLE | GRAVITY` added to the saved flags,
  `SHADOW` removed. Initial update is skipped, except the AmmoPickup list insertion (AI
  `FindAmmo`) and WeaponPickup's single-player check, which the original runs
  unconditionally. A projectile's precreate skips `PostPropRead` (the engine record carries
  file and update) and its initial update is skipped.
- **Mapped fields.** Pickups: type, value, `m_dwFlags`, bounce acceleration, bouncing and
  rest position, trigger target and message; the object name is the class's
  `m_szObjectName`. CProjectile: direction, speed, flags, damage type, radius, relative start
  time, damage, lifetime and the explode latch; grenades map last position and spin, the
  proximity, time and remote bombs also the armed flag, the Tesla ball and the Singularity
  the armed flag. Loads decode the whole block, check it and only then assign; a refusal
  goes through `RejectPayload`.
- **Values the port does not model.** The single-player pickup constants (`m_bFirstUpdate` 1,
  respawn time 0, `m_bRotate` 0, yaw 0, the never-assigned `m_fLastTime` written relative)
  and the projectile presentation fields (`m_bRemoveFromWorld`, damage message, shockwave,
  explosion and sparks flags, scales, light colour, duration, filename, dims) are written as
  the CProjectile constructor's values, or 0 where the original left them unset (special
  members of classes that do not use them), and must match on load. The port has no server
  `FormatString`, so a pickup's display name is NULL and a key's is its object name.
- **Original losses kept.** Unsaved subclass state restarts from the constructor, as in the
  original: grenade bounce count, proximity get-away time, hit-character and both sound flags
  (an armed bomb plays its arm sound again), remote falling flag, flare colour and tracking,
  orb tracking and attachment, leech curve, napalm fireball bounces, death-ray bounce clock.
  The subclasses' shadowed `m_fStartTime` (constructor `GetTime()`) is the new
  `CProjectile::scale_start`, so the 0.01-second scale delay and the alternate flare's burst
  timer restart; the Tesla ball's and Singularity's attract clocks (constructor 0) are lost,
  so an armed one ends on its first update after a load. Hit objects, weapon types and the
  client ID are lost. Every flight, bounce and arm loop restarts in Update; no sound is saved.
- **Effects.** The engine-saved trail effect message of the original becomes republication in
  Load: grenade, primary orb, life-leech primary, bug spray, vomit and skull trails. A flare
  fragment keeps the engine colour its last update set (the original reads an uninitialized
  colour).
- **Departures.** D15: the original never saves `m_hFiredFrom`; the CProjectile block ends with
  the shooter as a LoadSave reference, because the port's shooter exclusion, shooter-anchored
  one-update projectiles and damage attribution would otherwise fail or hit the shooter.
  With it, a remote charge its player has not signalled is listed again on that player in
  load order (`CPlayerObj::RestoreRemoteBomb`, list only), so a restored player can still
  detonate it; the original loses the list. A restored thrown eye turns its view off on the
  owner's current client when it is removed (the original's unsaved client leaves the view
  on a removed eye).
- **Links.** Load creates no interlink: the projectile-shooter, player-charge and
  eye-owner/stuck links are saved interlinks that the restore re-creates after every Load,
  so LINK keeps its order. CInventoryMgr's Load binds a thrown eye (it must be a SeeingEye)
  and sets its owner, as the original `SeeingEye::Init(owner, NULL)`; the lot-5 refusal is
  gone. A saved alternate orb never holds its player's camera (the player's save refuses an
  active Orb), so a restored one does not create it again.
- **Expected re-save rewrites.** Only one: a pickup whose flags lacked `TOUCH_NOTIFY`,
  `VISIBLE` or `GRAVITY` regains them from `PostPropRead`. Every other OBJS and LINK byte of
  these classes is identical after a load. Pickups never respawn in single player (the
  original removes a collected pickup), so no respawning state exists to save.

### Implemented world logic (lot 7)

The original Save/Load bodies of Trigger with BigTrigger and ToggleTrigger, ExitTrigger,
ObjectivesTrigger, ConversationTrigger, CinematicTrigger, Spawner, KeyFramer with its KeyData
blocks, and CameraObj. The 10 registered classes are Original with payload version 1; their
`PRECREATE_SAVEGAME` refusals are gone, and self-triggered doors save with their Trigger.

- **Byte order.** A subclass writes its block before Trigger's: ToggleTrigger's off messages and
  state, ExitTrigger's world, start point and exit type, ObjectivesTrigger's resource, title and
  text, ConversationTrigger's eight waves, character, count, cursor and flags. BigTrigger has
  no block. KeyFramer writes its timeline, the byte key count, every KeyData block, the current,
  position-1 and position-2 keys as float-encoded indices (−1 for NULL), then one offset and
  rotation pair per key, then the port tail below. Empty strings are the original NULL HSTRING.
- **Restore branches.** `PRECREATE_SAVEGAME` reads no property. CameraObj's PostPropRead still
  runs, forcing its flags and a 0.001 s first update. `INITIALUPDATE_SAVEGAME` skips the
  initialization; CameraObj still joins the camera list. Loads decode and check every field
  against the class's property bounds before changing a member; a refusal goes through
  `RejectPayload`. KeyFramer's key types must match their strings, and its indices the states
  the port's sequencing can reach.
- **Clocks.** Trigger's delay start and CameraObj's deactivation time are relative (`t −
  GetTime()`) and restore to the same saved difference. KeyFramer's elapsed time and key times
  are relative by nature. A trigger waiting on a delayed send, a reset, a toggle, a cinematic
  delay, a spawner and a path all resume from the restored next-update deadline.
- **KeyFramer.** A restored controller never collects its keys again: the always-load Keys it
  consumed come back with the restore-mode world and stay inert. As in the original, its
  target list is not saved: the next update rebuilds it by name and links each target again
  (an existing link is kept). Targets take the saved offsets in list order. Until then a save
  writes the loaded offsets unchanged. A controller saved before its first update collects the
  reloaded keys then. More than 255 keys refuses the save: the original byte count desynchronized.
  A target that was not there at the save gets a zero offset. The retained key sound is never
  saved: a restored path starts silent, like the original.
- **CameraObj.** Load publishes the camera effect again from the restored values, which the
  original engine replayed from the saved special-effect message. Then the original Load tail,
  `SetActive`, recomputes `g_hActiveCamera` and the schedule. An active listener camera
  therefore restores as the active camera and the client's camera view. The tracked target is a
  LoadSave reference; the engine restores the interlink.
- **Door.** A self-spawned Trigger is a saved Trigger. The door's Load checks the reference's
  class and gives the Trigger back its owning door, which is port state outside the original
  block.
- **Original behavior kept.** CinematicTrigger never saves its voice: one saved while a cue
  plays restarts that cue at the restored update, sending its message again. ObjectivesTrigger
  does not save its sound. A timed active camera restarts its full ActiveTime. Trigger's last
  sender is not saved.
- **Departures.** Decision 4, two player-visible losses:
  - The original Spawner reschedules on every initial update from the constructor's
    StartActive (false), so a running spawner stopped after a restore. Its
    `INITIALUPDATE_SAVEGAME` now keeps the restored schedule.
  - The original saves one offset per key, not per target. A target past the key count read
    uninitialized storage after a restore. SubwayA, SubwayB and SubwayC move twelve tunnel
    lights on a two-key `sublight` path, so ten of them would jump. A port tail after the
    original offsets carries a `u32` count (≤ 256) and the remaining targets' offsets.
- **Expected re-save rewrites.** CameraObj only: the forced flags, the schedule (0.01 s active,
  none inactive) and, for a timed active camera, the deactivation time. Every other OBJS, LINK
  and ATCH byte of these classes is identical after a load.

### Implemented effect publishers and audio emitters (lot 10)

The original Save/Load bodies of SoundFX, RandomSoundFX, ClientLightFX with its 13 registered
subclasses (the eleven wave lights, FlickerLight and GlowingLight, which add no code),
CClientExplosionSFX, CClientParticleStreamSFX, CClientLightningSFX, CClientWarpGateSFX and
DetailSprite. The 21 registered classes are Original with payload version 1; their
`PRECREATE_SAVEGAME` refusals are gone. ClientLightFX's record ends with lot 8's
`CDestructable` block.

- **Byte order and values.** Fields follow the original order and widths with lot 8's
  `save_fields` helpers: booleans are 0 or 1, empty strings are NULL, ClientLightFX's start
  time is absolute (valid because a full restore restores the clock). Never-assigned original
  fields are written with their constructor value and must match on load: the explosion's
  `m_fOffset` (0, world emitters never call `Setup`), lightning's `bTriggered` (0) and the warp
  gate's `bInitiallyOn` (0; ReadProp reads `InitiallyOn` into `bOn`). DetailSprite's
  uninitialized `m_bAlignedToWorld` is carried as a byte. Each Load decodes and checks its
  block (the precreate bounds, the light and stream descriptor validators, RandomSoundFX's
  list of `count` leading files) and refuses through `RejectPayload` before changing a member.
- **Restore branches.** `PRECREATE_SAVEGAME` reads no property. The explosion and lightning
  sources read properties for every reason, which on a restore fails and leaves values Load
  overwrites; the port reads nothing. ClientLightFX keeps its `PostPropRead`: with the
  constructor's ON it resets the flags to `VISIBLE | GOTHRUWORLD` and the next update to
  0.01 s. `INITIALUPDATE_SAVEGAME` skips SoundFX, RandomSoundFX, ClientLightFX, the particle
  stream and DetailSprite initialization. The explosion (0.1 s), lightning (0.1 s) and warp
  gate (1 s) initial updates are unconditional in the source and reschedule as before.
- **Effect messages.** The original engine record kept each object's persistent special-effect
  message and replayed it on a restore. OpenBlood2's record has no effect message and its
  publications are typed, so ClientLightFX (SFX_LIGHT) and the particle stream
  (SFX_PARTICLESTREAM) append the message they published, in its original field order without
  the effect ID byte, to their own block. Load publishes it again through
  `PublishLightEffect` or `PublishParticleStream`, once per owner; a failed republication
  refuses the restore. The stream keeps the descriptor as published, including the initial
  ON, as the replayed message did; the saved ON and `USRFLG_VISIBLE` then drive the client,
  whose particles and ramp clock start again. Light waveforms evaluate the absolute restored
  clock, so every phase continues; an invisible light's last evaluated colour is the client's
  and is not restored.
- **Sounds.** No voice is saved. SoundFX's Load restarts the current state's sound from its
  start (an ambient ON loops), as the original. TripLaser (lot 8) is the other class that
  restarts its own loop. RandomSoundFX starts nothing: its next wait is the restored next
  update, and the saved generator (D10) then draws the same waits, choices and offsets.
- **Instant effects.** A scene explosion's components and a lightning bolt are one-shot
  effects and are lost. A triggered explosion emitter is removed by its next update, which
  its initial update reschedules at 0.1 s.
- **Decision-4 departures.**
  - RandomSoundFX was never saveable, so its objects vanished; lot 4 marks it and its original
    Save/Load bodies now run.
  - ClientLightFX was saveable only when it started on, so an initially-off light vanished;
    lot 4 marks every light. Such a light then follows the original restore path of an on
    light (rescheduled at 0.01 s; its update only reschedules and checks the lifetime).
  - SoundFX's `PostPropRead` rescheduled a restored object from the constructor's StartOn and
    its Load ended with `SetNextUpdate(0)`, freezing RampUp, RampDown and a non-ambient ON.
    The port keeps the saved next update in both places and Load restarts the sound without
    rescheduling, so each ramp reaches its next state at the saved deadline.
  - The warp gate's `SetupFX` published three instant messages the engine never kept, and the
    restored `bFirstUpdate` is 0, so its visuals were lost. A gate saved after its first update
    publishes them again from its Load, once, through `PublishWarpGate`; a gate saved before
    it publishes at that update, as before. The client ramps start again.
- **Rollback.** A refused restore also discards the light effects, particle streams and warp
  gates that the removed records' Loads published, before the clock moves back.
- **Expected re-save rewrites.** A save right after a load reproduces every OBJS, LINK and
  ATCH byte of these classes except: the ClientLightFX family's flags and next update; the
  next update of the explosion, lightning and warp gate emitters (each then keeps polling at
  its own phase).
- **Real worlds.** The private run lists, for every mounted world, the classes still blocking
  a whole save and restores every Original object of each world through `load_game`: each
  light, stream and published warp gate comes back once, and SoundFX loops restart. With lots 7
  and 11, 06_airship1, 08_subwayb, credits, drop and end save and load whole; the other worlds
  still hold classes of lots 9 or 12 (see the evidence). The save tests share one list of the
  lots' listed rewrites (`tests/save_rewrites.h`).

### Implemented native state (lot 13)

Lot 13 covers the state that no original class saves: engine-implemented classes, the owner-bound
sky and PolyGrid publications, the client voice and communication state, and the CTF path. It adds
no payload and no class declaration.

- **Engine-implemented classes.** Every 1998 engine class (`engineobjects.c`) is `CF_ALWAYSLOAD`
  (Model, WorldModel, Container, Sprite, Sound, StartPoint, World, Brush, DemoSkyWorldModel,
  SkyPointer, FogSphere) or editor-only `CF_NORUNTIME` (the lights, OutsideDef, FastApproxArea,
  ProjectorSprite, RaySelecter). None is saved, so none needs a payload.
  - The port registers BaseClass (NotSaved) and the seven runtime classes (AlwaysLoad).
    WorldModel and FogSphere stay unregistered because no shipped world places them.
  - Their `PRECREATE_SAVEGAME` refusals stay, with corrected comments. They are unreachable. The
    fingerprint stage refuses an image holding such a record by name, because the class is not in
    the build's Original schema (`class Sound is not registered`). `RestoreObjects` would also
    refuse it by name before any precreate.
  - Always-load objects created at runtime are never saved, as in the original: VolumeBrush
    surface PolyGrids, the player's SmellHint trail and Naga's ceiling Model. References to them
    read NULL.
  - FlagObject and FlagStand stay NotSaved.
- **Republication.** Neither the sky nor the PolyGrid service is saved. After a load:
  - World PolyGrids come back with the always-load rows and publish from their own initial update.
  - Each VolumeBrush recreates its surface in its Load (lot 8).
  - Sky objects return with INSKY and their slot from the engine record, and the always-load
    SkyPointer and DemoSkyWorldModel add theirs again.

  The new `check_restored_publications` (`world_validation.cpp`) runs in `load_game`'s validation
  stage and refuses the candidate, naming the problem, if any of these fails:
  - every live sky object is registered once and holds at most one slot;
  - every slot's object is registered;
  - every published PolyGrid has exactly one instance;
  - no two volumes share a surface;
  - no PolyGrid exists beyond the authored rows and the volume surfaces.

  The PolyGrid wave phases restart, as the original client's do.
- **VoiceMgr.** The original keeps the client process across a load. `StartNewGame` calls
  `VoiceMgr.Reset`, which clears the unique-line flags and the next play time; the server's
  `PostStartWorld` stops the voice. `load_game` reproduces this when it promotes the candidate
  (`CVoiceMgr::InheritAcrossLoad`):
  - it copies the on/off state, the random generator and each group's last choice;
  - the unique flags start clear;
  - the next play time is the restored session's own, measured on the restored clock;
  - no sound handle is carried over.

  NEW_LEVEL keeps its `InheritHistory`, which also keeps the unique flags, as the original
  `StartNewWorld` does.

  This matches the original and is not a loss to fix: decision 4 does not list it. A unique
  line (a first weapon-pickup reply) can therefore play again after a load, as in the original.
- **CommLink.** The candidate client starts with an idle panel and no retained voice. A running
  conversation is lost; its ConversationTrigger carries its own state (lot 7). The original client
  kept drawing the previous game's panel over the restored world until the panel's own clock ran
  out (the original never saves it). OpenBlood2 does not reproduce that artifact.
- **CTF.** CTF saves stay refused, with a message that names CTF:
  - `save_game` refuses a CTF game type (`CTF games cannot be saved (multiplayer, out of scope)`)
    before the generic D12 check.
  - It also refuses any live CTF state: a team flag registered in `g_ctf`, a FlagObject or
    FlagStand, or a player on a team or carrying a flag. The message names the reason, and the
    store is left unchanged.
  - `load_game` refuses an image whose console holds the CTF game type at admission
    (`CTF saves cannot be loaded`).
  - The explicit local inspection of the four CTF maps is therefore never saveable.

### Implemented AI core (lot 11)

AI_Mgr's original Save/Load block (`AI_Mgr.cpp:6759-6938`), its script command list and path list,
the CultistAI, CivilianAI, Fanatic and CCorpse blocks, and every AI_Mgr-only subclass (ShikariAI,
SoulDrudge, MadScientistAI, Behemoth, Prophet, DrudgeLord, GabriellaAI/REV, OpheliaAI/REV,
IshmaelAI/REV, CalebAI, DeathShroud) save and load (`src/game/original/AISave.cpp`). The 18
registered classes are Original with payload version 1; their `PRECREATE_SAVEGAME` refusal is
gone. AI_Mgr itself is never placed and stays Unsupported. The bosses and special enemies are
lot 12.

- **Byte order.** The most derived class, AI_Mgr, CBaseCharacter (0 bytes), then lot 5's
  aggregates (CMovement with its own owner, CInventoryMgr, CDestructable). Load inserts the faction
  link (`InsertMe`) after AI_Mgr's block and before the aggregates, as the original does. Class
  blocks: CultistAI the gender byte, CivilianAI `male | labtech << 1 | scared << 2`, Fanatic the
  raw fuse clock; CCorpse its 18 hit spheres, the constructor mass (200) and hit points (50), the
  sound directory as a String and the limb-loss byte, before its CDestructable block. The other
  subclasses write nothing.
- **AI_Mgr block.** The 48 original fields in the original order and widths:
  - targets, positions, states, the six stimuli, dodge flags and the three trigger strings;
  - the four perception distances (hearing, smelling, sensing, seeing) and four speeds;
  - relative `m_fTimeStart`/`m_fLoadTimeStart`, corpse type, reload time, animating, Metacmd and
    the current command, flags, block flags, shutdown, raw `m_fLastUpdate`, injured leg,
    `m_hEnemyAttach`, scale, raw `m_fLastLedgeCheck`, current animation and dimensions;
  - the script index as a float, the raw wait deadline, script flags, movement, the update flag,
    the current command, the command list (float count) and the path list (float count, the
    constructor's `m_bDeleteElements` 0, then position, name, action target and message).

  Fields the port never sets are written as the original's constant values and must match on
  load: AI_Mgr's `m_bJumping` (only the unported `MC_Fire_Jump` sets it), `m_bStartFire` and
  `m_hEnemyAttach` for a class that never attaches. Unknown script names keep the original DONE
  mapping and load with the canonical name. Script arguments are at most 49 bytes.
- **OpenBlood2 tail** (after the path list), the port state that the original keeps in links or
  loses, as LoadSave references and bytes: attacker memory, smell cursor, sought ammunition
  pickup, pending self-removal, the target and tracked-position flags, the failed-plan latch, the
  script and motion outcomes, and a runtime failure with its text. A suspended actor therefore
  stays suspended; a script resumes its current command.
- **Restore branches.** `PRECREATE_SAVEGAME` reads no property and draws no random number; the
  engine record's model and skin apply (D13). `INITIALUPDATE_SAVEGAME` runs the original
  subclass statics and AI_Mgr's inventory and damage `Init`, then stops: no weapons, health,
  flags, faction lists, scale, animation, schedule or ground placement. Cultist and Civilian
  bind their gender table in Load, where the saved gender is known; DeathShroud restarts its loop
  and the Prophet obtains its melee weapon (replaced by the inventory Load), as the original
  initial updates do on every path. The SoulDrudge constructor's weapon draw now runs at the
  start of a fresh precreate, so a restore consumes no gameplay random number (D10).
- **Checks before change.** Every field is decoded, then checked: states, finite vectors and
  clocks, distances and speeds in 0..10⁷, booleans 0 or 1, positive scale, script movement,
  command types and a script index inside its list, no self reference, a smell cursor that is a
  SmellHint. Then members change. Counts stay readable after an earlier refusal, so the refusal
  names the first bad field.
- **References.** Target, tracked object, enemy attachment and the tail's identities are LoadSave
  references: anything outside the save list (always-load SmellHints, ExitHints and PathPoints,
  unsaved FX sources) reads back as NULL, as in the original. The target's inter-object link comes
  back with LINK. Script paths are owned snapshots, so a restored script needs no PathPoint
  identity.
- **Clocks.** `m_fTimeStart` and `m_fLoadTimeStart` are relative (lot 8's inverse); the script
  wait deadline, Fanatic's fuse and the ledge clock are raw absolute, valid because a full restore
  keeps the clock.
- **Losses kept.** As the original: `m_fWaitForIdleTime`/`m_bHasTargeted` (the 120 s idle guard),
  `m_szAIState` and the other init-only properties, the AIShared strafing state, the fire and
  smoke sources of a burning death or corpse, the current voice and every one-shot. Fanatic's
  smoke source is recreated by its Load and republished by the next update; the NPC's held weapon
  model is rebuilt by its first update.
- **Departures.**
  - Decision 4 (listed): the original Load adds `GetTime()` to the raw `m_fLastLedgeCheck`,
    which suppresses every ledge check for about the saved game time. The port loads it raw.
  - Decision 4 (found in this lot, player-visible): the attacker memory (the port's checked
    replacement of the damage aggregate's linked `m_hLastDamager`) is restored, so an NPC hit just
    before a save still retaliates; and a pending self-removal is kept (AI_Mgr's `m_bRemoveMe`,
    CCorpse's `m_bDead`), so a save between a corpse transfer or a gib and the removal does not
    bring back a second corpse or a gibbed one.
  - Structural: the port's shared tables must never be poisoned by another model (ai_runtime), so
    Cultist and Civilian skip the original's constructor-default male table pass in
    `INITIALUPDATE_SAVEGAME`; the port's CDestructable has no animation pointer to leave stale.
- **Expected re-save rewrites.** AI_Mgr's Load adds `GetTime()` to the raw `m_fLastUpdate`
  (original quirk, benign: the first update's desync test is false once); CCorpse's PostPropRead
  restores its constant flags. Every other byte of these classes is identical after a load.
- **Damage block.** An NPC's precreate admits an authored destruction spawn that fails
  validation (its death spawns nothing): 14_SubwayC's Prophet asks for `LifeSeedPU` and
  16_Bridge's Fanatic for `BullettammoPU`. CDestructable's block now carries such a request for
  an AI_Mgr owner instead of refusing the save; every other owner still refused it at creation.
- **Real worlds.** 35 of the 58 worlds hold lot-11 NPCs; all of them restore every supported
  object through `load_game` and the NPCs keep running. No lot-11 class blocks a world any more;
  each world is still blocked by classes of lots 9, 10 or 12 (the evidence lists them).

### Implemented bosses and special enemies (lot 12)

The original Save/Load bodies of Gideon and EnhancedGideon (shield duration, raw last-shield, full
health, raw last-panic), UndeadGideon (full health), AncientOne (`m_fLastFire`, always the
constructor's 0, and full health), AncientOneTentacle (raw time of death, the constructor's
`m_fLastIdle` 0, scale, hiding), Naga, ZealotAI, BoneLeech with Thief and TheHandAI, and
DrudgePriest (raw last spawn) (`src/game/original/BossSave.cpp`). The 11 classes are Original with
payload version 1. Each block is written before AI_Mgr's (lot 11).

- **Initial updates.** Gideon's and EnhancedGideon's run on every path, as the original's: health
  bar re-armed, Gideon's full health from the current hit points (then overwritten by Load), gravity
  stripped after the base. DrudgePriest restarts its ambient loop and strips gravity. Naga creates and
  attaches its spike and sets alpha 0 on every path; it clears SHADOW and SOLID for a fresh object
  only, as the original does. The fire node (`rr_gun`), the boss bars' re-arm flags and
  EnhancedGideon's `m_bNoFire` are not saved.
- **BoneLeech, Thief, TheHandAI.** One block each (the port's Thief and TheHandAI derive from
  BoneLeech; the original writes the block once): the jump flag (the port's FIRE_JUMP command), the
  raw damage and detach clocks. AI_Mgr's `m_hEnemyAttach` slot holds the attached player. An
  OpenBlood2 tail follows: the tracked player (LoadSave; its link comes back with LINK) and the world
  flags and dimensions the replacement detach restores. After the aggregates, the original
  re-attachment runs: the creature points at the player again, restarts `STATE_EnemyAttach` (the
  attach handshake and the client acknowledgement are replayed, as `m_bProceedAttach` is not saved)
  and takes `FLAG_REALLYCLOSE` without gravity. An attached record whose target is not the player
  is refused.
- **Naga.** The whomp count is written as a DWord and read as a Float, as the original does: a
  small count reloads as 0 (a listed rewrite). The spike is a plain object reference: the NULL
  sentinel (D7). Alpha is re-applied with the current colour.
- **ZealotAI.** The packed float, staff-start and divine byte and the never-assigned
  `m_fMaxHeight` (written 0). The staff effect is an unsaved BaseClass; a restored cast keeps
  its start flag with no staff, which the next update clears (the original's calls on the NULL
  object do nothing).
- **Inventory.** The damage aggregate's typed validator now admits strengths up to 10 (Gideon's);
  players keep 1–6 through ValidateAttributes.
- **Departures.** Decision 4: the original Naga Load overwrites the spike its initial update just
  created with the stale plain handle, orphaning a visible model that no longer hides or shows.
  The port keeps the new spike.
- **Expected re-save rewrites.** Naga's whomp count (above). A creature attached to the player
  restarts its attach state on load (Metacmd 1, its last state, no animation), so its record differs
  until the handshake has run again.
- **Real worlds.** With lots 11 and 12 every AI_Mgr class is Original. All 35 worlds that hold NPCs
  restore every supported object, the bosses included, through `load_game` and their NPCs keep
  running; the remaining blockers are lot 9's pickups and projectiles and lot 10's effect publishers
  and emitters (the evidence lists them per world).

### Certified whole worlds (lot 14)

The private certification runs every original rule above on the retail data, world by world
(`tests/save_audit_tests.cpp`, `tests/train_session_tests.cpp`).

- **Worlds.** The 36 single-player worlds are every mounted world outside `multi/`: 01_subwaya
  to 25_ancientone (with 24b_arena), credits, drop and end. Multiplayer maps are out of scope (D12).
- **Audit** (`save_audit.worlds`). Each world starts as a New Game at difficulty 1; the test also
  runs at difficulties 0 and 2. The test sets the player's original CDestructable god-mode flag,
  which is saved like any other field, so a hostile world cannot end the run early. Two cycles run:
  - *entry*: the level-entry snapshot is taken as the application takes it, then the cycle runs.
  - *combat*: the three nearest live non-boss NPCs take player-attributed damage, two lethal and one
    wounding, and the world runs three seconds (corpses, drops, gibs, effects). Then the cycle runs.

  Each cycle saves to slot 0 through the real files, reads the file back, loads it and compares the
  live census of saved classes. It saves again: OBJS, LINK and ATCH must equal the file's apart from
  the listed rewrites. It then runs 30 seconds (600 steps of 50 ms): every step must succeed, with no
  NPC runtime failure and no unavailable service.
- **Census.** Saved classes must match exactly. Always-load objects come back in their authored
  state (rule 2), so their differences are reported but not failures: Keys a KeyFramer consumed
  reappear, SkyPointers that removed themselves after binding return until their first update, and
  the runtime SmellHint trail is lost.
- **Restart after a load** (`save_audit.restart`, SubwayA). The level-entry snapshot is taken, then
  a slot ten seconds later that carries it as AUTO. Loading the slot and then Restart, twice, puts
  the player back at the entry position, and a save matches the entry image.
- **Mid-route save** (`train_session.private_save`). The scripted SubwayA route saves at the
  train's midpoint, round-trips the container bytes, loads and drives on to the authored exit and
  NEW_LEVEL into CityHub, with health and ammunition kept.
  - Handles are new after the load: wounded Cultists are carried by name, dead ones by count.
  - Per-session inspection counters (messages, fire keys, music messages) start again with the
    restored session and are summed across the load.
  - `gpu.train_session_save` renders and reads back the restored game.
- **Fixed here.**
  - A level-entry snapshot is refused while a deferred removal from initialization is pending
    (07_cathedral). The snapshot now finishes that frame first ([lot 6](#implemented-slots-quick-and-restart-lot-6)).
  - `save_effects.private` now accepts lot 13's CTF refusal.
  - Two music routing gaps ended 11_slaughter_house and 25_ancientone whether or not the game had
    been saved: a key outside every region of Track1's program 13, and the missing program 1. A
    received note in either case is now silent and counted, in two separate commits
    ([routing contracts](music_routing_contracts.md)).
- **Original loss kept.** The original creates an NPC's held weapon model only in
  `CInventoryMgr`'s MID_UPDATE, and an inactive object receives none. An NPC that was deactivated at
  the save therefore comes back unarmed until it wakes. One killed before it wakes drops nothing:
  `DropCurrentWeapon` refuses it with `CHWEAP_NOTAVAIL`, silently, as the port does. Decision 4
  does not list this loss, so it is preserved.
- **Limits.** This certifies that saves restore equivalent engine records and run on healthily. It
  is not retail parity, the combat is synthetic damage rather than play, and it does not exercise
  every authored script.

## Restored and regenerated state

| State | Treatment |
| --- | --- |
| Saveable objects: engine record and original payload | Restored |
| Console variables, clock, global force, gameplay generator | Restored before objects (D9, D10) |
| Interlinks and attachments between saved objects | Restored after all Loads |
| Client blob (objectives, music level, fade, wonky, blind, game type) | Restored through the player record, as the original |
| Always-load world objects (start points, path points, keys, hints, WorldProperties, PolyGrid, ParticleSystem, SparksObj, LightFX, built-in Model/Sprite/Container/Sound/sky) | Recreated from the DAT in authored state; references to them read NULL |
| Server voices, music transport | Never saved. Classes restart their own loops in Load or initial update; DAT `Sound` objects replay; music restarts at the saved level |
| Client VoiceMgr and CommLink | Never saved. The unique-line flags and next play time reset as after the original `StartNewGame`; the on/off state, randomness and group choices continue; the communication panel starts idle ([lot 13](#implemented-native-state-lot-13)) |
| Owner-bound effects (PolyGrid surfaces, particle streams, warp gates, object particles, light effects, camera views, sky) | Republished by the restored owner through the same publication path the original's saved effect message fed; lights, particle streams and warp gates since [lot 10](#implemented-effect-publishers-and-audio-emitters-lot-10), the warp gate through a decision-4 departure |
| Detached cosmetic effects (explosions, lightning bolts, sparks, spurts, splats, fragments, gibs, flame/smoke, laser beams, weapon sprites) | Lost, as in the original |
| HUD, crosshair, view weapons, MoveMgr prediction, zoom | Rebuilt: the player Load zeroes stat caches, the first update republishes, and ForcePos resets prediction |
| Path manager, AI shared animation tables, container index, posed shells, collision | Rebuilt from restored objects and the DAT |
| Runtime SmellHint trail, start-point name, cheats, unsaved per-class members | Lost, as in the original |
| Held weapon models | Recreated by each NPC's first update after the load, as the original's `CInventoryMgr` MID_UPDATE does; a deactivated NPC stays unarmed until it wakes ([lot 14](#certified-whole-worlds-lot-14)) |
| Inspection history, diagnostics, reports | Never saved; the new session starts empty |

## Failure policy

- **Save failures** leave the game running and the previous slot file intact:
  - refused preconditions;
  - an unsupported class;
  - a payload over its bound;
  - an impure callback;
  - an I/O error.

  The menu shows the reason.
- **Load failures** before step 2 change nothing. Later failures destroy the
  candidate and keep the source. In both cases the menu shows the reason, and
  the slot is neither deleted nor rewritten. There is no partial world, no fresh
  fallback player, and no partially written slot or current-level file.
- **Unreadable slot files** are listed as unreadable, with the reason (version,
  CRC, fingerprint, bound). Stale temporary files are ignored, then removed at
  the next successful write of that slot.
- **Crash safety.** An atomic rename means a crash leaves either the old file or
  the new one. Like `preferences.cfg`, no fsync durability is claimed.
- **Device loss.** GPU allocation or device loss after CPU promotion remains an
  application error, as for NEW_LEVEL.

## Test strategy

Each lot adds its tests to the existing per-area executables and labels. Every
lifetime- or dispatch-changing lot runs the object contract suite and the Clang
ASan/UBSan recipe from [testing](../testing.md). Every application-lifetime lot
also builds and runs the inspector-free Release preset.

1. **Primitives and container** (headless). Golden byte layouts per primitive and
   section. Truncation at every offset of a small file. Single-bit flips. Oversize
   lengths. Newer versions. Bad section order. Nested-message bounds. Refusal of
   reference primitives outside the save/load context.
2. **Engine records** (headless, synthetic classes). Round trips of transforms,
   flags, delays, activity, deactivation, animation and sprite tracks, sky slots,
   links and attachments. NULL for references outside the list. Pending removals
   excluded. Clock-first restore. A refused precreate fails the load. Over- and
   under-reads fail.
3. **Per-class round trip** (headless; private where real assets are needed):
   1. Build the object in a nondefault state, with references to saved and
      unsaved objects.
   2. Save, restore into a fresh candidate, and compare fields.
   3. Save again: the bytes must be identical apart from that class's listed
      original rewrites.

   Each preserved original quirk, and each departure, has its own named check.
4. **Purity.** The object snapshot, clocks, generator, voices and identities are
   identical before and after a save. An impure test class fails the save.
5. **Failure isolation.** A failure injected at each load step leaves the
   source's lists, identities, clocks, voices and revision unchanged and the AI
   tables restored. It leaks no objects, strings or voices under ASan. Covered
   failures include a missing class, a version mismatch, a payload error, a
   resource failure, a Load failure in the middle, a missing local player and an
   audio transfer failure.
6. **Whole worlds** (private).
   - For each of the 36 single-player worlds, at entry and after simulated
     combat: save, load, compare object snapshots, and save again with bytes
     identical apart from the listed rewrites. Then continue for 30 seconds
     with no runtime failure.
   - The scripted SubwayA → CityHub route saves mid-level, loads and still
     reaches the exit.
   - Restart returns to the level-entry state after a load.
7. **Application** (gpu, menus). The menu smoke drives slot save, quicksave,
   quickload, Restart, an error modal and death to the Load menu. A GPU check
   renders and reads back after a restore. Both inspector and inspector-free
   builds run it.

## Implementation plan

Sizes include tests and documentation: **S** ≤ 400 changed lines, **M** 400–1,200,
**L** 1,200–3,000, **XL** > 3,000. Every lot is one commit (or a short series),
keeps all existing tests green, and updates this contract's status line. Its
contract changes and source ledger record are part of the lot.

| Lot | Content | Depends on | Size |
| --- | --- | --- | --- |
| 1 | **Save-message primitives.** The full original `CSBase` set (Byte, Word, Float, Vector, Rotation, String, nested messages, LoadSave references through an index context), exact consumption, plain-object NULL policy, and refusal outside save/load dispatch. Engine and adapter only; no behavior change. | — | M |
| 2 | **Container and fingerprint.** Section writer and reader, CRC, bounds, version policy, META quick read, directory and index digests, DAT digest, class schema digest. The atomic write helper is shared with preferences. | — | M |
| 3 | **Engine records and clock.** Capture and apply the engine record. `Objects` clock restore on an empty registry. Relative delay and deactivation. Links, attachments, sky, animation and sprite tracks. Creation-order save list. Real `SaveObjects`/`RestoreObjects` over an in-memory logical store (`Current`, `Auto`, `Keep`). Two-pass dispatch and purity checks, tested with synthetic classes. | 1, 2 | L |
| 4 | **Restore-mode world and candidate transaction.** `CF_ALWAYSLOAD` flags on the original classes and engine built-ins. Restore-mode `load_world` with `SavedState` dispositions. `GameSession::save_game`/`load_game`: candidate build, NEW_LEVEL isolation and promotion, failure destruction. The original `USRFLG_SAVEABLE` marking on every registered class (the port sets it inconsistently today), so a saveable class without support is refused rather than silently dropped. The save-support declaration, with every class Unsupported except always-load and NotSaved ones. | 3 | L |
| 5 | **Player and client.** Original byte Save/Load for CPlayerObj (including power deadlines and imprisonment), CBaseCharacter, CMovement, CInventoryMgr and items, CDestructable. These reuse the existing typed validators and keep the typed NEW_LEVEL path working. Also the client blob and `SMSG_CLIENTSAVEDATA`, the local-client flag and restored association, and the shell console overrides. | 4 | L |
| 6 | **Slots, Quick and Restart, menus.** Save location, slot and current-level files, META naming, the level-entry snapshot and AUTO embedding. Enabled Save/Load rows with original labels. Quick keys, death to the Load menu, notices and error modals, and refusal listing unsupported classes. Inspector rows. | 5 | M |
| 7 | **World logic.** Trigger, BigTrigger, ToggleTrigger, ExitTrigger, ObjectivesTrigger, CinematicTrigger, ConversationTrigger, Spawner, KeyFramer/KeyData, CameraObj. | 4 | L |
| 8 | **Movers and brushes.** Door, RotatingDoor, the Rotating aggregate and rotators, CDestructableBrush, CDebris, CDestructableModel, WorldModelDebris, VolumeBrush and its types, TripLaser, Tchernotronic. | 4 | L |
| 9 | **Pickups and projectiles.** PickupObject and every pickup, KeyPickup with its PickupObject block, CProjectile and the projectile family, grenades and bombs, SeeingEye with the CInvTheEye reference. | 5 | M |
| 10 | **Effect publishers and audio emitters.** SoundFX, RandomSoundFX, the ClientLightFX family, ClientExplosionSFX, ClientParticleStreamSFX, ClientLightningSFX, ClientWarpGateSFX, DetailSprite. Owner-bound effect republication. | 4 | L |
| 11 | **AI core.** AI_Mgr including script and path lists, CultistAI, CivilianAI, Fanatic, CCorpse, and every AI_Mgr-only subclass (Shikari, SoulDrudge, MadScientist, Behemoth, Prophet, DrudgeLord, Chosen and REV, Caleb, DeathShroud). | 5 | XL |
| 12 | **Bosses and special enemies.** Gideon, EnhancedGideon, UndeadGideon, AncientOne, AncientOneTentacle, Naga, ZealotAI, BoneLeech, Thief, TheHandAI, DrudgePriest. | 11 | L |
| 13 | **Native state.** Engine-implemented classes that are not always-load, sky and PolyGrid republication checks, VoiceMgr and CommLink client state, and the CTF refusal path. | 4 | M |
| 14 | **Whole-world certification.** The private audit over all 36 single-player worlds, the mid-route SubwayA save, Restart after load, GPU readback, sanitizers, inspector-free Release. Roadmap 3.3 is then marked done. | 6–13 | M |
| 15 | *Optional:* NEW_LEVEL on `SaveObjects`/`RestoreObjects` with dwParam 1 and a player-only list, retiring the typed capture once equivalence tests pass. | 5 | M |

Lots 1 and 2 are independent. After lot 4, lots 7, 8, 10 and 13 touch separate
files and can proceed in parallel; lots 9 and 11 wait for the player (lot 5),
and lot 12 follows lot 11. Lot 6 can ship before the class families, because a
save containing an Unsupported class is refused with its name.

```
1, 2 → 3 → 4 → 5 → 6
           4 → 7, 8, 10, 13
               5 → 9, 11 → 12
                   5 → 15 (optional)
6, 7, 8, 9, 10, 11, 12, 13 → 14
```

## Decisions

The user accepted all seven recommendations below on 2026-10-20. They are
binding for the implementation lots; the alternatives are kept for history only.

1. **Save location.** The recommendation is the per-user preferences directory
   (`SDL_GetPrefPath("OpenBlood2", "Blood2")`), under `saves/retail/`:
   - `slot0`–`slot7`, `quick` and `current` (the Restart source), each with the
     `.ob2sav` extension;
   - never inside the game installation, which may be read-only or shared with
     retail saves.

   Alternatives: one directory per installation, or the installation's `Save`
   directory.
2. **Quicksave behavior.** The recommendation is to keep the original F5
   (quicksave without pausing, overwrite without confirmation, a brief
   "Quicksave..." notice) and F6 (quickload, with a notice when the quick slot is
   empty). The F2/F3 menu shortcuts stay unbound, because OpenBlood2 already uses
   F2 (keyboard preference) and F3 (mouse capture).
3. **Original `.sav` compatibility.** Recommended out of scope, as above. A future
   read-only importer would first need a 1998 record-layout study.
4. **Original save losses.**
   - The recommendation is to keep the original payload layout and quirks, and to
     fix as recorded departures only the losses a player would notice:
     - RandomSoundFX and initially-off lights vanish;
     - running spawners stop;
     - SoundFX ramps freeze;
     - WarpGate visuals are lost;
     - VolumeBrush forgets the player class;
     - AI ledge checks are suppressed after a load.
   - Alternative: strict reproduction of every loss.
5. **Mod or game-data mismatch.** The recommendation is to refuse and show the
   difference. Alternative: warn and allow an explicit load. A different world
   DAT or class schema is always refused.
6. **Compatibility across OpenBlood2 versions during the beta.** The
   recommendation is to refuse saves whose class payload versions differ, with
   no migration code before 1.0. Beta testers are told that saves may not
   survive an update.
7. **Demo edition.** The recommendation is to keep saves disabled, as in the
   original. The demo's Enter-after-death retry stays separate and could later
   use the level-entry snapshot.

## Limits

This contract does not certify retail engine parity for record contents, list
order or timing. The 1998 engine's own layout and ordering are unknown, and
later-runtime evidence is corroboration only. Saves never contain object
pointers, handles, resource pointers or platform-dependent data. Save/load does
not reopen M2 or clear M2-GPU-001.
