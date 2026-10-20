# M2: Original built-in models in the next destination

Project commit: `24ae2d4d627d297ebc84ebd145e8a3d615af3cad`.

CityHub now creates and renders its 47 original built-in `Model` objects using
two real ABC/DTX asset pairs: light bulbs and red lamps. The original engine class
sets OT_MODEL then delegates to BaseClass properties and aggregates. Its typed C++
adapter introduces no new gameplay rule, dependency, global state or custom renderer.
It is distinct from the still-unported destructible model class.

[Contracts](../../contracts/builtin_model_contracts.md),
[source ledger](../sources/m2_builtin_model_sources.json) and
[retained evidence](evidence/m2-builtin-models.json) describe the boundary.
Objects shows the class's actual semantics, dimensions, ABC state and properties;
Skeleton and 3D view provide existing pose, skin and hidden-object inspection.
These models do not emit light. Missing resources retain explicit errors rather
than becoming successful geometry or a ready destination.

CityHub now has **291 created / 541 editor-only / 159 unsupported / 35 failed**
source entries. Its structural scan has 295 schema-checked / 159 unknown / 31
invalid entries. The 47-entry improvement is exercised both by actual world loading
and disposable destination rehearsal; all source-session preservation assertions
remain enabled. SubwayA remains 204 created / 4 transferred sounds / 372 editor-only,
with no unsupported or failed source entries. Those counts alone never close M2.

New checks cover source callback/property semantics, missing and malformed resources,
hidden animation, exact asset sharing, source poses, read-only rendering, stale IDs
and release on removal/unload. Private rendering checks inspect all 47 models and
both skins through two world lifecycles. GPU pixel checks isolate one real instance
of each asset at its authored transform, verify textured non-magenta pixels, omit
and restore its submission, then repeat after unload/reload. Their diagnostic camera
and isolated geometry are explicitly distinct from whole-level visual acceptance.

Debug and inspector-free Release each pass **707/707 checks**: 649 headless and
58 serial GPU, including 146 private-data cases (104 headless). Debug takes
90.11s headless / 187.67s GPU;
Release takes 12.93s / 120.4s.
Clang ASan/UBSan with leak detection passes **48/48** selected checks in
37.55s. Final retained build logs contain no compiler warnings/errors;
final runtime logs contain no GPU validation error/VUID or sanitizer diagnostic.

Commands from Blood2: `cmake --build build/debug -j4`,
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer build uses `-j2 --target object_contract_tests model_view_tests world_tests services_tests`.
Sanitizer CTest uses `-j2 -R '^(objects|services|world|model_view)\.'`, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

All nine changed C++ files pass clang-format 23.1.3; all 2809 local Markdown
links resolve. All 10923 reference-file hashes and six reference repositories remain
unchanged. Only Linux/Vulkan was verified. Debug/Release private asset captures
were visually checked; they remain ignored. The preparation diagnostics and
synthetic/GPU evidence remain separate from retail gameplay comparison.

The next destination still needs its 122 CDestructableModel objects, other missing
classes and failed services before a safe transition can be claimed. Temporary
rehearsal still never promotes or ticks a destination. Automatic NEW_LEVEL, required
remaining services/FX, complete encounter parity and the M2 gate remain open.
