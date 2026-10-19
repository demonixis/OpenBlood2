# Source ledgers

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
[dedicated evidence directory](../../../../Documentation/Implementation/evidence).

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
