# OpenBlood2

A preservation project rebuilding **Blood II: The Chosen** with its original game
code and a new, small, portable C++20 engine.

The project supports Windows, Linux and MacOS with plans to support Android, iOS and VR.

An internal debugger can be toggled by pressing the `F10` key.

## Status

| Milestone | Estimated task completion |
| --- | ---: |
| M0 | 100% |
| M1 | 100% |
| M2 | 100% |
| M3 | ~60% |
| M4 | ~35% |
| M5 | ~10% |
| M6 | ~15% |
| M7 | 0% |
| M8 | 0% |

The percentages above are rough task estimates, not milestone acceptance.
M0/M1 are complete for their documented Linux scope. **M2 is closed with a
documented NVIDIA/D-Bus GPU leak reservation** for the tested SubwayA-to-CityHub
session. See the [acceptance contract](docs/contracts/m2_acceptance_contracts.md).
M3 is the next milestone and has not been accepted.
Linux/NVIDIA is tested; macOS/Metal works according to user testing. The full game,
other platforms and later milestones remain incomplete.

See the [workspace roadmap](../Documentation/Roadmap/readme.md) and
[implementation and validation records](docs/implementation/readme.md).

## Build

Install CMake 3.28+, Ninja, a C++20 compiler, SDL's platform development dependencies
and OpenSSL development headers/libraries for the launcher.
On Windows, install MSVC and the Windows SDK, and run the Ninja presets from an
x64 Visual Studio developer shell; see [Windows setup](docs/getting_started.md#windows-build).
The first configure downloads pinned dependencies. Supply your own Blood II installation.
Run from this repository:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
./build/debug/blood2 --data-dir "/path/to/Blood II"
```

The default build also produces `build/debug/blood2-launcher` beside the game.
Run it to select your game folder or install the demo. Use
`-DBLOOD2_BUILD_LAUNCHER=OFF` at configure time for a game-only build.
The launcher uses an isolated SDL build; see [launcher setup](docs/launcher.md).

Normal retail launch opens the original main menu. F10 toggles the development inspector;
Esc opens the pause menu. Use `--view-world` for direct entry. Four default Chosen
profiles, difficulty, Audio and Mouse options are connected; save/load slots remain
explicitly unavailable. See [menu contracts](docs/contracts/menu_contracts.md).

Use `--demo --data-dir /path/to/B2Demo` for the [demo menu](docs/contracts/demo_menu_contracts.md).
Caleb starts Demo_01 with the chosen difficulty; EndGame returns through the Info
screen to Main. Add `--view-world [--world Demo_02]` for menu-free direct entry.
Esc pauses/resumes; Enter after death restarts the current world with initial
equipment. See [demo contracts](docs/contracts/demo_contracts.md) for the temporary
retry behavior, future save hook and validation limits.

Use the `dev` presets (optimized, with inspector) for playtesting and the `release`
presets for an optimized build without the inspector; Debug is several times slower
in large worlds.
[Setup, controls and tests](docs/getting_started.md) cover configuration,
NVIDIA selection, Metal and validation commands.

[GitHub Actions](https://github.com/demonixis/OpenBlood2/actions/workflows/ci.yml)
builds and tests Release versions for Windows x64, Linux x64 and macOS Apple
Silicon. Each successful job provides a game + launcher archive; see
[CI builds](docs/ci_builds.md) for downloads, test scope and pushing to both remotes.

## Contributing

- Read [AGENTS.md](AGENTS.md) and the [developer documentation](docs/readme.md) before changing code.
- Preserve original behavior and record source provenance; keep portability fixes
  distinct from gameplay changes. Use the supplied formatting rules and small changes.
- Run relevant behavioral/lifetime tests, document results and limitations in English,
  and keep commercial assets outside Git. C++ and Rust are the accepted languages.

## AI assistance

AI tools assist implementation, analysis and documentation. AI-assisted contributions
need the same source review, tests and human scrutiny as any other contribution;
generated code and claims are not evidence of original-game fidelity.

## License

Original work contributed to this reconstruction is licensed under the
[MIT License](LICENSE). Imported game code, reference repositories, dependencies
and commercial assets retain their respective rights and notices; the MIT license
does not relicense them. See [source provenance](docs/implementation/provenance.md).
