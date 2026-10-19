# Desktop launcher contracts

Scope: L0 through L2 of the [launcher plan](../../../Documentation/Roadmap/06_launcher.md).
The launcher uses the existing pinned SDL3, SDL_GPU and Dear ImGui. Native dialogs
are enabled only in dedicated launcher builds; existing game SDL options, shaders
and rendering stay unchanged. Nightmares, mods, preference editing and demo
installation are not executable features of L0 through L2. M2/M2-GPU-001 and M3
status are unchanged.

Implementation and Linux test coverage are recorded in the linked evidence.
Launcher window geometry is not persisted in L1/L2; version-1 state stores only
the executable and installation records. Native Windows/macOS and interactive
portal qualification remain pending.

## L0 baseline and shared boundary

Baseline: Blood2 `8fcbf35`, with gameplay/code unchanged from `d149d325`.
The unchanged development config, menu preference, demo flow/rules and demo-menu
manifest/flow tests pass 12/12; [raw evidence](../../../Documentation/Implementation/evidence/launcher-l0-baseline.log).
SDL revision remains `6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1`, ImGui
`c1aaac65e896a45f81f47a4d68f9959e0f86c859`; archive hashes and the SDL patch
remain in `cmake/Dependencies.cmake`.

`blood2_startup` shares the current config and REZ implementation, with existing
`engine/config.h` and `engine/resources.h` interfaces retained. Game targets link
it through `engine`. The launcher does not link `blood2_game` or scene code.
Preference serialization stays in the game until L3. No original launcher code
was supplied in the reference tree; this frontend is new project-owned code.

## Capability protocol version 1

The game accepts standalone `--launcher-info` before crash helpers, logging,
configuration lookup, SDL initialization or content access. Other arguments in
that invocation are rejected. UTF-8 stdout contains exactly these newline-ended
key/value fields, once each, in any order; successful exit code is zero:

```text
protocol=1
game=OpenBlood2
version=0.3.0
revision=BUILD_REVISION
preferences=2
profiles=0
editions=retail,demo
```

Output is limited to 4096 bytes; unknown/duplicate/missing fields, invalid values,
nonzero exit and output overflow fail. `preferences=2` describes the game's codec,
not an available launcher editor. `profiles=0` means no mod launch support.
The query has a five-second deadline and bounded per-update reads. Cancellation
or timeout stops only the query process. Its pipes are drained before waiting;
the playable game's output is inherited, never piped into an unserviced buffer.

## Launcher state version 1

Store `launcher.cfg` under SDL_GetPrefPath("OpenBlood2", "Launcher"). The grammar
is UTF-8 `key=value`, split at the first equals sign, with blank lines and lines
beginning with `#` ignored. Values are literal; paths are not shell expressions.
Reject embedded CR/LF/NUL, unknown/duplicate keys and unsupported versions.
Maximum file size is 64 KiB, paths 4096 bytes, and installations 32. Save complete
validated state through a unique sibling temporary and atomic replacement.
Reject malformed/future existing state instead of silently overwriting it.

Required fixed keys: `version=1`, `game=ABSOLUTE_EXECUTABLE_OR_EMPTY`,
`selected=INDEX_OR_MINUS_ONE`. Each contiguous `installation.N` has exactly
`.edition=retail|demo` and `.path=ABSOLUTE_DIRECTORY`. Empty installations require
selected -1. Relative paths and duplicate edition/directory records fail. Missing
state means defaults. Failed loading/saving never partially mutates published
state. Local commercial paths belong only in this user file, not Git.

## Content and process ownership

Edition selection is explicit, even if a directory contains both archives.
Validate readable metadata then atomically mount through the existing REZ parser.
Keep retail optional sound/patch behavior and exact-path order. Mount demo alone;
never mount Nightmares. Index validation is distinct from menu decoding, service
support and full gameplay compatibility. Revalidate immediately before spawn.

The packaged game is resolved from SDL_GetBasePath; a user-specified local game
path supports development layouts. Query capabilities before launch. Generate an
absolute launcher-owned bootstrap INI with only `data_dir`; pass absolute
`--config`, `--data-dir` and `--log-dir`, adding `--demo` only for demo. No
`--start-game`, diagnostic, world or arbitrary shell arguments are injected.
Game initialization remains authoritative and may fail after process creation.

Only one playable child is tracked per launcher. Repeated Play cannot spawn
another while querying, validating or playing. Hide the launcher while playing,
suspend rendering, then restore it on exit. Preserve nonzero exit status and log
location. Closing the launcher releases tracking without killing the game.
Retire and release the swapchain before hiding, then reclaim the window on
return; a hidden native surface must not retain pending presentation work.
Validation uses an owned SDL thread, with explicit creation failure and
cooperative cancellation after the bounded archive operation finishes.
SDL dialog callbacks own shared completion state, publish under a mutex, and
never reference UI objects or mutate ImGui. Late results after closure are ignored.

## Frozen future interfaces

L3 will reuse version-1/2 `preferences.cfg`, preserving all audio/mouse fields.
An ownership guard must cover externally started games and recover after crashes.
No graphics keys are added to the current bootstrap INI.

L4's profile grammar will use the same literal key/value rules, a 64 KiB limit
and at most 32 overlays. Required keys: `version=1`, `edition=retail|demo`,
`root=PATH`; optional `name`, `start_world`, `requires_engine` and contiguous
`overlay.N=REZ_PATH`. Resolve paths relative to the manifest. Reject duplicate
canonical archives, unknown requirements, expansion/code requests and conflicting
CLI edition/root. This freezes the planned format, not a working parser or mod
compatibility claim. Nightmares remains separate.

L5/L6 require independent ZIP/provider qualification; no downloader or installer
is included in L0 through L2. Existing tested demo content is v1.0 Build 108.

## Verification

Synthetic tests isolate state, query and process fixtures; private tests use
external retail/demo directories. Native/GPU tests are opt-in and report actual
backend/device separately from headless results. Test Debug, inspector-free
Release, malformed/cancelled operations and lifetime under Clang sanitizers.
Do not suppress GPU leak failures or treat M2-GPU-001 as resolved.
