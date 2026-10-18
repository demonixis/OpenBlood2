# Implementation records

This directory holds work-in-progress history and source evidence. Start with the
[developer guides](../readme.md) for routine development and the
[contracts](../contracts/readme.md) for subsystem behavior.

| Record | Location |
| --- | --- |
| Imports and portability adaptations | [Provenance](provenance.md) |
| Donor selections, commits and hashes | [Source ledgers](sources/readme.md) |
| Current M2 progress and limitations | [Workspace M2 report](../../../Documentation/Implementation/m2.md) |
| M0 / M1 validation | [M0](../../../Documentation/Implementation/m0.md), [M1](../../../Documentation/Implementation/m1.md) |
| Increment reports | [Workspace implementation directory](../../../Documentation/Implementation) |
| Logs, measurements, patches and validation ledgers | [Workspace evidence directory](../../../Documentation/Implementation/evidence) |
| Milestone acceptance criteria | [Roadmap](../../../Documentation/Roadmap/03_milestones.md) |

Workspace links require the parent documentation repository; it is separate from
Blood2 and is not a build dependency. Keep the existing reports/evidence there
instead of copying them into both repositories. New source ledgers belong in
`sources/`; new reports and measured results belong in the workspace implementation
and evidence directories. Keep the top-level `docs/` for developer guides.

Reports are chronological evidence, not blanket support claims. M2 remains
incomplete. Preserve failed attempts, platform limitations and historical hashes;
later verified records may supersede an earlier limitation without rewriting
the original log or patch.

- [DLS diagnostic voice synthesis](../../../Documentation/Implementation/m2_music_sampler.md): sample/envelope auditions, shared audio clock and Cabal refusals; M2 remains incomplete.

- [Legacy DLS pan compatibility](../../../Documentation/Implementation/m2_music_legacy_pan.md): all 38 Cabal region auditions, isolated original-dispatch evidence and explicit remaining music gaps.

- [IMA pattern-note decoding](../../../Documentation/Implementation/m2_music_notes.md): original loader equivalence, 7,030 inspected note records and explicit remaining sequencing gaps.
- [IMA band decoding and channel setup](../../../Documentation/Implementation/m2_music_bands.md): original executable equivalence and separate style/section inspection.
- [IMA sections and authored timing](../../../Documentation/Implementation/m2_music_sections.md): original header/event-reader equivalence and explicit remaining sequence semantics.

- [IMA style meter and section timeline](../../../Documentation/Implementation/m2_music_timeline.md): original reconciliation, explicit missing style bindings and pending sequence playback.

- [IMA pattern channels and variations](../../../Documentation/Implementation/m2_music_variations.md): original loader and 786 private decisions; live music remains unavailable.

- [IMA note event preparation](../../../Documentation/Implementation/m2_music_events.md): original post-harmonization arithmetic and diagnostic inspection; live MUSIC remains unavailable.

- [IMA harmonic note conversion](../../../Documentation/Implementation/m2_music_pitch.md): original executable comparison, explicit contexts and remaining live-music limits.

- [IMA prepared harmony](../../../Documentation/Implementation/m2_music_harmony.md): original chord classification, external scale metadata and inspected bindings; MUSIC remains unavailable.

- [IMA section and pattern decisions](../../../Documentation/Implementation/m2_music_selection.md): original lookup/request/choice, explicit tick inspection and remaining live-music gaps.

- [IMA grid cursor](../../../Documentation/Implementation/m2_music_cursor.md): original consecutive-grid decisions, inspected manual traversal and explicit remaining playback gaps.

- [IMA effective band selection](../../../Documentation/Implementation/m2_music_band_selection.md): original section/style priority, prepared configuration and owned inspection.

- [IMA band channels and DLS note routing](../../../Documentation/Implementation/m2_music_routing.md): original packed/key lookup, percussion fallback and controller-aware diagnostic PCM; MUSIC and M2 remain incomplete.

- [Connected IMA grid notes](../../../Documentation/Implementation/m2_music_grid_notes.md): atomic normal-mode pitch/event preparation and executable comparison; MUSIC remains unavailable.
