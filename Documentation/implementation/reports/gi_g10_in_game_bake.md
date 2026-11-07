# GI lot G10: in-game bake

2026-11-07, lot G10 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#in-game-bake-lot-g10). Host: Apple M5 (4
performance and 6 efficiency cores), macOS, Metal.

## What landed

- **`app/gi_bake_process.{h,cpp}`**: `GiBakeProcess` (start, non-blocking poll, cancel, bounded
  stop, destructor), the executable path per platform, the child's arguments from the game's
  content, and the GI_PROGRESS/GI_RESULT parsers with the overall fraction and ETA.
- **F8 Bake panel** (`GiTools::start_bake`, `update_bake`, `bake_panel`): Final/Preview, the
  current world's excluded models (editing the placement's `excluded_models`), "Suspend scene
  rendering during bake", Bake/Cancel, progress with ETA, the outcome with frame times, the file,
  Copy to project data/gi and the last output lines; the Pinned strip shows the bake.
- **Hot reload**: a finished bake increments the presenter's reload counter when the session's
  world and DAT digest still match the ones it started for; the presenter's next update (the
  start of the next frame) looks it up.
- **Frontend**: the world record is skipped while a suspended bake runs; `stop_bake` after the main
  loop (the destructor covers every other exit).
- **`gi_project_directory`** (engine): the checkout's `data/gi` for GI tool builds, shared by the
  placement directory and the Copy button.
- **Docs**: the GI contracts (G10 section, Panels and placement versions updated), AGENTS.md, the
  plan, the roadmap and the provenance notes.

## Results

| Check | Result |
| --- | --- |
| Headless and private suite, RelWithDebInfo (`dev`, retail data), `gi_bake_process.*` and `gi_overlay.retail` included | 1,644 / 1,644 |
| GPU suite (`dev`, real display, Apple M5 Metal) | 149 / 150: `gpu.display_options` failed, the window system not applying the requested fullscreen state (as during G9; that test links neither GI nor F8 code) |
| Inspector-free Release, headless suite | 1,190 / 1,190 |
| `gi_bake_process.process` | the stub child: exit 0/4, merged stderr, 64 lines of at most 4,096 bytes, graceful cancel (exit 12), a SIGTERM-ignoring child killed after 300 ms, the destructor reaps the child |
| `gi_overlay.retail`, fake baker | the child's arguments (content, placement, `user_directory()/gi`, 8 jobs), reload at the next draw, suspension, cancel, quit and NEW_LEVEL during a bake |
| `gi_overlay.retail`, this build's `blood2` | F8 Preview bake of 01_SubwayA: baked (exit 0) in 0.5 s, 23 draws at 60 Hz (mean 21 ms, the test's 16 ms sleep plus a headless ImGui frame); the lookup chooses its file; bytes equal an in-process bake with 3 jobs; a Final cancelled at its first progress line exits 12 in 0.1 s and leaves the file unchanged |

## Frame cost during a bake

The game polls once per frame (a non-blocking read and a non-blocking wait), so the cost is CPU and
power contention from the child. `02_CityHub`, `--bench --bench-sweep --gi`, 1920×1080, 1,200
measured frames, `dev`, alone and while an 8-job bake of the same world runs (`--gi-jobs 8`, what
F8 passes on this 10-thread host); two runs each:

| | Frame mean (ms) | Frame p95 (ms) | GPU wait mean (ms) | Simulation mean (ms) |
| --- | --- | --- | --- | --- |
| Alone | 2.348 / 2.386 | 2.465 / 2.475 | 0.769 / 0.779 | 1.200 / 1.222 |
| During the bake | 3.676 / 3.712 | 4.139 / 4.071 | 1.881 / 1.878 | 1.365 / 1.400 |

The frame stays far below 16.7 ms; the GPU wait grows most, the CPU and GPU sharing the SoC's power
budget. "Suspend scene rendering during bake" removes the world draws for users who want the bake
to finish sooner. A SIGTERM after each measured run stopped the CLI bake with exit 12.

## Decisions within the lot

- **Jobs:** the child gets hardware threads − 2 (at least 1) so the game keeps its frame and audio
  threads; the bytes do not depend on the job count.
- **Windows cancel:** SDL's graceful kill posts WM_CLOSE, which a console child never reads, so
  cancel forces the kill there; the file is written atomically at the end, so no partial file can
  remain. POSIX keeps the graceful SIGTERM (exit 12).
- **Result strings:** the child's `error=` is a JSON string; the parser decodes it for the notice.

## Open

- **Gate C**: the user plays the A/B and decides whether GI becomes a player option.
- G11 (reflection bake) is next.
