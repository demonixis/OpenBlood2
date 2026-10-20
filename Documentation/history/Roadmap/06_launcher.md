# SDL3 launcher implementation plan

The [default-build continuation](../../implementation/reports/launcher_default_build.md)
also includes the launcher in normal game builds, preserving separate SDL
configurations and placing the two executables beside one another.

**Scope revision, 2026-10-19:** the user replaced the initial interface with a
compact desktop menu (Play, Game Settings, Mods, Quit), requested demo download
and automatic installation/selection, and cancelled the graphical settings
panel. This [implemented continuation](../../implementation/reports/launcher_redesign.md)
adds ordered data-only REZ toggles and the qualified Archive.org v1.0 ZIP.
Nightmares remains separate. The detailed L0-L7 proposal below records the
initial plan; L3 graphics editing is cancelled, L4's profile-manifest design is
not the current simpler `--rez` interface, and L5/L6 have the bounded provider
implementation documented in the current contracts. M2/M3 decisions are unchanged.


Date: 2026-10-19. L0 through L2 are implemented with the Linux verification scope
recorded in [launcher validation](../../implementation/reports/launcher.md). L3 through L7
and the later display extension remain planned.

Build a small C++20 desktop launcher using SDL3, SDL_GPU and Dear ImGui. It will
select external Blood II or demo data, start OpenBlood2, edit supported graphics
preferences, select compatible data mods, and install the original demo. Keep
the useful Launch, Display and Customize functions of the original launcher.
Nightmares support is a separate project and is excluded from these steps.

This plan does not reopen [M2 or M2-GPU-001](../../implementation/reports/m2_acceptance.md),
accept M3, or authorize graphics optimization. The remaining steps are future work.
Changes to the game's graphics startup must wait for the other computer's
graphics work to be pushed and reviewed.

## Scope and delivery boundaries

| Delivery | Included | Boundary |
| --- | --- | --- |
| Foundation | Desktop window, optional original illustration, installation selection, validation and Play | Launch OpenBlood2; no Wine wrapper for the commercial client |
| Shared preferences | Existing detail and model-lighting settings, round trips with the in-game menu | Preserve audio and mouse values |
| Data mods | Ordered REZ overlays, profiles and an optional custom starting world | No historical DLLs, new game classes, expansion rules or mod catalogue |
| Demo installation | Import a qualified ZIP; download from a qualified provider | Start with the tested v1.0 edition and preserve music and notices |
| Later display extension | Window size, borderless fullscreen and VSync | Requires coordinated game startup changes; not required for V1 |

Desktop targets are Linux, Windows and macOS, qualified independently. The
desktop child-process design does not establish an Android or iOS launcher;
mobile content selection and application lifecycle need a later design. VR,
multiplayer, save management and engine auto-updates are outside this work.

## Verified starting point

The examined pre-launcher Blood2 baseline is `d149d325c06a99c2aa238be199e5906e839270e5`.
Recheck these interfaces against the implementation checkout before each step.

