# Source ledgers

- [Compact launcher continuation](launcher_redesign_sources.json): qualified demo, optional external artwork, data overlays and launcher-only dependencies.

- [Launcher L0 through L2](launcher_sources.json): existing project config/REZ
  reuse, pinned SDL/ImGui, no original launcher-code import.

These JSON files record selected donor code, revisions, source ranges, hashes and
adaptations for implementation increments. [Provenance](../provenance.md) links
them to the corresponding feature; [contracts](../../contracts/readme.md) describe
the supported behavior and lifetime rules.

Paths retain their original repository/workspace meaning. Moving a ledger does
not make its `source` or `target` fields relative to this directory. Use the donor
repository/root named in each record; targets normally refer to the Blood2
repository root. Hashes describe the recorded snapshot, not necessarily HEAD.

The documentation reorganization preserves every existing JSON file byte for
byte. Add a new record for a new source selection; do not refresh historical
hashes merely because an implementation file or its documentation links changed.
Runtime test logs and measurements belong in the
[dedicated evidence directory](../reports/evidence).

- [Save/load audit](save_load_audit_sources.json): read-only audit of original and later-LithTech save/restore code; no import.
- [Save-message primitives](save_load_messages_sources.json): lot 1 CSBase declarations and Save/Load call patterns; no implementation imported.
- [Save/load container](save_load_container_sources.json): lot 2 container and fingerprint; new code, no original source read or imported.
- [Save/load engine records](save_load_records_sources.json): lot 3 SaveObjects/RestoreObjects; original file names and flags consulted, no implementation imported.
- [Save/load candidate transaction](save_load_session_sources.json): lot 4 declarations, marking, shell SaveGame/RestoreGame and the restore-mode world; selective shell excerpts recorded in provenance.
- [Save/load player and client](save_load_player_sources.json): lot 5 CPlayerObj, CMovement, CInventoryMgr and CDestructable Save/Load, the client blob and the restored client; selective excerpts recorded in provenance.
- [Save/load movers and brushes](save_load_movers_sources.json): lot 8 Save/Load bodies of doors, rotators, destructible brushes and models, debris, volumes, TripLaser and Tchernotronic.
- [Save/load slots and menus](save_load_slots_sources.json): lot 6 slot, quick and Restart rules, Load/Save menu rows, F5/F6 and death to the Load menu.
- [Save/load world logic](save_load_world_logic_sources.json): lot 7 Save/Load bodies of the triggers, Spawner, KeyFramer/KeyData and CameraObj.
- [Save/load effects and emitters](save_load_effects_sources.json): lot 10 Save/Load bodies of SoundFX, RandomSoundFX, the light family, scene explosions, particle streams, lightning, warp gates and DetailSprite.
- [Save/load native state](save_load_native_sources.json): lot 13 engine class flags, PolyGrid/VolumeBrush surfaces, the VoiceMgr reset and CommLink on a load, and the multiplayer save guard.
- [Save/load AI core](save_load_ai_sources.json): lot 11 AI_Mgr, script and path lists, CultistAI, CivilianAI, Fanatic, CCorpse and the AI_Mgr-only subclasses.

- [Original music asset preparation](m2_music_asset_sources.json)

- [DLS decoding references and private format quirks](m2_music_dls_sources.json)

- [Legacy DLS pan source ledger](m2_music_legacy_pan_sources.json): installed binary observations, independent implementation and validation limits.

- [IMA pattern notes](m2_music_notes_sources.json): independent reader, original binary identity and bounded loader probe.
- [IMA bands](m2_music_bands_sources.json): independent reader/setup planner and original executable probe.
- [IMA sections](m2_music_sections_sources.json): bounded header/event readers and original authored-timing arithmetic.
- [Session cost](m2_session_cost_sources.json): project scalar baseline, transparent trace lookup and exact differential checks; no new original-code import.

- [Session startup source ledger](m2_session_start_sources.json): engine validation extraction and unchanged original shell/player call paths.

- [GPU lifetime diagnostic sources](m2_gpu_lifetime_sources.json): project teardown baseline, pinned SDL APIs and system Vulkan probe; no original-game import.

- [M2 acceptance](m2_acceptance_sources.json): original missing-zero sound selection, inspection provenance and stronger selected-session checks; no graphics-stack change.

- [Crypt, Bridge, Entrance and Lobby sources](later_levels_sources.json): four original enemies, Minigun, particle systems and bounded authored-data adaptations.
