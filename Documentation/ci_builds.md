# Continuous integration and downloadable builds

[CI](https://github.com/demonixis/OpenBlood2/actions/workflows/ci.yml)
runs on GitHub-hosted runners for pushes, pull requests and manual dispatches.
GitLab remains the `origin` remote; GitHub is the `github` remote. No GitLab
runner, personal access token or repository secret is needed by this workflow.

| Artifact | Runner | Compiler | Architecture |
| --- | --- | --- | --- |
| `OpenBlood2-windows-x64` | Windows Server 2025 | MSVC | x86-64 |
| `OpenBlood2-linux-x64` | Ubuntu 24.04 | GCC | x86-64 |
| `OpenBlood2-macos-arm64` | macOS 26 | Apple Clang (Xcode 26.6) | Apple Silicon |
| `OpenBlood2-android-arm64` | Ubuntu 24.04 | NDK 29.0.14206865 Clang | arm64-v8a (phone/tablet APK) |
| `OpenBlood2-quest-arm64` | Ubuntu 24.04 | NDK 29.0.14206865 Clang | arm64-v8a (Meta Quest OpenXR APK) |

Every job builds the inspector-free Release game, then runs its synthetic headless
CTest suite, including the setup screen's persistence and demo-installer checks.
Missing tests are an error. GPU and private-data tests are
excluded explicitly. The Android and Quest jobs build the `android` and `quest`
presets only (no tests, as everywhere for these platforms) and upload the
debug-signed RelWithDebInfo APK as `OpenBlood2-<version>-<preset>-arm64.apk`,
with `libmain.so` stripped of debug info (`BLOOD2_ANDROID_STRIP`). The unstripped
library is a separate `symbols-<preset>-arm64` artifact kept 90 days, for
symbolicating crashes with `ndk-stack`; it is not attached to releases. These jobs do not certify graphical execution, campaign
traversal or the outstanding M2 GPU reservation.

Failures remain failures, and the other platforms continue independently. Test
reports (JUnit), CTest logs and CMake diagnostics are uploaded even after failure.
Build archives are uploaded only after successful builds and tests. Artifacts
expire after 14 days. Dependency archive downloads are cached; CMake still checks
the repository's pinned hashes. Compiler outputs are not shared between runners.

## Get and use a build

Open a successful Actions run and download the artifact for your platform.
Extract the contained `OpenBlood2-<version>-<platform>` ZIP (Windows) or tar.gz
(Linux/macOS; see [packaging](packaging.md) for versions and release tags), then run
`blood2` (`blood2.exe` on Windows): its setup screen finds or installs the game data. Or
start it on a folder directly:

```sh
./blood2 --data-dir "/path/to/Blood II"
```

The archive includes dependency notices (`licenses/`), an example configuration and
the exact source commit in `BUILD.txt`. It contains no commercial game data or demo;
supply your own installation or use the setup screen's demo installer. Linux links
OpenSSL (for libcurl) from the system; Windows uses Schannel and macOS NSURLSession.
Windows archives also carry the MSVC runtime DLLs. Linux needs an Ubuntu 24.04-compatible glibc and
system graphics/audio drivers. Windows needs a Vulkan-capable graphics driver.
The macOS build requires macOS 26 or newer, targets Apple Silicon and uses Metal; it is not an Intel or
universal binary. These are unsigned development builds, without macOS notarization.
The current floating-point `std::from_chars` calls require macOS 26's libc++;
selecting a newer Xcode on a macOS 15 runner is not sufficient. CI explicitly
selects Xcode 26.6 and a macOS 26.0 deployment target.
The pinned libcurl disables optional libidn2 discovery to avoid linking a developer
machine's shared library. Packaging rejects non-system macOS dylibs and smoke-tests
the staged Windows executables without the developer tool directories on `PATH`.

## Releases

Pushing a tag `vx.y.z` that matches `project(VERSION)` (see [packaging](packaging.md))
runs every job, then the `release` job creates the GitHub release `vx.y.z` with
generated notes and attaches the three desktop archives and the two APKs. A
mismatched tag fails packaging, and any failed job prevents the release. `0.x`
versions are published as pre-releases. If a release for the tag already exists
(created by hand on GitHub), the job only uploads the builds to it. Only this job
has write permission. The tag must reach the `github` remote: a tag pushed only to
GitLab starts nothing, and a release created on GitHub for a tag whose commit CI
has not built gets no files until that tag's run passes.

The APKs are signed with the runner's throwaway debug key, which changes on every
run: installing a newer release over an older one fails with a signature mismatch,
so uninstall the previous build first (game data in `/sdcard/openblood2` is kept).

```sh
git tag v0.3.1
git push github v0.3.1
```

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
```

On Windows, use an x64 Visual Studio developer shell. The CI presets use
`build/ci/` and leave ordinary development presets and private data paths alone.