| Area | Existing behavior and evidence | Consequence |
| --- | --- | --- |
| Original launcher | No launcher project in `sources/blood2`. Installed `readme.txt`, section 5, describes Launch, Display, Customize and Advanced; installed `BLOOD2.EXE` has matching dialog resources. | Preserve evidenced functions without claiming a source port or interactive visual parity. |
| Original settings | [MenuSetDetail](../../../../sources/blood2/ClientShellDLL/BloodClientShell.cpp) consumes launcher console variables and CFGs; [MenuDisplay](../../../../sources/blood2/ClientShellDLL/MenuDisplay.cpp) writes display values to `autoexec.cfg`. | Do not reproduce obsolete DirectX switches as new-engine controls. |
| Startup | [Application dispatch](../../../src/app/main.cpp) and [frontend](../../../src/app/frontend.cpp) already open the retail or demo menu. `--start-game` is a headless diagnostic. | Ordinary Play uses normal startup. |
| Local INI | [config.cpp](../../../src/engine/config.cpp) accepts only `data_dir`; unknown/duplicate keys fail. The file is read before CLI overrides. | Keep this bootstrap format intact. |
| Preferences | [menu_preferences.cpp](../../../src/game/menu_preferences.cpp) reads versions 1 and 2 of `preferences.cfg` under `SDL_GetPrefPath("OpenBlood2", "Blood2")`; v2 includes `detail` and `pixel_lighting`. | Share the existing codec and preserve unrelated values. |
| Display startup | [runtime.cpp](../../../src/engine/runtime.cpp) creates a resizable HiDPI 1200 by 780 window, chooses SDL_GPU automatically and requests VSync. | Resolution, fullscreen and VSync controls need new game support. |
| Content | [resources.cpp](../../../src/engine/resources.cpp) mounts retail base, sound and two patch REZs in order, or only `B2Demo.rez`. Later mounts override exact normalized paths. | Use one mounting policy for menu and sessions; keep editions separate. |
| Detection | [inspect_data](../../../src/engine/config.h) checks presence, readability and size only. | `--check-data` success is not parsed-resource or gameplay validation. |
| Build | [Dependencies.cmake](../../../cmake/Dependencies.cmake) pins SDL3/ImGui and builds their SDL3/SDL_GPU backends; `SDL_DIALOG` is OFF. | Isolate the launcher's native-dialog build. |

Existing [menu](../../contracts/menu_contracts.md),
[demo menu](../../contracts/demo_menu_contracts.md),
[demo](../../contracts/demo_contracts.md) and
[world](../../contracts/world_contracts.md) contracts remain the
behavioral references.

## User flow

Use one resizable window with an optional illustration and a compact control
panel. Main controls: installation, mod profile and Play, followed by Graphics,
Game data, Install demo, Help and Quit. Keep archive details and logs on the
data/diagnostics page. Keep labels separate from logic for later localization.

On first use, offer Choose existing installation, Import demo archive and
Download demo. If the provider is not qualified, explain that state beside the
download action and keep import available. Supply both a native folder chooser
and an editable path field.

Selecting a directory discovers editions but never merges them. Remember the
explicit edition choice. Show missing or invalid files with their path and
reason. Enable Play after required content checks pass; call this content
validation, not full campaign compatibility. Nightmares may appear as detected
on the data page, with no launch action.

While the game runs, hide or minimize the launcher and suspend drawing. Retain
process tracking, restore the launcher on exit, and expose the game log on
failure. Closing the launcher must not kill a running game. Downloads and
extraction report progress/cancellation without blocking UI events.

## Architecture and interfaces

Add a `blood2_launcher` executable, producing `blood2-launcher`, and a small
`blood2_startup` library shared with the game. These are proposed target names.
The launcher does not link original game objects, simulation or scene rendering.

Move only demonstrated shared responsibilities into `src/startup/`: installation
metadata, content-profile validation, existing bounded REZ index/mounting code,
preference serialization and path conversion. Preserve existing callers with
small adapters where useful. Do not duplicate `Resources` or introduce another
gameplay compatibility validator. Keep format reading separate from policy.

The launcher owns UI state, process tracking, transfer/extraction jobs and its
versioned `launcher.cfg` in the SDL user directory for `OpenBlood2/Launcher`.
Store installation records, last selection and launcher window state there;
do not duplicate game graphics preferences. Use plain structs, enums and explicit
boolean/error outputs in `OpenBlood2`, without a plugin framework or IPC service.

