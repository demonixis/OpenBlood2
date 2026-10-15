# Source and Dependency Provenance

## Project Code

M0 started a new repository and build. No source file, project skeleton, build flags, compatibility table or gameplay implementation has been copied/cherry-picked from OpenBlood2, NoMadEngine or nomad. M1 imports the selected original Blood 2 code below directly from the preserved reference tree. The prior audit supplies behavioral requirements for the contract tests.

The SDL_GPU/ImGui integration follows the API sequencing documented by the [upstream SDL3/SDL_GPU example](https://github.com/ocornut/imgui/blob/c1aaac65e896a45f81f47a4d68f9959e0f86c859/examples/example_sdl3_sdlgpu3/main.cpp). The backends themselves are compiled directly from the pinned dependency. Project configuration, ownership, inspector panels, smoke checks and optional capture are new code.

Future imports should add one concise entry with donor repository/commit/path, purpose, local changes and validation. There are no prototype imports to record for M0.

## M1 Original Imports

Donor: `sources/blood2`, commit **`006b4aabd3fb348f6846cad03799cf8df685ea95`**. All reference files remain unchanged. Imported files retain their original comments; line endings are normalized to LF. [Original/source hashes](ORIGINAL_SOURCES.json) and the [adaptation patch](../../Documentation/Implementation/evidence/m1-original-adaptations.patch) record the selected inputs and edits. No original binary library or commercial resource is copied.

| Original path | New use and adaptations | Validation |
| --- | --- | --- |
| `ObjectDLL/B2BaseClass.{h,cpp}` | Compile original difficulty/game-type filters and parent forwarding. Remove unused shell include; use selected shared enums. Replace old registration macros with explicit typed registration/defaults in `game/registration.cpp`. Function bodies are unchanged. | Original world difficulty rejection and flag initialization |
| `ObjectDLL/CameraSpot.{h,cpp}` | Compile original precreate/initial-update behavior. Remove unused `PathListData.h`; replace registration; omit an unused parameter name. | Original 0.001 precreate delay, zero initial schedule, inherited dispatch |
| `ObjectDLL/Key.{h,cpp}` | Compile original property reads/string ownership. Correct `key.h` include casing to `Key.h`; replace registration; omit an unused parameter name. | Actual fields and strings, destruction, world-versus-string creation |
| `AppHeaders/cpp_engineobjects_de.h` | Single authoritative C++ `BaseClass`. Initialize aggregate/head/handle/internal pointers in the constructor; mark the retained internal field `maybe_unused`. Original dispatch methods retained. | Nested creation, actual virtual calls, aggregate forwarding, sanitizer tests |
| `AppHeaders/cpp_aggregate_de.h` | Retain original aggregate virtual methods and typed thunks. Initialize next pointer; omit names on unused default-handler parameters. | Synthetic aggregate through original callbacks; sanitizer tests |
| `AppHeaders/engineobjects.c` | Copy only `bc_AddAggregate`, `bc_EngineMessageFn`, `bc_ObjectMessageFn` as C++, with the explicit C++ base include. Exclude legacy registrations and unrelated built-in classes; forwarding/property logic is unchanged. | Precreate property transfer; no recursive object-message redispatch |
| `AppHeaders/engineobjects_de.h`, `cpp_server_de.h` | Retained declarations/backward-compatibility include, unchanged except line endings. Only the C++ branch is used. | GCC/Clang builds |
| `basedefs_de.h`, `basetypes_de.h`, `de_codes.h`, `serverobj_de.h`, `server_de.h`, `CSBase.h` | Curated declarations/constants for the supported source interface. Fixed-width scalar types, initialized structs, const input strings, opaque owned handles; array-bounded `SAFE_STRCPY`; real concrete server object. Original DLL/C layout is not supported. APIs outside the subset are absent, with selected deferred services explicitly returning error. | All object contracts; see [service matrix](OBJECT_CONTRACTS.md) |
| `Shared/SharedDefs.h`, `Shared/NetDefs.h`; two globals from `ObjectDLL/BloodServerShell.cpp` | Preserve difficulty/game-type numeric values and console variable names without pulling in the unimplemented shell. | Original filtering tests |

The old macro registration defaults are transcribed in one explicit table. `DifficultyInsane` is accepted as an optional property because original `ReadProp` requests it, although the original class declaration omits it. Property lookup is case-insensitive, covering the original `TypeBloodBath`/`TypeBloodbath` spelling difference. Retail lookup behavior remains to be compared in M2.

No gameplay fix was applied to `B2BaseClass`'s cooperative-mode `OPT_FLAG_CTF` check. `Key` still reads its custom properties only for world creation. The M1 layer does not replace either behavior with a guessed correction. Original `Rotating`, AI, weapons, player and session shells are not imported yet.

## Dependencies

Revisions were resolved on **15 October 2026**. [Dependencies.cmake](../cmake/Dependencies.cmake) is the authoritative manifest and includes SHA-256 checksums of the downloaded source archives.

| Dependency | Revision/source | Use | Upstream notice |
| --- | --- | --- | --- |
| SDL3 | `main` at `6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1` (reports 3.5.0) | Platform, GPU, PNG capture; includes upstream OpenXR support | [zlib notice](https://github.com/libsdl-org/SDL/blob/6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1/LICENSE.txt) |
| Dear ImGui | `master` at `c1aaac65e896a45f81f47a4d68f9959e0f86c859` (reports 1.93.0) | Optional development inspector and SDL3/SDL_GPU backends | [MIT notice](https://github.com/ocornut/imgui/blob/c1aaac65e896a45f81f47a4d68f9959e0f86c859/LICENSE.txt) |
| spdlog | v1.15.3 at `6fa36017cfd5731d617e1a934f0e5ea9c4445b13` | Logging with its bundled fmt configuration | [License and bundled-component notices](https://github.com/gabime/spdlog/blob/6fa36017cfd5731d617e1a934f0e5ea9c4445b13/LICENSE) |

Dependencies are fetched into ignored build/cache directories with their upstream notices. Retain those notices when distributing binaries. EnTT, Jolt, ImGuizmo, SDL_mixer and SDL_shadercross are not M0 dependencies. The ImGui backend carries its own shader assets; custom game shaders belong to later rendering work.
