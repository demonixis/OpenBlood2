# Save and load

Status: **design contract for roadmap task 3.3. Lot 2 (container and fingerprint) is
implemented, see its [evidence](../implementation/reports/save_load_container.md); the other
lots are pending.** The Save and Load screens stay disabled until the lots below land. The
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
  must equal the first same-named model in file order);
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

`WriteToMessageCompVector` and `WriteToMessageCompPosition` appear only in
original effect messages, never in Save bodies, and are refused in a save
context. The reader latches its first error. The original reader rewinds
automatically at the end of a message; inside `MID_LOADOBJECT` that rewind is
disabled, so a read past the end is an over-read. Over-reads and under-reads are
failures (D5). An original read whose type differs from its write is reproduced
literally; Naga's DWord-written, Float-read counter keeps that original
behavior unless a departure is recorded.

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
deliberately rewrites. Examples: ClientLightFX resets its flags, SoundFX clears
its next update, and CPlayerObj hard-sets flags and zeroes velocity. Each class
lists its expected rewrites. This is the primary round-trip property tested
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

## Restored and regenerated state

| State | Treatment |
| --- | --- |
| Saveable objects: engine record and original payload | Restored |
| Console variables, clock, global force, gameplay generator | Restored before objects (D9, D10) |
| Interlinks and attachments between saved objects | Restored after all Loads |
| Client blob (objectives, music level, fade, wonky, blind, game type) | Restored through the player record, as the original |
| Always-load world objects (start points, path points, keys, hints, WorldProperties, PolyGrid, ParticleSystem, SparksObj, LightFX, built-in Model/Sprite/Container/Sound/sky) | Recreated from the DAT in authored state; references to them read NULL |
| Server voices, music transport | Never saved. Classes restart their own loops in Load or initial update; DAT `Sound` objects replay; music restarts at the saved level |
| Owner-bound effects (PolyGrid surfaces, particle streams, warp gates, lightning, object particles, light effects, camera views, sky) | Republished by the restored owner through the same publication path the original's saved effect message fed. Whether WarpGate keeps the original's lost visuals is open decision 4 |
| Detached cosmetic effects (explosions, sparks, spurts, splats, fragments, gibs, flame/smoke, laser beams, weapon sprites) | Lost, as in the original |
| HUD, crosshair, view weapons, MoveMgr prediction, zoom | Rebuilt: the player Load zeroes stat caches, the first update republishes, and ForcePos resets prediction |
| Path manager, AI shared animation tables, container index, posed shells, collision | Rebuilt from restored objects and the DAT |
| Runtime SmellHint trail, start-point name, cheats, unsaved per-class members | Lost, as in the original |
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
