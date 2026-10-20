# Documentation/CI merge resolution — 2026-10-20

The merge of incoming `b7256aea` into documentation-reorganization commit
`fb5c902` preserves the unified `Documentation/` tree and the incoming
Crypt/Bridge/Entrance/Lobby implementation and desktop CI.

- Keep the incoming level contract, source ledger and CI guide byte for byte at
  their new paths.
- Transfer the two old index additions into the maintained documentation and
  implementation indexes; retain deletion of `docs/readme.md` and
  `docs/implementation/readme.md`.
- Combine the new level instructions and CI README section with the concise
  local guides, using current links.
- Update CI packaging source paths and the packaged provenance directory to
  `Documentation/`; fix the source-index evidence link.

No gameplay behavior, reference source, commercial data, dependency revision or
historical evidence is changed by conflict resolution. M2 closure and reservation
M2-GPU-001 remain unchanged.

## Validation

- `cmake --build --preset dev --parallel 8`: passed on Linux, including the
  game, launcher and test executables.
- `ctest --preset dev -L headless --parallel 4`: 881/881 passed (4.03 seconds).
  This preset has no private-data path and GPU tests are disabled.
- Checked 1,011 local Markdown paths in the affected documents, the package's
  repository file inputs and JSON parsing of the new source ledger.
- Compared the three relocated incoming files byte for byte with `MERGE_HEAD`.
- `cmake --list-presets`, `git diff --check`, staged whitespace checks and
  unmerged-index/marker checks passed.

Local build/test logs are `build/merge-resolution-build.log` and
`build/merge-resolution-tests.log`. No private-data, GPU, sanitizer, Windows,
macOS or full CI archive-packaging run was performed for this merge resolution.
