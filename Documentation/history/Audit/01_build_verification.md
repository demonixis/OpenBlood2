# Builds and Executed Checks

Audit date: **15 October 2026**. These results apply to the exact repository commits recorded in [snapshot.json](evidence/snapshot.json), which match the [inventory baseline](../Inventory/workspace.json). They do not establish support on another platform or with future dependency revisions.

## Environment and Isolation

The host was Linux x86_64, Fedora 44, with CMake 4.3.0, Ninja 1.13.2, GCC 16.2.1, Clang 22.1.8, .NET SDK 10.0.112, and .NET runtime 10.0.12. No .NET 8 runtime or Godot executable was available. Exact tool output is preserved in the snapshot.

Builds used `/tmp/lithtech-audit-2bVxTF`. Dependencies were fetched there through each project's CMake declarations. No system packages were installed. Source files and build declarations were not patched. The successful C++ builds used Debug configuration and four parallel jobs.

The Godot project build accepted `--artifacts-path` for intermediate artifacts, but its SDK still wrote the final assembly to `nomad/.godot/mono/temp/bin/Debug/NoMAD.dll`. These are ignored build outputs; the repository remained clean. C++ post-build commands created asset/shader symlinks and OpenBlood2 generated its string table inside the temporary build tree. Those outputs are not part of the documentation evidence.

## OpenBlood2

The initial configuration used:

```sh
cmake -S OpenBlood2 -B /tmp/lithtech-audit-2bVxTF/openblood2 -G Ninja \
  -DENABLE_VR=OFF -DBUILD_TOOLS=OFF -DCMAKE_BUILD_TYPE=Debug
```

It failed during generation because the build expects `${imguizmo_SOURCE_DIR}/ImGuizmo.cpp`, whereas the fetched `master` checkout contains `src/ImGuizmo.cpp`. This is a dependency-layout drift in the build inputs, not a failure to compile original Blood 2 code. See the [initial configure log](evidence/openblood2_configure.log) and [root CMake file](../../../../OpenBlood2/CMakeLists.txt), lines 162–173.

To examine the code beyond that blocker, configuration was repeated with this **temporary diagnostic override**:

```sh
-DFETCHCONTENT_SOURCE_DIR_IMGUIZMO=/tmp/lithtech-audit-2bVxTF/openblood2/_deps/imguizmo-src/src
```

Generation then succeeded. `cmake --build /tmp/lithtech-audit-2bVxTF/openblood2 --parallel 4` built the engine, adapted original game static library, and executable. The resulting `game/OpenBlood2 --help` exited successfully. [Configure](evidence/openblood2_configure_override.log), [build](evidence/openblood2_build_override.log), [CLI smoke check](evidence/openblood2_help.log).

