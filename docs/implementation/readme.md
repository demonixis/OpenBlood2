# Implementation records

- [Default launcher build, sibling executable and window names](../../../Documentation/Implementation/launcher_default_build.md)

- [Compact launcher, demo delivery and data mods](../../../Documentation/Implementation/launcher_redesign.md)

- [SDL3 launcher baseline and validation](../../../Documentation/Implementation/launcher.md)

- [Demo font metrics and spawned-Fanatic placement retakes](../../../Documentation/Implementation/demo_retakes.md)

- [Unused SDL subsystem configuration and validation](../../../Documentation/Implementation/sdl_build_options.md)

- [CityHub museum jump diagnosis; production defect remains open](../../../Documentation/Implementation/cityhub_jump.md)

- [Crash stacks and core-dump validation](../../../Documentation/Implementation/crash_capture.md)

- [Diagnostic file logging validation](../../../Documentation/Implementation/logging.md)

- [Gib impact audio correction](../../../Documentation/Implementation/gib_impact_audio_fix.md)

- [Original menu validation](../../../Documentation/Implementation/original_menus.md)

- [Original crosshair validation](../../../Documentation/Implementation/crosshair.md)

- [Original statistics HUD validation](../../../Documentation/Implementation/original_hud.md)

Current decision and criterion matrix: [M2 closed with reservation M2-GPU-001](../../../Documentation/Implementation/m2_acceptance.md), authorized on 2026-10-18. Functional acceptance and the unresolved external GPU sanitizer failure remain distinct. M3 is next and is not yet accepted.

This directory holds work-in-progress history and source evidence. Start with the
[developer guides](../readme.md) for routine development and the
[contracts](../contracts/readme.md) for subsystem behavior.

| Record | Location |
| --- | --- |
| Imports and portability adaptations | [Provenance](provenance.md) |
| Per-world entry gaps and implementation batches | [Level gaps](level_gaps.md) |
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

Reports are chronological evidence, not blanket support claims. Historical
open-M2 statements do not override the closure decision above. Preserve failed
attempts, platform limitations and historical hashes;
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

- [IMA active notes and due stops](../../../Documentation/Implementation/m2_music_active_notes.md): original lifetime/dispatch-order comparison and inspected pending tails; M2 remains incomplete.

- [IMA style curve decoding](../../../Documentation/Implementation/m2_music_curves.md): 47 curves checked against the original reader; evaluated controllers and MUSIC remain incomplete.

- [IMA style curve events](../../../Documentation/Implementation/m2_music_curve_events.md): original expansion checked against 1296 generated cases and 47 installed points; MUSIC remains unavailable.

- [IMA ordered grid dispatch](../../../Documentation/Implementation/m2_music_grid_dispatch.md): 9198 grid visits checked against original filtering and emission order; MUSIC remains unavailable.

- [DLS live voice controls](../../../Documentation/Implementation/m2_music_live_controls.md): actual sample-voice changes without restarting clocks; original MUSIC transport remains incomplete.

- [Immediate MIDI channel delivery](../../../Documentation/Implementation/m2_music_midi_receiver.md): real voice mutation and original lifetime selection, with explicit transport limits.

- [Timed MIDI delivery](../../../Documentation/Implementation/m2_music_timing.md): queued notes/controllers on the existing audio clock; IMA transport and MUSIC remain incomplete.

- [IMA runtime clock](../../../Documentation/Implementation/m2_music_clock.md): checked original tempo conversion; running sections/playlists and MUSIC remain incomplete.

- [Finite IMA section playback](../../../Documentation/Implementation/m2_music_program.md): original terminal-note flushing and complete authored audio; playlists/MUSIC remain incomplete.

- [Finite music playlists](../../../Documentation/Implementation/m2_music_playlists.md): original level decisions and one-pass transition/list audio; live MUSIC remains incomplete.

- [Repeating music transport](../../../Documentation/Implementation/m2_music_transport.md): transition once, continuous prepared lists and bounded audio ownership; M2 remains incomplete.

- [Live MUSIC messages](../../../Documentation/Implementation/m2_music_messages.md): original player/client requests, real immediate/next-song playback and truthful pending/failure inspection; M2 remains incomplete.

The [session-cost audit](../../../Documentation/Implementation/m2_session_cost.md) removes temporary trace-name allocations and zero-depth audio modulation work. Read [contracts](../contracts/session_cost_contracts.md); preserve owned counters, exact scalar PCM/cursor progression and all existing session/test deadlines. The full M2 acceptance review and runtime-start guard remain open.

- [Checked session startup](../../../Documentation/Implementation/m2_session_start.md): actual admission/refusal checks, shared NEW_LEVEL resource validation and startup inspection; M2 remains incomplete.

- [GPU lifetime attribution](../../../Documentation/Implementation/m2_gpu_lifetime.md): SDL-only and standalone Vulkan controls for the retained NVIDIA LeakSanitizer failure.

- [Demo two-world implementation and validation](../../../Documentation/Implementation/demo_playable.md), [phase-2 menu audit](../../../Documentation/Implementation/demo_menu_audit.md): original Voodoo/Focus, menu-free pause/retry/end; no new milestone acceptance.

- [Demo menu implementation and validation](../../../Documentation/Implementation/demo_menu.md): explicit edition provider, limited source branches, promotions and shared pause/settings; original-client visual parity remains unverified.
