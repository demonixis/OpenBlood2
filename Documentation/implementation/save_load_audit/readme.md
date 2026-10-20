# Save/load audit (roadmap 3.3, phase 0)

This is the source audit behind the [save/load contract](../../contracts/save_load_contracts.md).
It was made by reading code on 2026-10-20, with no code imported or executed except
the existing `--inspect-world` diagnostic used for world class counts. The
[source ledger](../sources/save_load_audit_sources.json) records the hashes of the
77 audited reference files.

Paths starting with `Blood2/` or `runtime/` are relative to the LithTech reference
tree (`lithtech/`). Paths starting with `src/` or `Documentation/` are relative to
this repository at `8c1ad35b`. Line numbers refer to those snapshots.

## Evidence classes

- **Original game source** (`Blood2/ObjectDLL`, `ClientShellDLL`, `Shared`): the
  2.x/Nightmares-era code. It already contains the 2.0 patch's load/save fixes.
- **1998 SDK headers** (`Blood2/AppHeaders`): API, flags and documented semantics.
  They do not describe the engine's save-file layout.
- **Later LithTech runtime** (`runtime/server/src/game_serialize.cpp` and friends):
  corroborates engine ordering and engine-owned fields. It is not a trace of the
  1998 executable; [engine.md §11](engine.md#11-unknown-or-unverified-for-1998)
  lists the 15 points it cannot settle.
- **Retail 2.1 installation** (private, not committed): one real `Save/Current`
  (`Auto.sav`, `Save.dat`) and the readme. Its `Auto.sav` header starts with
  1113, not the later runtime's 2001/2002, so the 1998 record layout is unknown.

Statements marked INF are inferences from code that was not executed.

## Files

| File | Content |
| --- | --- |
| [engine.md](engine.md) | Later-runtime `SaveObjects`/`RestoreObjects`: record layout, handle encoding, two-pass restore, world models, clock, sounds, client refs, keep-alive, error hazards, 1998 header differences |
| [shells.md](shells.md) | Original server/client flow: `Save\Current` files, slots, `Save.dat`, message ids, save/load/Restart/autosave/quick keys, keep-alive interplay, client blob, failure behavior, multiplayer guards, retail notes |
| [characters.md](characters.md) | Field-by-field Save/Load for CBaseCharacter, CDestructable, CMovement, CInventoryMgr and items, AI_Mgr, every NPC and boss with its own Save, CCorpse, Tchernotronic, SeeingEye, CPlayerObj |
| [world_objects.md](world_objects.md) | Field-by-field Save/Load for cameras, triggers, doors, rotators, keyframers, brushes, debris, client-SFX publishers, sounds, spawner, TripLaser, VolumeBrush, pickups and projectiles |
| [class_catalog.md](class_catalog.md) | All 224 compiled ObjectDLL classes by save category and restore fate, OpenBlood2's 192 registrations, gap tables, native-state mapping and hierarchy divergences |
| [openblood2_map.md](openblood2_map.md) | What OpenBlood2 already has (typed player/inventory/damage state, NEW_LEVEL transaction, message layer, menus) and every engine/client state family a whole-world save must cover |

## Key findings

1. **Only `USRFLG_SAVEABLE` objects are saved**, active then inactive, players
   included. A restore loads the world **without** its objects except
   `CF_ALWAYSLOAD` classes (14 game classes and the engine built-ins), which come
   back in their authored state. Everything else that was neither saved nor
   always-load disappears.
2. **Restore runs in two passes**: every object gets `PRECREATE_SAVEGAME` and
   `INITIALUPDATE_SAVEGAME`, the engine then overwrites its own fields, and only
   when all objects exist does each receive `MID_LOADOBJECT`; links and
   attachments follow. Save-list references are indices; anything outside the
   list reads back as NULL.
3. **A full restore restores the game clock** (`RESTOREOBJECTS_RESTORETIME`), so
   the many absolute deadlines saved raw stay valid. Keep-alive restores do not;
   they carry only players, whose times are relative or durations.
4. **Engine-owned state** (transform, flags, user flags, dims, model and
   animation, colour, physics, the next-update countdown, activity, the special
   effect message, attachments, interlinks) is saved by the engine, not by game
   code. Sounds are never saved; client effects are rebuilt from the saved effect
   message or lost.
5. **dwParam** is 0 for a full save and 1 for keep-alive. Only
   `CInventoryMgr::Load` reads it (it drops keys).
6. **The client's state travels inside the player's record**: a nested blob with
   objectives, music level, fade, wonky and blind state, re-broadcast on load.
7. **Restart** restores `Auto.sav`, the snapshot taken right after the latest
   NEW_GAME or NEW_LEVEL entry. It travels with every slot copy.
8. **Original failures are mostly silent**: a failed save soft-locks the client,
   restore results are ignored, slot copies happen before the load is known to
   succeed, and the error printer has an inverted check.
9. **The original has save bugs and losses** that a faithful port reproduces
   unless a departure is decided:
   - RandomSoundFX and initially-off lights vanish.
   - Running spawners stop; SoundFX ramps freeze.
   - WarpGate visuals are lost.
   - VolumeBrush forgets the player class.
   - AI ledge checks are suppressed for about the saved game time.
   - Naga's whomp counter is written as a DWord and read as a Float.
   - Many projectile timers are not saved.

   See [characters.md §23](characters.md#23-cross-cutting-findings) and
   [world_objects.md §27](world_objects.md#27-cross-cutting-summary).
10. **OpenBlood2 today**:
    - It has no persistence.
    - The message layer carries only DWords and HStrings.
    - `SaveObjects`/`RestoreObjects` refuse.
    - No ported class keeps its original Save/Load; about 26 refuse
      `PRECREATE_SAVEGAME`.
    - There is no way to load a world without its objects, and the engine clock
      cannot be set.

    The NEW_LEVEL transaction already builds and promotes a replacement session
    without touching the source on failure. That is the base for loading.