This is meaningful evidence that the existing adaptation can be compiled with a current Linux compiler. It is not a clean baseline build: the dependency override is required, dependencies were fetched at the recorded revisions, and the game is compiled with compatibility options including `-fpermissive`. Warnings include legacy string-literal conversions, hidden overloads, format mismatches, and a conflicting `MID_DAMAGE` macro (`8` in the replacement types header versus `1001` in the game's message header). Their consequences need individual investigation; a warning count would not measure fidelity.

The build excludes original networking, save/load UI, and other files, and supplies replacements in [Blood2Stubs.cpp](../../../../OpenBlood2/blood2/Misc/Blood2Stubs.cpp). Therefore successful linking does not mean every original subsystem was preserved. See [blood2/CMakeLists.txt](../../../../OpenBlood2/blood2/CMakeLists.txt).

## NoMadEngine

The initial configuration also failed on the ImGuizmo path. With the corresponding `FETCHCONTENT_SOURCE_DIR_IMGUIZMO` override, generation succeeded. The tested minimal configuration set `BUILD_TOOLS=OFF`, `BUILD_EDITOR=OFF`, and `ENABLE_VR=OFF`.

Compilation then failed in [GameBase.cpp](../../../../NoMadEngine/nomadengine/src/game/GameBase.cpp), including lines 1337, 1371, 1384, 1646, 1660, and 1689: `m_vrEnabled` was referenced although its declaration in [GameBase.h](../../../../NoMadEngine/nomadengine/src/game/GameBase.h), line 457, is inside `#ifdef OPENXR_ENABLED`. This is a source-confirmed and reproduced defect in the advertised non-VR configuration. [Failure log](evidence/nomadengine_build_override.log).

Reconfiguring the same temporary tree with `ENABLE_VR=ON`, retaining the dependency override and editor/tools exclusions, allowed the engine and both game executables to build and link. [VR-enabled configuration](evidence/nomadengine_configure_vr.log), [successful build](evidence/nomadengine_build_vr.log). No OpenXR session, graphics output, audio playback, or gameplay was tested. This success does not negate the non-VR defect.

## Godot / C# Prototype

Both commands succeeded, each reporting zero warnings and zero errors:

```sh
dotnet build nomad/NoMAD.csproj \
  --artifacts-path /tmp/lithtech-audit-2bVxTF/dotnet-main --nologo
dotnet build nomad/tests/Nomad.Tests/Nomad.Tests.csproj \
  --artifacts-path /tmp/lithtech-audit-2bVxTF/dotnet-tests --nologo
```

See the [main-project build](evidence/nomad_main_build_corrected.log) and [test-project build](evidence/nomad_tests_build.log). Godot.NET.Sdk restored successfully. Compiling its C# scripts does not validate scene loading, native Godot methods, shader output, physics interactions, or export packaging.

The test executable targets .NET 8. A direct `dotnet <test.dll>` launch failed because that runtime is absent. An explicit `dotnet --roll-forward Major <test.dll>` ran on .NET 10. This is useful local execution evidence, but not validation under the requested .NET 8 runtime. [Default runtime failure](evidence/nomad_tests_default_runtime.log).

The first roll-forward run from the workspace root reported 63 `PASS` lines and one failure in `Game config locator resolves local extracted configs`. The test discovers the project by walking upward from its executable, then falls back to the working directory. Since the executable was outside the repository, it used the workspace root. The test unconditionally expects an `assets/blood2/config` path even when that directory does not exist and [GameConfigLocator](../../../../nomad/src/Nomad.Core/GameConfigLocator.cs) correctly returns its fallback. This is a harness/environment assumption, not evidence that all game configuration loading is broken. [First run](evidence/nomad_tests_rollforward.log).

A second run used an isolated temporary root containing empty synthetic `materials.json`, `strings.txt`, and `musics.json` files at `assets/blood2/config/`, and no game archives. All 64 entries printed `PASS`. Inspection of [Program.cs](../../../../nomad/tests/Nomad.Tests/Program.cs), lines 1213–1390, shows that the five local-data entries skip their bodies when archives are absent. The defensible result is **59 completed non-asset checks and five skipped asset-dependent checks**, with synthetic configuration-path fixtures and .NET 10 roll-forward. The console harness does not report skips distinctly. [Controlled run](evidence/nomad_tests_synthetic_root.log).

## Reference LithTech Tree

`cmake -S sources/lithtech ...` failed immediately because its CMake 2.8-era compatibility declaration is rejected by CMake 4.3. A diagnostic retry with `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` proceeded farther, then failed generation on references such as `StdAfx.cpp`, `S_Intersect.cpp`, `BaseFX.cpp`, and `main.cpp` that do not resolve in this checkout. Case sensitivity and stale file lists both need consideration; this audit did not repair them. [Baseline](evidence/lithtech_configure.log), [policy override](evidence/lithtech_policy_configure.log).

No compiler result is claimed for this engine, the untouched Blood 2 reference game, or Shogo Aegis. The latter two are source references, not configured modern standalone targets in this audit.

## Test Discovery and Probe Qualification

`ctest -N` reported **zero tests** in both configured C++ build trees. Existing OpenLithTech test source files do not become an automated regression suite merely by being present. [OpenBlood2 discovery](evidence/openblood2_ctest_inventory.log), [NoMadEngine discovery](evidence/nomadengine_ctest_inventory.log).

The audit's isolated engine probe links the unchanged engine, object, class, string, and base-class implementations. It substitutes only the unused server pointer and frame-time setter. Its scheduling callback writes the same field as `GameInterface::SetNextUpdate`; it does not exercise the complete compatibility interface. The compiled engine core is actual production code, not a copied implementation.

GCC's sanitizer link failed because the installed libasan linker reference pointed to a missing file. A Clang C++20 attempt failed in spdlog's bundled fmt constant-expression code. A Clang C++17 build of this subset succeeded with AddressSanitizer and UndefinedBehaviorSanitizer. Thus the sanitizer results apply to that isolated C++17 probe, while the complete application build was GCC with the projects' declared C++ standards. These setup failures remain recorded in the [evidence index](evidence/readme.md).

The aggregate probe links the actual adapted `cpp_engineobjects_de.cpp`, with synthetic globals for registration/server state. It stops after eight recursive entries to avoid an unbounded stack overflow. The ABC probe compiles the actual parser and binary reader against a generated 12-byte cyclic file. Neither probe loads commercial assets.

An initial main-project command used the incorrect spelling `Nomad.csproj`; its recorded missing-file failure is an audit invocation error, superseded by the successful `NoMAD.csproj` command. The first aggregate-probe link similarly lacked a synthetic registration global; the corrected probe supplies it. These are not attributed to the projects.
