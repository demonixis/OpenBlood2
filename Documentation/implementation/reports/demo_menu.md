# Demo menu implementation and validation

Date: 2026-10-19. Separately requested implementation following the
[menu audit](demo_menu_audit.md), at the requested X-high scope. Normal `--demo`
now opens the limited original-source menu with an explicit edition resource
provider. The old fixed-offset reader remains removed. M2 is still closed with
M2-GPU-001; this does not accept M3 or change the graphics stack.

Blood2 implementation commit: `39c3af6`, on the existing `47d4f87` project-name
update. Test logs were captured before committing that worktree; the final CLI
help wording was then rebuilt in Debug/Release and checked with `--help`.

## Delivered behavior

The main menu has Single Player, Bloodbath, Options, Prime Gibs and Quit. Caleb
selects difficulty and starts Demo_01. Chosen/Bloodbath/Prime Gibs show their
actual supplied promotional PCXs. Source `_DEMO` omits Load/Save/Custom rather
than presenting fake disabled slots. The existing Display, Audio and Mouse
settings work; unsupported options identify an OpenBlood2 limitation.

Esc uses the existing pause menu and independent menu mixer. EndGame unloads
outside callbacks, shows Info with the source enter cue and returns to Main;
starting another game after completion is exercised. Quit shows Info with the
source exit intent. Promotion dismissal returns to Main even when opened through
Chosen, as established by the additional `GameStates.cpp` review.

The provider owns 27 reviewed English labels, edition title/font/layout choices,
demo ambience mapping and three promotion paths. Demo menu text does not require
either DLL. Retail still reads external cres.dll. Selected demo images/fonts/WAVs
are validated before menu exposure, without global text fallback or fake assets.
Gameplay localization is separate. See the detailed
[contracts](../../contracts/demo_menu_contracts.md) and
[source/hash ledger](../sources/demo_menu_sources.json).

