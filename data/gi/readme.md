# Project GI folder

Baked GI inputs for the M10 [GI plan](../../Documentation/implementation/gi_plan.md) (decision 8).
The formats are in the [GI contracts](../../Documentation/contracts/gi_contracts.md).

- `placement/<world>.json`: probe placement, one file per world (lot G4). Committed. Written by
  `--gi-place` and the F8 overlay's Save, which use this folder by default in GI tool builds.
  The lattice is derived from the file and the world, so a file holds only the settings, the
  include/exclude volumes and the reflection probes.
- `baked/`: `.ob2gi` files written by `python3 -I tools/gi/bake_all.py` (lot G6; `--bake-gi WORLD`
  alone writes to the user directory's `gi/` unless `--gi-out` says otherwise), with its
  `bake_report.json` beside this file. Never committed (decision D3): git ignores both and the
  `gi_policy` test fails if one is tracked. Builds copy `baked/` into `gi/` beside the binary,
  the Apple bundles' `Resources/gi` and the APK's `assets/gi` (`BLOOD2_PACKAGE_GI`,
  `BLOOD2_GI_DATA_DIR`); public CI turns this off. A later lot adds `settings/`.
