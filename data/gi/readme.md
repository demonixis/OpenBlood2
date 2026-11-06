# Project GI folder

Baked GI inputs for the M10 [GI plan](../../Documentation/implementation/gi_plan.md) (decision 8).
The formats are in the [GI contracts](../../Documentation/contracts/gi_contracts.md).

- `placement/<world>.json`: probe placement, one file per world (lot G4). Committed. Written by
  `--gi-place` and the F8 overlay's Save, which use this folder by default in GI tool builds.
  The lattice is derived from the file and the world, so a file holds only the settings, the
  include/exclude volumes and the reflection probes.
- Later lots add `settings/`, `baked/` (never committed, decision D3) and `bake_report.json`.
