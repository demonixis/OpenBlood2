# GI lot G6: bake-all script and build packaging

2026-11-06, lot G6 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#bake-all-script-and-packaging-lot-g6). Host:
Apple M5 (4 performance and 6 efficiency cores), macOS.

## What landed

- **`tools/gi/bake_all.py`** (standard library, `python3 -I`): world selection from
  `--list-worlds --json` (`--set single|multi|all`, `--worlds`), one `--bake-gi` per world with
  `--gi-if-stale` unless `--force`, `--edition`, `--quality`/`--bounces`/`--rays`,
  `--jobs`/`--threads`, `--prune`, `--list`, `--dry-run`, `GI_BAKE`/`GI_BAKE_TOTAL` lines,
  `data/gi/bake_report.json`, SIGINT/SIGTERM forwarded to the running bakes (exit 12).
- **`cmake/GiPackaging.cmake`**: `BLOOD2_GI_DATA_DIR` (default `data/gi/baked`) and
  `BLOOD2_PACKAGE_GI` (default ON). One `copy_if_different` command per globbed file into
  `gi/` beside the binary, `Resources/gi` in Apple bundles, and `android/assets/gi`, which
  `blood2_apk` passes to Gradle as `-Pblood2.assets` (an optional `assets` source folder).
- **Decision D3 guard**: `.gitignore` rules for `data/gi/baked/` and the report, the
  `gi_policy` test (`cmake/CheckGiPolicy.cmake`), `BLOOD2_PACKAGE_GI` OFF in `ci-release` and
  the CI APK jobs, and `PackageCi.cmake` refusing a `gi` folder beside the CI binary.
- **Tests**: `gi_policy` and `gi_bake_all` (the script against a stub binary, Unix only).

No engine or game source changed in this lot.

## Deviation from the plan

The plan names a POST_BUILD `copy_directory_if_different`. A POST_BUILD step runs on every
link and adds a command even when the folder is empty, so it cannot meet "an empty folder
leaves build time unchanged". Per-file custom commands found by a `CONFIGURE_DEPENDS` glob add
nothing when there is no file, copy only changed files, and never relink the game. The
destinations are the ones the plan lists.

## Results

| Check | Result |
| --- | --- |
| Headless suite, RelWithDebInfo (`dev`) | 1,165 / 1,165 |
| Headless suite, inspector-free Release | 1,165 / 1,165 |
| `gi_bake_all` (stub binary) | pass |
| `gi_policy` | pass; fails on a tracked `.ob2gi` and on a missing ignore rule (scratch repository) |
| First `bake_all.py` run, 36 single-player worlds, Final, 10 threads | 36 PASS in 12.4 s, 3.5 MB |
| Second run | 36 SKIP in 1.2 s |
| `--list` after the runs | 36 READY |
| `--prune --dry-run` | nothing to prune |
| `git status` after baking | no `data/gi/baked` entry (ignored) |
| `dev` build after configure | 36 `Packaging GI file` steps; `gi/worlds` identical to the source |
| No-op `dev` build with 36 files | `ninja: no work to do`, 0.05 s |
| One rebaked file | one copy step, no relink |
| Empty folder (`BLOOD2_GI_DATA_DIR` pointing nowhere) | `Packaged GI files: 0`, no GI target in `build.ninja` |
| `xcode-macos` bundle | builds; 36 files in `OpenBlood2.app/Contents/Resources/gi/worlds` |
| `xcode-ios` device build (`CODE_SIGNING_ALLOWED=NO`) | builds; 36 files in `OpenBlood2.app/gi/worlds` |
| `android` APK (phone, debug) | builds; 36 `assets/gi/worlds/*.ob2gi` entries (`unzip -l`) |

The ASan and GPU checks were not rerun: this lot changes no C++ source, shader or GPU path.
No simulator, emulator or device was started; the mobile results come from building only. The
Quest APK shares the phone's Gradle and CMake path and was not built separately.

## Notes

- The F8 overlay is still the only runtime caller of `find_gi_file`, so the packaged root is
  checked by file location (the four roots in the contract), not by a rendered frame. G8
  connects rendering.
- Copies deleted from `data/gi/baked` remain in an existing build tree until it is cleaned;
  names carry the DAT digest, so a leftover is never used for another DAT.
- G7 is next: the GI lightmap core, which adds the LMAP and `VTX ` sections to these files.
