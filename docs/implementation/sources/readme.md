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
