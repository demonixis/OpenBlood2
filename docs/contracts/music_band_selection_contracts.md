# IMA section band selection

`select_ima_band` reproduces the installed loader's initial effective band choice.
Inputs are immutable, reader-produced section and optional referenced-style band
catalogues. Output is an optional source scope/index, cleared on failure. This is
preparation for original music; no controller messages are sent and MUSIC remains
unsupported.

## Priority and ordering

A direct section AABN loaded after the direct `secn` header overrides the style.
Each subsequent eligible section band replaces the previous one, so the last
eligible file-order entry wins. Its byte104 does not affect this selection.
Names, collection labels and program values do not participate in the choice.

Without a loaded section band, use the section's validated referenced style.
The original style loader prepends each AABN loaded after `styl`; its default-band
getter walks that reversed list and returns the first band with nonzero runtime
`+0x38`, the sign-extended file byte104. Thus the last eligible file-order entry
with **any nonzero byte104**, including a negative value, wins. A style with no
marked band returns no band; do not fall back to its head or to another style.
Byte105 and channel byte154 remain uninterpreted.

Raw readers still retain bands preceding their owning header for inspection.
Those entries are excluded here, matching the original loader's initialization
guard. Missing/duplicate owning headers, wrong root forms, more than 256 bands or
8,192 source chunks refuse selection. Readers retain their existing stricter
byte/name/RIFF budgets; selection performs no file reads, allocation of runtime
objects, RNG or mutation of source data.

The original no-section-band/no-style branch constructs a default GM band. That
branch remains explicitly unsupported here; it is not replaced by a guessed DLS
instrument or a successful empty setup. The catalogue already requires validated
style decoding/identity/meter before exposing a style fallback. This is stricter
than the historical general resource resolver and remains visible as failure.

## Ownership and inspection

Each decoded section in `MusicCatalog` stores the selected scope/index, filename,
and an aliasing shared pointer into the immutable owning `ImaBands`. No second
band copy or raw borrowed lifetime is introduced. World unload/reinspection drops
these owners. A separately retained alias keeps only its source catalogue alive.

The existing exact-path style binding is used; `InstrumentFiles`' bootstrap style
is not an implicit fallback. A section's own band may be selected even when its
style or timeline failed, but those failures remain and prevent live sequencing.
Selecting a band does not establish instrument collection availability.

`ima_band_setup` prepares its 80 ordered messages once per catalogue survey. Its
failure does not erase a valid selection or retry another band: selection errors
and controller-range errors are distinct. Failed setup is zeroed and marked
unavailable. The inspector shows the selected name, source file, scope/index,
setup readiness and failures. Drawing, hidden UI and inspector-free builds do not
change selection or ownership.

See [band data/setup](music_band_contracts.md),
[source ledger](../implementation/sources/m2_music_band_selection_sources.json) and
[verification](../../../Documentation/Implementation/m2_music_band_selection.md).
Live tempo/transport, controller application and curves, DLS instrument routing,
active-note lifetime, original playlists/transitions and M2 acceptance remain open.

[Selected-band routing](music_routing_contracts.md) now applies these prepared values to owned diagnostic MIDI snapshots and the configured DLS bank. Automatic transport, residency and MUSIC remain unfinished.
