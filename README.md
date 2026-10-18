# OpenBlood2

A preservation project rebuilding **Blood II: The Chosen** with its original game
code and a new, small, portable C++20 engine. The stack uses CMake, pinned SDL3
`main`, SDL_GPU, spdlog and Dear ImGui for in-game inspection.

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

Install CMake 3.28+, Ninja, a C++20 compiler and SDL's platform development dependencies.
The first configure downloads pinned dependencies. Supply your own Blood II installation.
Run from this repository:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
./build/debug/blood2 --data-dir "/path/to/Blood II" --view-world
```

Use the `release` presets for an optimized build without the inspector.
[Setup, controls and tests](docs/getting_started.md) cover configuration,
NVIDIA selection, Metal and validation commands.

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