Enter after death still feeds the shared owned restart request; the application
transactionally starts the current world with initial Caleb equipment and retained
difficulty. The [future checkpoint/save hook](../../contracts/demo_contracts.md#temporary-death-retry-and-future-saves)
remains documented. This is not a save system. `--demo --view-world` preserves the
phase-1 menu-free frontend and terminal end-image-to-exit behavior.

```sh
# Normal menu; select Caleb and difficulty.
./build/release/blood2 --demo --data-dir /path/to/B2Demo
# Preserved direct diagnostic/play route.
./build/release/blood2 --demo --data-dir /path/to/B2Demo --view-world --world Demo_02
```

## Fidelity and remaining differences

| Area | Evidence and implementation | Remaining boundary |
| --- | --- | --- |
| Main/solo/difficulty/promotions | Original `_DEMO` branches, source semantic IDs and reviewed Build 108 labels | No claim of original-client pixel/timing parity |
| Title images | Explicit Build 108 `*_title.pcx` names | Existing source layout and aspect-preserving canvas retained |
| Mouse | Additional Build 108 constructor review confirms exactly Sensitivity/Input Rate/Invert Y | Shared later-source sensitivity range 100..1500/step 20 retained; binary has 15..400/step 15 |
| Controls/Joystick/Keyboard | All six source options destinations remain visible; these three are grey | Rebinding and device pages unavailable in the reconstruction, not historically disabled |
| Audio/Input Rate | Working sound/music and volumes; historical 16-bit and input-rate controls grey | No new output-depth or smoothing mode |
| Display | Existing detail/lighting controls reused | Renderer/resolution pages absent; lighting row is a reconstruction option |
| Promotion state | Fresh Esc/Enter/Space returns to root; quit flag exits; ambience stops during splash | Gameplay/audio stay paused under the opaque image, unlike source global ResumeSounds on menu exit |
| Loading/title/movies | Existing startup behavior retained | Loading1/2 overlays and other original presentations remain separate work |
| Language | Finite English manifest; retail PE strings remain external | Translated demo editions need explicit review/provider |

Static mouse evidence for the supplied 719,872-byte cshell.dll: the constructor
at 0x1006cf07..0x1006cfe6 adds the three rows and sets slider minimum 0x0f,
maximum 0x190 and step 0x0f. Its data references resolve to the previously
identified Sensitivity/Input Rate/Invert Y literals. These addresses are analysis
evidence only; none is used by the runtime. Source files and archive hashes are
recorded in the new ledger; previous ledgers/audit/evidence are unchanged.

The original client was attempted in a full private directory copy and a private
Wine prefix. It displayed **“Unable to set the video mode. (6929314)”**. A virtual
desktop retry also did not produce a usable original menu. This failed observation
does not verify any original visual/device behavior. No graphics-stack changes
were made to pursue it. The local error capture is
`/tmp/blood2-menu-reference-_lnqs5aa/original-start.png`; original commercial data
was not changed. Our Release captures were inspected for text, pages, promotions
and framing, with local paths/digests in
[capture hashes](evidence/demo-menu-capture-hashes.json). Commercial captures are
kept outside tracked artifacts.

## Validation and evidence

Host: Linux, NVIDIA GeForce RTX 3060 Laptop GPU. Debug/Release use the existing
GCC build; headless sanitizer checks use Clang, `BLOOD2_SANITIZERS=ON`,
`BLOOD2_GPU_TESTS=OFF`. Release has `BLOOD2_INSPECTOR=FALSE`.

| Run | Result | Evidence |
| --- | --- | --- |
| Debug configuration, retail menu state/art/session/profiles/preferences, demo flow/rules, new manifest/flow/assets, both demo SDL traversals | 13/13 pass | [Debug log](evidence/demo-menu-debug.log) |
| Release demo manifest/flow/assets, retail artwork and traversal, menu-free/demo-menu traversals | 7/7 pass | [Release log](evidence/demo-menu-release.log) |
| ASan/UBSan, leak detection enabled, selected headless menu/config/demo tests | 10/10 pass | [Sanitizer log](evidence/demo-menu-sanitizers.log) |
| Normal unscripted-clock demo menu startup, 20 frames | Exit 0, no world loaded, one menu ambience, no failures | [Startup log](evidence/demo-menu-normal.log) |
| Direct Demo_02, 120 real-clock frames | Exit 0, expected world, zero menu audio starts | [Direct log](evidence/demo-menu-direct.log) |
| Menu startup with both DLL names removed from a private REZ index | Exit 0, captured menu, no world loaded | [Procedure](evidence/demo-menu-independence.txt), [log](evidence/demo-menu-without-dlls.log) |

`demo_menu.manifest` removes each required text in turn and rejects unknown,
duplicate, empty, oversized, newline and embedded-NUL entries. `demo_menu.assets`
composes all seven supported pages, checks edition mismatch and bounds/cache,
compares all three promotions to the decoded PCX pixels, decodes selected ambience
and cues, and rejects synthetic corrupt title/font/promotion/audio overrides and
missing resources. Fixtures contain no commercial payloads.

The real SDL menu traversal checks more than three seconds of frozen simulation,
Focus and game audio while the menu mixer advances. It visits all three promotion
actions, Display/Audio/Mouse, focus loss/gain, available inspection toggles,
resume without catch-up, source death, Enter retry, authored Demo_01→Demo_02
transition at `05_egress`, EndGame→Info→Main, a new game at a different difficulty
and the quit promotion. [Debug frontend](evidence/demo-menu-debug-frontend.log)
and [Release frontend](evidence/demo-menu-release-frontend.log) record completion.
Injected input is tagged and preferences are isolated; normal input is unchanged.
Exit callbacks are deliberately triggered by the diagnostic, not reached by a
human traversal. This turn validates menu/session integration, not a new complete
combat walkthrough of both worlds; phase-1 gameplay evidence retains its scope.

Reproduction from Blood2, using the existing configured private-data build trees:

```sh
cmake --build --preset debug --target blood2 demo_menu_tests menu_tests demo_tests config_tests --parallel 4
ctest --test-dir build/debug -R '^(demo_menu\.|menus\.|config.demo$|demo\.(flow|rules)$|gpu.demo$|gpu.demo_menu$)' --output-on-failure
cmake --build --preset release --target blood2 menu_tests demo_menu_tests --parallel 4
ctest --test-dir build/release -R '^(demo_menu\.|menus.artwork$|gpu.menu_traversal$|gpu.demo_menu$|gpu.demo$)' --output-on-failure
cmake --build build/sanitize --target demo_menu_tests menu_tests config_tests demo_tests --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize -R '^(demo_menu\.|menus\.(state|artwork|session|preferences)$|config.demo$|demo\.(flow|rules)$)' --output-on-failure
```

GPU sanitizers were not repeated; the independent M2-GPU-001 platform failure
remains open. Other operating systems, translated editions and original-client
visual parity are not validated by these results. Root README user edits were
preserved; source trees, historical evidence and commercial archives stay intact.