Use CMake, Ninja, FetchContent and spdlog. Preserve current dependency pins;
resolve SDL main and record its SHA only for a coordinated dependency update.
[libcurl](https://curl.se/libcurl/) for HTTP and
[miniz](https://github.com/richgel999/miniz) for ZIP are proposed additions for
demo installation. Pin revisions/archive hashes at implementation, build only
needed features and retain their notices.

### Native dialogs and build isolation

Use [SDL folder](https://wiki.libsdl.org/SDL3/SDL_ShowOpenFolderDialog) and file
dialogs. Asynchronous callbacks publish owned results to the main thread and
never touch ImGui or a destroyed window. Distinguish cancellation, errors and
late completion after closing a page.

Provide separate launcher build presets, with game targets disabled and
`SDL_DIALOG` enabled. Existing game presets retain their SDL options. Both
builds use the same dependency manifest and bounded SDL patch; do not link two
SDL builds into one executable. Package the separately built programs together.
Manual entry remains usable if a Linux portal or another dialog backend fails.
Name the proposed presets `launcher-debug` and `launcher-release`, with separate
build directories; add `launcher-sanitize` for the supported Clang sanitizer host.

### Startup and process contract

Resolve the packaged game executable from the launcher installation, independent
of the shell's working directory. Use SDL's process API with an argument vector,
preserving Unicode and spaces; never concatenate a shell command.

Stock Play passes an absolute `--data-dir` and an explicit absolute `--config`
pointing to a valid launcher-owned bootstrap INI containing only `data_dir`.
An unrelated working-directory INI therefore cannot break launch. Add `--demo`
for the demo. Ordinary Play does not add diagnostic flags or `--view-world`.

L4 introduces `--content-profile PATH` for an explicit manifest. It must agree
with the selected edition/data root; reject conflicts. Only a profile with a
custom starting world adds `--view-world --world NAME`. Resource-only mods
retain the original menu route. These profile interfaces do not exist today.

Validate with the shared content policy before spawn, then repeat checks in the
game before mounting. Process creation is not successful game initialization.
Track spawn failure, normal/nonzero exit and lost tracking separately. Drain
captured output continuously or inherit it, so a full pipe cannot freeze the
game. Reap or detach children correctly. V1 has no arbitrary-arguments field.

Introduce a headless `--launcher-info` query before configuration/content lookup.
It returns a bounded versioned key/value record with the build identity,
supported profile/preference schemas and available launcher controls, without
creating a window or mixing logs into its output. The launcher checks this
record asynchronously with a timeout before offering unsupported features.
Unknown protocols fail explicitly. This proposed option gives executable
compatibility checks a concrete interface; it is not present at the baseline.

Launcher content validation covers directory metadata, archive indexes and the
selected world's presence when specified. The game remains responsible for
decoding required menu/world assets and enforcing service support. Keep these
levels distinct in diagnostics and surface child startup failures; passing the
launcher checks never certifies full resource or gameplay compatibility.

### Shared graphics preferences

| V1 setting | Effective behavior |
| --- | --- |
| Low detail | Vertex-lit world, model shadows off |
| Medium detail | Lightmaps on, model shadows off |
| High detail | Lightmaps on, model shadows on |
| Model lighting | Per vertex or per pixel, as applied by the current frontend |

Move preference data/codec into the shared library without changing defaults,
version-1 migration, version-2 values, ranges or atomic saves. Preserve audio and
mouse fields. Keep `--no-model-shadows` as an explicit override. Do not rely on
`--pixel-lighting`, which normal frontend preferences replace.

Add a shared preference ownership guard: launcher edits are unavailable while
a current game owns preferences, including games started outside the launcher.
Reload before editing/applying and reject concurrent modification rather than
overwrite it. Guard recovery must work after abnormal process exit. Apply saves
preferences; Cancel writes nothing.

Later startup settings should extend this same typed configuration and versioned
codec. Do not add graphics keys to the bootstrap INI merely to match historical
appearance. In-game menu saves must preserve future window settings as well as
existing unrelated fields.

### Compatible data mod contract

A bounded, versioned UTF-8 profile declares edition, base installation, ordered
REZ overlays, optional custom starting world and required engine/profile-schema
compatibility. Use flat key/value fields and numbered overlay keys; freeze the
grammar in L0. Resolve relative paths from the manifest directory. Unknown or
duplicate keys, archive aliases, edition conflicts and unsupported requirements
fail explicitly.

Mount existing edition archives first, then overlays in listed order. The same
resolved profile reaches menu resources, fresh games, custom-world entry, death
retry and destination preparation. A transition must not fall back to stock
mounting. Keep base REZs immutable and overlays outside the commercial install.

Initially support replacements and custom worlds using already ported formats,
classes and rules. Reject code-DLL and expansion requirements. Unknown legacy
mods may be imported for inspection, but not labelled supported. Parsing a REZ
does not establish that every world works. Give a real mod a tested label only
with named scenario evidence; synthetic overlays prove the mounting contract.

### Demo provider and installation contract

The first target is v1.0 Build 108, already used by
[demo verification](../../implementation/reports/demo_playable.md). Its local `B2Demo.rez`
is 55,041,396 bytes, SHA-256
`e410ce2944210c776c7407a79e01cfa031c782132f68407cd0871379c5e7218c`.
This is the REZ digest, not the downloadable ZIP digest.

[RGB's page](https://www.classicdosgames.com/game/Blood_II:_The_Chosen.html)
lists v1.0 and v1.0a. The inspected v1.0 ZIP exposes `Game/B2Demo.rez`,
`Game/Music/` and `Game/Readme.txt` directly. Its directory and README were
inspected without downloading the whole archive; the full remote ZIP SHA-256
and complete remote REZ equivalence still need qualification. Other versions
or repacked archives need separate recipes and evidence.

A provider record fixes URL, edition/build, ZIP length/SHA-256, required extracted
paths/hashes, work limits and extraction recipe. Check original notices and
provider terms before selecting it. [RGB's terms](https://www.classicdosgames.com/about.html)
request permission for direct download links; this plan does not approve automatic
integration of that URL. Use an authorized provider, or leave Download unavailable
with a landing-page link. Do not describe the demo as freeware.

Import and download share one installer: acquire ZIP, validate, extract required
REZ/music/notices to staging, validate with shared readers, then publish a new
directory under user-owned content storage. Keep the complete original ZIP in
an external user cache and preserve extracted notices. Remove only the known
`Game/` prefix. Never execute setup components or downloaded DLLs. Keep demo and
retail installations separate, and do not overwrite an existing installation.
Include the external music tree, not only the REZ.

Bound compressed/expanded bytes, entry counts and paths. Reject traversal,
absolute archive paths, links, duplicate/case-colliding destinations and writes
outside staging. Cancellation, disk exhaustion and interrupted publication must
not expose a partial installation. Enforce the qualified redirect policy, TLS
verification and hash validation before extraction; reject HTTP error pages.
Start with cancellable streaming to a temporary file. Resuming is optional and
must revalidate the final archive.

### Optional original artwork

The examined original `BLOOD2.EXE` has `GAME1` through `GAME4` and add-on variants
as 320 by 240 indexed DIB resources. Use a narrow bounded PE/DIB reader, reusing
resource-table code where suitable; do not load or execute the binary. The
source tree's `ClientRes/bitmap1.bmp` and `AddOn.bmp` are vertical multiplayer
wizard logos, not these illustrations.

Missing or unrecognized optional art uses a plain launcher-owned title panel
and records a diagnostic. This must not conceal a missing required game resource.
Keep commercial images, previews and binaries out of Git and packages. Artwork
extraction is optional for acceptance.

## Implementation steps and exit criteria

L0 through L7 are local launcher steps, unrelated to global M0 through M8.
Each needs its own changes, relevant verification and commit.

### L0 Freeze interfaces and regression baseline

Record checkout/dependency revisions, shared-module boundaries, profile grammar,
launcher storage schema and preference ownership. Add the future launcher
contract and source ledger before importing code. List V1 graphics controls,
defer later display startup controls and record provider qualification as open.

Exit criteria: every action maps to an existing interface or a named proposed
change; Nightmares is absent from executable profiles. Unchanged config,
menu-preference and demo tests establish the baseline. No dependency update
or renderer refactor is included.

### L1 Build the SDL3 and ImGui shell

Add the target and dedicated presets. Implement pages, native-dialog adapter,
manual paths, keyboard navigation, HiDPI layout and asynchronous job ownership.
Add optional artwork independently. Use a low-work idle loop and suspend hidden
rendering.

Exit criteria: Debug and Release open without game data; keyboard navigation,
resize/focus/close and 100/150/200-percent scaling work. Handle dialog cancel,
error and completion after page closure. Repeated open/close releases resources
normally. Game SDL options and pins stay unchanged. Dialog failure leaves
manual entry usable.

### L2 Select content and launch the game

Factor shared readers without behavior changes. Add installation records,
explicit edition selection, metadata and REZ-index validation, bootstrap INIs,
process tracking and access to game logs. Revalidate before spawn and reject
a missing or incompatible packaged executable.

Exit criteria: empty, unreadable, ambiguous-case and corrupt installations cannot
launch. Stock retail and the tested demo open their existing main menus. Spaces
and Unicode work from unrelated working directories. Missing optional retail
patches retain existing behavior. Stale local INIs, spawn failure, early game
failure and normal exit produce distinct outcomes. Repeated Play creates one
tracked child; closing the launcher leaves it alive. Return after child exit
works without blocked output pipes.
The capability query works without data or a display; unsupported protocols,
malformed/oversized output and a query timeout prevent an incompatible launch.

### L3 Share current graphics preferences

Extract the preference data/codec with compatibility adapters, add ownership
guards to both applications, and connect detail/model lighting to Apply/Cancel.
Do not change rendering algorithms.

Exit criteria: launcher to game to in-game menu to launcher round trips preserve
both graphics values and audio/mouse values. Version 1 migrates as before;
malformed, incomplete and future-version files fail without partial writes.
Cancel preserves file bytes. Edits are refused while another game owns
preferences; crash recovery releases ownership. Private frontend checks confirm
effective lightmap/shadow/lighting state and the shadow override. Release works
without the inspector.

### L4 Add compatible data profiles

Implement `--content-profile`, ordered overlays, import/selection and an optional
direct custom-world route. Thread the profile through menu, retry and destination
preparation. Show conflicts without promising arbitrary mod compatibility.

Exit criteria: synthetic base/patch/overlay resources resolve in order; reversing
overlays changes the expected winner. Edition mixing, unknown requirements and
code/expansion requests fail before mutation. Overlays survive fresh game,
demo death retry and a level transition. Failed destination preparation retains
the current profile/session. Removing a profile leaves user data intact; stock
startup stays equivalent to L2. Record one named real data-mod/custom-world
check if suitable content is available, or keep that claim unverified.

Acceptance of L4 requires automated propagation evidence, even when no real
third-party mod is available: use synthetic resource overrides with real private
session fixtures for the menu, retry and transition checks. Absence of a named
third-party scenario limits the compatibility claim, not this required gate.

### L5 Import and install the demo locally

Implement package qualification records, the ZIP reader and staging installer.
Support a local ZIP and destination selection first. Register the installation
only after extraction and content validation succeed.

Exit criteria: synthetic archives cover wrong hashes, truncation, forbidden
paths, collisions, excessive expansion, cancellation, write failures and existing
destinations without changing prior installations. A fully qualified original
v1.0 ZIP installs REZ, music and notices, then opens the demo menu. Verify a new
game, Enter death retry, Demo_02 transfer and EndGame return. This named smoke
is not a full playthrough. Interrupted work can be cleaned up/retried without
presenting a partial install as valid.

### L6 Download from a qualified provider

Resolve provider terms/permission and full ZIP/hash qualification. Add HTTP,
progress/cancellation and bounded failure handling, using L5's installer. Opening
the launcher makes no network request; Download is an explicit user action.

Exit criteria: a local HTTP server covers redirects, transfer failures, incorrect
lengths/hashes, cancellation and non-ZIP responses deterministically. One opt-in
real-provider check reproduces installed hashes. TLS failures and new-host
redirects follow the declared policy. Until qualification passes, local import
may ship, but automatic download stays explicitly incomplete.

### L7 Package and qualify each desktop platform

Package both programs and dependency notices without commercial assets. Verify
executable discovery in Linux/Windows distributions and macOS bundles, data
paths, dialogs, process lifetime and cleanup. Update setup instructions and
record the delivered subset.

Exit criteria: a clean user environment can choose data or install the qualified
demo, change supported settings and start the matching game. Report Linux
X11 and Wayland/portal results separately; Windows and macOS need native runs.
Launcher GPU/backend results and game shader support remain separate claims:
a Windows launcher does not establish D3D12 game support. Missing native
coverage stays unqualified.

### Later display startup extension

After the graphics push is integrated and this scope is separately selected,
add persistent window size, windowed/borderless fullscreen and VSync to the
shared configuration. Apply at game startup first. Validate display choices,
dimensions and presentation modes while preserving existing defaults.
GPU/backend selection, exclusive fullscreen, HDR and historical renderer
switches are outside this extension.

Exit criteria: settings change observed game window/presentation state; invalid
values leave stored settings intact. Menu saves preserve new fields. Startup
failure offers a deterministic default-window retry without silently overwriting
the choice. Check HiDPI, monitor changes and native Metal/Vulkan behavior on
the platforms being qualified.

## Validation and evidence

| Layer | Required checks | Evidence |
| --- | --- | --- |
| Headless synthetic | Profile/config compatibility, mounting, process argument recorder, preference ownership/atomicity, ZIP failures, local HTTP server | CTest results and build revision; no commercial fixtures in Git |
| Private content | Retail/demo separation, original ZIP extraction, profile resources, retry and transition retention | External data version/hash and named scenario results |
| UI and GPU | Navigation/scaling, dialogs, asynchronous close/cancel, launch/return and teardown in Debug and Release | Captures and native logs with actual GPU/backend |
| Sanitizers | Readers, extraction/network jobs, callbacks, guards and child cleanup with ASan/UBSan/LSan | Raw results with leak detection enabled; failures remain failures |
| Packaging | Relocation, no-content launch, Unicode paths, offline import and independent desktop qualification | Platform matrix and package manifest |

Proposed tests: `launcher.config`, `launcher.content`, `launcher.process`,
`launcher.preferences`, `launcher.profiles`, `launcher.install`,
`launcher.download` and `gpu.launcher`. Test observable behavior, retention on
failure and cleanup, not widget implementation. Existing `config.*`,
`menus.preferences`, `menus.*`, `demo.*` and `demo_menu.*` remain relevant when
their shared code changes. Profile propagation also needs destination/level-change
regressions. Live internet checks stay opt-in.

At implementation, build the new launcher presets and existing game presets.
Run new headless tests and affected existing families, then required private/GPU
cases in development and inspector-free Release. Use Clang sanitizers for
lifetime work. Select NVIDIA for GPU tests on this developer machine and record
the actual device; never embed that choice in production.

The following launcher-only presets were introduced by L1. Run from `Blood2/`;
see [launcher setup](../../launcher.md) for the separately built game
path and private/GPU test configuration.

```sh
cmake --preset launcher-debug
cmake --build --preset launcher-debug --parallel
ctest --preset launcher-debug -L headless
cmake --preset launcher-release
cmake --build --preset launcher-release --parallel
ctest --preset launcher-release -L headless
```

Register private and GPU cases with the existing opt-in data/GPU CMake options.
List their selected CTest cases before running them; an empty selection is not
a successful validation. Configure existing game Debug/Release builds separately
and run the affected families there. No full-suite rerun is required for an
unchanged subsystem, but a failed relevant check cannot be replaced by a passing
unrelated one.

Put subsystem rules in `Blood2/docs/contracts/launcher_contracts.md`, ledgers in
`Blood2/docs/implementation/`, and results/raw logs in
`Documentation/Implementation/` and `evidence/`. These are future artifacts.
Each result records command, platform, build, content/profile, observed outcome
and unverified scope. Preserve M2-GPU-001: do not suppress it, relabel a failed
GPU test or start another platform-leak investigation as launcher work.

## Delivery order and completion

L0, L1 and L2 produce a reviewable launcher for existing installations. L3 adds
shared graphics settings. L4 and L5 build on L2's content policy and can be
delivered independently. L6 depends on L5 and provider qualification. L7
qualifies the package per platform. Later display startup work and Nightmares
are not prerequisites for V1.

V1 is complete for a declared platform only when applicable L0 through L7
criteria pass, stock menu/startup behavior is preserved, compatible profiles
retain content across transitions, graphics preferences take effect, and the
qualified demo can be downloaded, installed and started. Import without an
approved download provider is an interim release with automatic download
pending. No step implies M3 acceptance, full campaign support or arbitrary
mod compatibility.
