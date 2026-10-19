# Desktop launcher contracts

The 2026-10-19 [compact launcher continuation](../../../Documentation/Implementation/launcher_redesign.md)
supersedes the initial L0-L2 interface and the future graphics/profile/installer
sections below. The main menu and separate Game Settings/Mods panels are now
implemented. The requested graphical settings panel is cancelled. Nightmares,
M3 and the M2 GPU reservation remain separate.


Current scope extends L0-L2 with the user-requested compact panels, local data
mods and qualified demo installation. The launcher uses the existing pinned
SDL3, SDL_GPU and Dear ImGui. Native dialogs and HTTP/ZIP dependencies are
isolated in launcher builds. Game SDL options, shaders and rendering remain
unchanged. Nightmares, graphical preference editing, M2/M2-GPU-001 and M3 stay
outside this continuation.

Window geometry is not persisted; version-1 launcher state retains executable
and installation records. Native Windows/macOS and interactive portal
qualification remain pending.

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
Preference serialization stays in the game; the launcher editor was cancelled. No original launcher code
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
profiles=rez1
editions=retail,demo
```

Output is limited to 4096 bytes; unknown/duplicate/missing fields, invalid values,
nonzero exit and output overflow fail. `preferences=2` describes the game's codec,
not an available launcher editor. `profiles=rez1` means ordered data-only REZ overlays via repeated `--rez`.
The parser still accepts `profiles=0`; enabled mods require `rez1`.
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

## Data mods

`mods.cfg` is a separate version-1 UTF-8 literal key/value file (64 KiB maximum,
24 records). Fixed key `version=1`; contiguous `mod.N` records contain exactly
`edition=retail|demo`, `root=ABSOLUTE_GAME_DIRECTORY`, `path=ABSOLUTE_REZ` and
`enabled=0|1`. Unknown/duplicate/noncontiguous fields, invalid paths and duplicate
canonical archives within one root/edition reject the whole state. Saves use the
existing exclusive temporary/atomic replacement helper and refuse malformed or
future existing files. Changing installations never activates another one's mods.

New archives start disabled. The UI persists toggles and passes only enabled
archives to both content validation and playable process arguments. Every game
startup mounts them after base/sound/patch archives in list order, with the same
last-exact-path-wins semantics. Maximum 24 overlays (within the reader's 32 mounts).
Canonical/equivalent duplicate base/mod files, empty archives, `BLOOD2NM.REZ` and
archives containing executable/DLL resources fail. No dynamic code is loaded;
renamed data cannot be certified as compatible from its filename. Nightmares
support and original mod-specific game rules have not been implemented.

All mount changes remain transactional. The launcher revalidates immediately
before spawning, and the game independently validates at startup. Existing
Resources copies/menu/world transfers retain the selected index. The game's
configuration struct adds only ordered overlays, not display/preference fields.
The earlier proposed profile-manifest format remains unimplemented and is not
advertised by `rez1`.

## Qualified demo delivery

Provider: `https://archive.org/download/B2demo/B2Demo.zip`, 37,695,395 bytes,
SHA-256 `2f22bed184ed2a9c39b448041603ee7199310f83cfa791e1a26cd4bdc67b9bc1`.
Extracted `Game/B2Demo.rez`: 55,041,396 bytes, SHA-256
`e410ce2944210c776c7407a79e01cfa031c782132f68407cd0871379c5e7218c`.
This is the already tested v1.0 Build 108 data. ModDB lists a different v1.0a
package; it has no qualified extraction recipe here.

Download only after user action; use TLS peer/host verification, HTTPS only,
maximum five redirects, and only `archive.org` or its subdomains (no credentials
or nonstandard port). Stream to owned staging with an exact byte ceiling,
15-second connect/600-second overall timeout and a 30-second low-speed limit.
Reject non-200 final responses, wrong length, SHA mismatch and partial files.
A dedicated owned SDL thread publishes progress/results without touching ImGui.
Cancel is atomic; shutdown waits for bounded work and releases the worker.

The miniz reader limits compressed data to 64 MiB, expanded data to 128 MiB,
256 entries and 1024-byte UTF-8 paths. Before extracting anything, validate all
entry paths, including skipped setup entries: no absolute/traversal paths,
links/special files, duplicate/case-colliding entries, file/directory collisions,
Windows device names or trailing-dot/space components. Reject encryption and
unsupported compression. Stream extracted regular files with exclusive creation,
per-entry bounds and CRC validation. Only the qualified `Game/` prefix is removed;
all its files, music and notices are kept. PE/Windows setup code is never executed.
The original ZIP is retained outside Game for provenance/notices.

Extraction targets a fresh staging directory below the SDL user storage, then
checks the REZ SHA and shared content index before same-filesystem rename to a
unique published directory. Existing directories are never intentionally replaced.
Failed/cancelled operations clean only their own staging. The UI saves/selects
the published demo, validates the game and returns to Main. Persistence failure
reports the installed path but preserves the previous saved selection. A process
crash can leave an unreferenced staging directory; it cannot become a selected
installation. No resume or automatic abandoned-staging deletion is implemented.

## Presentation

Compact main menu with Play disabled without validated content. Game Settings
owns the folder/edition, download/import and an Advanced executable selector;
Mods owns local overlay toggles. No graphical preference editor. Default title
art is project-owned; optional external RT_BITMAP `GAME1` (retail) or `BLOOD2`
(demo) uses a bounded data-only PE32 reader, single-language resource selection
and exact 320x240 8-bit uncompressed DIB decode. Bad/missing artwork falls back
without blocking Play. GPU textures belong to the launcher view and are released
before device shutdown. Droid Sans is embedded from the pinned ImGui archive,
with its Apache license shipped. The game renderer and SDL dependency pins stay
unchanged.

## Verification

Synthetic tests isolate state, query and process fixtures; private tests use
external retail/demo directories. Native/GPU tests are opt-in and report actual
backend/device separately from headless results. Test Debug, inspector-free
Release, malformed/cancelled operations and lifetime under Clang sanitizers.
Do not suppress GPU leak failures or treat M2-GPU-001 as resolved.
