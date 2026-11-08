# OpenBlood2

Rebuild **Blood II: The Chosen** using its original game code and a portable
C++20 engine with SDL3/SDL_GPU.

[Roadmap](Documentation/roadmap.md) · [Documentation](Documentation/readme.md) ·
[Tests](Documentation/testing.md)

## Build and run

Install CMake 3.28+, Ninja, Git, a C++20 compiler and the SDL platform
development packages (on Linux also OpenSSL development files). On Windows, use
an x64 Visual Studio developer shell. The first configure downloads pinned
dependencies.

```sh
cmake --preset dev
cmake --build --preset dev --parallel
./build/dev/blood2
```

The first launch opens the setup screen: choose your Blood II folder, or download
and install the free 1998 demo. Options > SETUP returns to it (edition, folders,
data mods). To skip it:

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II"
./build/dev/blood2 --demo --data-dir "/path/to/Blood2 Demo"
```

Normal launch shows the original title screen and menu; Esc pauses. Add `--view-world` for direct
entry. Single Player saves and loads the original slots; F5 quicksaves, F6
quickloads. Use `release` for an optimized build without the
inspector, `debug` for debugging. See [setup and controls](Documentation/getting_started.md).

## Contributing and license

Read [AGENTS.md](AGENTS.md) and [contributing](Documentation/contributing.md).
Reuse original rules, record adaptations, test behavior and keep commercial
assets external. AI-assisted changes require the same review and verification.
Original contributions use the [MIT License](LICENSE); imported material keeps
its own notices. See [provenance](Documentation/implementation/provenance.md).
