# Contributing

Read [AGENTS.md](../AGENTS.md), the [architecture](architecture.md) and the relevant
[service contracts](contracts/readme.md) before editing a subsystem. Follow the
workspace instructions when working in the full reference workspace.

## Implementation workflow

1. Check the original Blood II caller and its engine interface before inventing
   behavior. Preserve the reference repositories and commercial files.
2. Keep the change small. Separate compiler/platform adaptations, replacement
   engine behavior and deliberate gameplay differences in the explanation.
3. Use C++ or Rust, the existing pinned stack, explicit return values and simple
   ownership. Follow [.clang-format](../.clang-format) and
   [.editorconfig](../.editorconfig). Project-owned C++ namespaces use `OpenBlood2`.
4. Expose implemented state and failures in the in-game inspector, including
   hidden/inactive and unresolved elements. A counted request is not proof that
   its service succeeded.
5. Run checks appropriate to the change. Behavioral changes need observable
   assertions; lifetime changes need sanitizer coverage. Application/rendering
   changes also need the inspector-free Release build and relevant GPU checks.
6. Record results and limitations in English, then commit completed work in the
   affected repositories. Do not mark a milestone complete from compilation or
   a partial scenario alone.

See [getting started](getting_started.md#tests) for commands and external-content
configuration. Private tests require your own installation. Select NVIDIA for
GPU tests on the configured development machine and verify the device in the log;
do not hardcode that device choice in production.

## Where documentation belongs

- General developer guides: this directory.
- Subsystem behavior and ownership: `contracts/`.
- Import/adaptation history: [implementation/provenance.md](implementation/provenance.md).
- Source-selection and hash ledgers: [implementation/sources/](implementation/sources/readme.md).
- Progress reports and measured results: the dedicated locations in the
  [implementation index](implementation/readme.md), not the README or guide root.

Keep filenames lowercase except the conventional repository entrypoints. Update
maintained links after a move. Preserve historical logs, patches and source-hash
snapshots; they describe the files and paths at their recorded commits.

Documentation-only changes need link/path checks, not a game rebuild. Imported
code retains its original notices and rights; see [LICENSE](../LICENSE) and
[provenance](implementation/provenance.md). AI-assisted contributions require the
same source review and verification as other contributions.
