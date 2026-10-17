# Original music asset inspection

This is a prerequisite for the original interactive music service, not a player
or synthesizer. `MUSIC LEVEL 3`, reached by the continuous train route, remains an
explicitly unsupported command. A valid container never implies successful
`InitMusic`, `LoadSong`, `InitInstruments` or `PlayList`.

## Original request plan

`CMusic::BuildAssetPlan` selectively adapts the client `Music.cpp` initialization
into owned values. WorldProperties supplies MusicDirectory, InstrumentFiles,
AmbientList, CruisingList and HarddrivingList. InstrumentFiles requires the two
original arguments, DLS then its initial style (also documented by client_de.h).
The three transition arrays retain their source order, each loaded in reverse
index order; the hard-driving ending really is **cts.sec**, repeated from the
cruising group. SilenceList follows, then the three authored playlists without
sorting or deduplicating their entries.

The bounded parser consumes the first semicolon-delimited command with quoted
arguments, as the original single Parse call did. Empty lists are admitted;
missing properties, malformed quotes, NULs, instrument/playlist strings over
500 bytes and tokens of 80 bytes or more fail. Paths must be relative, with no empty/dot/parent
components or drive prefix. This admitted profile requires section basenames
with `.sec`, the instrument `.dls` and the initial `.sty`. It is not a complete
historical console parser or the CD-audio branch.

This plan is **inspection**, not original initialization execution. The inspector
continues past a missing file to account for later assets; the original loader
would stop on its first failed engine call. No source music state, RNG, simulation
clock or playback result is changed.

## External files and structural reader

A successful `Resources::mount_game` retains the configured content root; a failed
mount leaves the previous resources/root intact. Direct individual REZ mounts do
not infer a music root. Music remains loose external data below that root and is
not mounted into the REZ index or copied into the repository.

Lookup matches the entire relative path with ASCII case folding, rejecting
ambiguous matches and symlinked components below the configured root. There is no
basename search across directories, executable loading or current-directory
fallback. Limits are eight components, 4,096 directory entries per component,
256 unique assets, 32 MiB per file and 64 MiB of reserved read buffers per survey.
Failed reads consume the reservation; actual delivered bytes are counted
separately. Complete reads must match the measured file size. The content root
itself may be a user-selected symlink; files must remain stable during inspection.
This is not a concurrent file-modification security boundary.

The replacement C++ reader checks exact RIFF length/form (`AASE` section, `AASY`
style, `DLS ` instrument bank), every nested RIFF/LIST range, odd-byte padding,
16 nesting levels and 8,192 chunks. Offsets/counts are owned metadata with no
retained input spans. All output is cleared on failure. Unknown leaf records are
retained as opaque ranges; their presence is not semantic format support.

Each section requires exactly one direct `sref`, observed in the installed
resources as a terminated UTF-16LE style basename. The admitted reference subset
is nonempty ASCII, at most 79 characters, with safe basename validation. Embedded
NULs, odd byte lengths, non-ASCII values and unsafe paths fail. Inspection follows
that name plus `.sty` in the same music directory using the same case/ambiguity
rules. This mapping is verified against the installed files, not a claimed
reimplementation of the historical IMA loader's complete resolution policy.

Unique file reads are deduplicated while the original request sequence remains
visible. Pattern and note-chunk counts count **records**, not played notes.
`secn`, `styl`, `band`, `patt`, `note`, `curv`, `chrd`, `cmnd`, `mute`, `pref`,
personality references and DLS instrument semantics remain opaque. In particular,
`.PER` personality files are not RIFF and are neither parsed nor certified by
this reader. A structurally valid file is not proven synthesizable.

## Session lifetime and inspection

After original world objects and PostStartWorld have established their variables,
the session takes one survey. The World sounds tab displays its original request
order, unique files, sizes, chunk/pattern counts, section style references and
failures. Rendering reads the snapshot only. Player entry and frame updates do
not rescan files, and hiding/disabling ImGui does not change the survey.

Unload clears the plan/results. A new world, destination rehearsal or NEW_LEVEL
owns a fresh survey and never retains source-world objects, byte spans or file
handles. Missing music does not newly prevent world entry: music playback was
already unavailable, and the inspection does not convert that failure to success.

## Remaining work

Implement and verify the original section/style event semantics and personality
references, DLS instrument/sample synthesis, timing and original playlist/level
transitions before accepting the engine music calls. Do not substitute recorded
soundtrack files or a silent success. CD audio, motifs and breaks require their
own source review (the supplied client motif/break helpers are no-ops). M2 remains
incomplete.

The [DLS continuation](music_dls_contracts.md) now decodes a bounded instrument-data
subset alongside the structural survey. Earlier blanket DLS opacity statements
are superseded only for that subset; synthesis and music playback remain absent.

The [IMA note continuation](music_note_contracts.md) now decodes a bounded AASY
pattern/grid/note profile alongside this structural survey. Its separate failures,
opaque fields and read-only data do not implement sequence playback or MUSIC.

The [section continuation](music_section_contracts.md) now exposes bounded authored
section timing, commands, mute and chord records. It does not reconcile the style
meter, schedule events, resolve personality or implement MUSIC playback.
