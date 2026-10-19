# Continuous integration and downloadable builds

[Desktop CI](https://github.com/demonixis/OpenBlood2/actions/workflows/ci.yml)
runs on GitHub-hosted runners for pushes, pull requests and manual dispatches.
GitLab remains the `origin` remote; GitHub is the `github` remote. No GitLab
runner, personal access token or repository secret is needed by this workflow.

| Artifact | Runner | Compiler | Architecture |
| --- | --- | --- | --- |
| `OpenBlood2-windows-x64` | Windows Server 2025 | MSVC | x86-64 |
| `OpenBlood2-linux-x64` | Ubuntu 24.04 | GCC | x86-64 |
| `OpenBlood2-macos-arm64` | macOS 26 | Apple Clang (Xcode 26.6) | Apple Silicon |

Every job builds the inspector-free Release game and launcher, then runs their
synthetic headless CTest suites, including the launcher/game information protocol.
The separate launcher build enables its tests, which the default combined build's
child tree disables. Missing tests are an error. GPU and private-data tests are
excluded explicitly; these jobs do not certify graphical execution, campaign
traversal or the outstanding M2 GPU reservation.

Failures remain failures, and the other platforms continue independently. Test
reports (JUnit), CTest logs and CMake diagnostics are uploaded even after failure.
Build archives are uploaded only after successful builds and tests. Artifacts
expire after 14 days. Dependency archive downloads are cached; CMake still checks
the repository's pinned hashes. Compiler outputs are not shared between runners.

## Get and use a build

Open a successful Actions run and download the artifact for your platform.
Extract the contained ZIP (Windows) or tar.gz (Linux/macOS), then run
`blood2-launcher` (`blood2-launcher.exe` on Windows), or start the game directly:

```sh
./blood2 --data-dir "/path/to/Blood II"
```

Keep both executables in the same directory. The archive includes dependency
notices, an example configuration and the exact source commit in `BUILD.txt`.
It contains no commercial game data or demo; supply your own installation or
use the launcher's demo installer. OpenSSL is linked statically; Windows archives
also carry the MSVC runtime DLLs. Linux needs an Ubuntu 24.04-compatible glibc and
system graphics/audio drivers. Windows needs a Vulkan-capable graphics driver.
The macOS build requires macOS 26 or newer, targets Apple Silicon and uses Metal; it is not an Intel or
universal binary. These are unsigned development builds, without macOS notarization.
The current floating-point `std::from_chars` calls require macOS 26's libc++;
selecting a newer Xcode on a macOS 15 runner is not sufficient. CI explicitly
selects Xcode 26.6 and a macOS 26.0 deployment target.
The CI launcher disables optional libidn2 discovery to avoid linking a developer
machine's shared library. Packaging rejects non-system macOS dylibs and smoke-tests
the staged Windows executables without the developer tool directories on `PATH`.

## Push and reproduce

A push only to GitLab does not start GitHub Actions. Push the same branch to both
remotes, or configure GitLab push mirroring separately:

```sh
git push origin HEAD
git push github HEAD
```

CI does not copy commits between repositories or modify either remote. The
workflow must be on GitHub's default branch before its manual Run workflow button
is available. Pull requests run with read-only repository permissions.

Reproduce the build and public test selection with CMake 3.28+, Ninja and the
platform dependencies described in [getting started](getting_started.md):

```sh
cmake --preset ci-release
cmake --build --preset ci-release --parallel 3
ctest --preset ci-release
cmake --preset ci-launcher-release
cmake --build --preset ci-launcher-release --parallel 3
ctest --preset ci-launcher-release
```

On Windows, use an x64 Visual Studio developer shell. Supply `OPENSSL_ROOT_DIR`
as a CMake cache variable when necessary; the launcher CI preset requires static
OpenSSL libraries. CI additionally sets `BLOOD2_LAUNCHER_TEST_GAME` to the absolute
path of the built game to enable its integration check. The CI presets use
`build/ci/` and leave ordinary development presets and private data paths alone.
