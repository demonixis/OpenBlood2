# OpenBlood2

Rebuild **Blood II: The Chosen** using its original game code and a portable
C++20 engine with SDL3/SDL_GPU. F10 toggles the development inspector.

[Roadmap](Documentation/roadmap.md) · [Documentation](Documentation/readme.md) ·
[Tests](Documentation/testing.md)

M0/M1 are complete on Linux. [M2 is closed with reservation M2-GPU-001](Documentation/implementation/reports/m2_acceptance.md).
M3 is next; full campaign support remains unaccepted. Linux/NVIDIA is tested,
macOS/Metal is user-confirmed; Windows graphical qualification, mobile and VR are pending.

## Build and run

Install CMake 3.28+, Ninja, Git, a C++20 compiler, SDL platform development
packages and OpenSSL development files. On Windows, use an x64 Visual Studio
developer shell. The first configure downloads pinned dependencies.

```sh
cmake --preset dev
cmake --build --preset dev --parallel
./build/dev/blood2-launcher
```

The launcher selects your installation or installs the demo. To launch directly:

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II"
./build/dev/blood2 --demo --data-dir "/path/to/Blood2 Demo"
```

Normal launch opens the original menu; Esc pauses. Add `--view-world` for direct
entry. Save/load is unavailable. Use `release` for an optimized build without the
inspector, `debug` for debugging. See [setup and controls](Documentation/getting_started.md).

## Contributing and license

Read [AGENTS.md](AGENTS.md) and [contributing](Documentation/contributing.md).
Reuse original rules, record adaptations, test behavior and keep commercial
assets external. AI-assisted changes require the same review and verification.
Original contributions use the [MIT License](LICENSE); imported material keeps
its own notices. See [provenance](Documentation/implementation/provenance.md).
