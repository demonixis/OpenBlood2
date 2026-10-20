# Reproducing the Audit Probes

These programs test actual project implementations with synthetic inputs. They do not modify project sources or use commercial assets. The recorded source commits, environment and dependency revisions are in [the snapshot](../evidence/snapshot.json). Exact successful and failed command arguments are in [the evidence index](../evidence/readme.md).

Run from the workspace root unless a command's metadata specifies another working directory. Recorded `/tmp/lithtech-audit-2bVxTF` paths identify this audit run, not permanent project configuration; replace them with a new scratch directory when repeating the audit.

## Command Recorder

[run_command.py](run_command.py) executes a command with GNU `timeout`, saves complete output and JSON metadata under `Documentation/Audit/evidence/`, and returns the command's status. Put options before the run name:

```sh
python3 Documentation/Audit/probes/run_command.py \
  --seconds 30 new_run_name -- command argument
```

A repeated run name overwrites its previous log/metadata. Use a new name to preserve the original evidence. Exit 124 denotes timeout. Nonzero probe exits can be expected reproductions of defects; consult each probe's description.

## Engine Contracts

[engine_contract_probe.cpp](engine_contract_probe.cpp) links these unchanged OpenLithTech sources: `engine.cpp`, `object.cpp`, `class_manager.cpp`, `string_manager.cpp`, and `baseclass.cpp`. It defines an unused null `g_pServerDE` and a no-op `LT_SetFrameTime` solely to satisfy the isolated link. Class callbacks and all tested object-management code execute through the real engine and class manager.

The successful sanitizer compilation command is preserved in [engine_probe_build_clang17.json](../evidence/engine_probe_build_clang17.json). It uses Clang, C++17, AddressSanitizer, UndefinedBehaviorSanitizer, debug information, and the spdlog headers fetched by OpenBlood2 configuration. Earlier GCC sanitizer and Clang C++20 attempts failed for the reasons documented in [build verification](../01_build_verification.md).

Run the resulting binary without arguments to observe creation and scheduling. At this snapshot it prints:

```text
precreate_struct_x=42.00 stored_object_x=0.00 initial_reason=0
callback_requested_delay=0.01 stored_delay=1.00
updates_after_two_20ms_frames=1
```

The callback's scheduling assignment matches `GameInterface::SetNextUpdate`, but the facade itself is not linked into this probe. Its return of `1` follows the original base engine-message callback convention. This run prints observations and exits zero; zero does not mean the engine contract passed.

Run the same binary with `remove` to remove the synthetic object from its update callback. The audit run exits nonzero with an ASan heap-use-after-free. If the host's symbolizer is unavailable, use `addr2line -Cfipe <binary> <address>` on the addresses recorded in that run; addresses change after rebuilding.

## Aggregate Dispatch

[aggregate_dispatch_probe.cpp](aggregate_dispatch_probe.cpp) links the actual adapted `cpp_engineobjects_de.cpp`, which includes `engineobjects.cpp`. Synthetic registration/server globals satisfy its link requirements. Compilation uses the same compatibility header needed by the adapted game, plus section garbage collection to exclude unrelated engine-object implementations.

The synthetic object delegates to its base handler as original game objects do. It stops recursion at eight callback entries. At this snapshot, one message produces `one_object_message_callback_entries=8 (bounded_at_8)` and exit 1. The correct nonrecursive expectation in this probe is one entry. See [build command](../evidence/aggregate_probe_build_corrected.json) and [execution](../evidence/aggregate_dispatch.json).

## ABC Cycle

[abc_cycle_probe.cpp](abc_cycle_probe.cpp) writes a 12-byte synthetic section chain to its supplied scratch path, then invokes the actual C++ ABC parser. It links `abc_parser.cpp` and `BinaryReader.cpp` with the fetched GLM headers. [Build command](../evidence/abc_cycle_build.json).

Always run it with a short timeout because it deliberately exercises a nonterminating path:

```sh
python3 Documentation/Audit/probes/run_command.py --seconds 3 abc_cycle_repeat -- \
  /path/to/abc_cycle_probe /path/to/scratch/synthetic-cycle.abc
```

The audit recorded exit 124. No source data from a commercial ABC file is used.

## C# Synthetic Test Environment

The existing test executable finds its project root by walking upward from the executable and then using its working directory. To reproduce the controlled run without inspecting local game data, use a separate scratch working directory containing only these empty synthetic files:

```text
assets/blood2/config/materials.json
assets/blood2/config/strings.txt
assets/blood2/config/musics.json
```

Build the test project outside the repository as shown in [build verification](../01_build_verification.md). Execute its DLL from this scratch root. The audit used explicit .NET 10 major roll-forward; repeat under .NET 8 when that runtime is available. The fixtures exercise path/existence assertions only, not valid configuration-file parsing. Five local-asset tests skip their work because no archives exist, although the harness prints `PASS` for them.

## Lexical Analysis and Integrity Snapshot

```sh
python3 Documentation/Audit/probes/analyze_original_api.py
python3 Documentation/Audit/probes/capture_snapshot.py /path/to/audit-build-root
```

[analyze_original_api.py](analyze_original_api.py) masks comments and literals, extracts function-pointer/virtual method declarations from five original API headers, and collects matching arrow-call names in original C/C++ files outside `AppHeaders`. It writes declarations and candidate call locations. It is not a compiler frontend or a reachability analysis.

[capture_snapshot.py](capture_snapshot.py) compares all phase-1 indexed text hashes, records repository status/commits, local tool versions and fetched Git dependency commits, then rebuilds the evidence index. It reads existing inventory records without regenerating or changing the phase-1 baseline. Both scripts overwrite their generated reports; archive earlier evidence before using them for another audit date.
