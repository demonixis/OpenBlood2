# GI lot G2: file format and keys

2026-11-06, lot G2 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#baked-file-and-keys-lot-g2). Retail data:
`/Users/yann/Games/Blood2` (58 world DATs). Host: Apple M5, macOS.

## What landed

- **`engine/gi_file.{h,cpp}`:**
  - the `.ob2gi` container: `write_gi_file`, `read_gi_file` and atomic `save_gi_file`;
  - the content keys: `gi_albedo_key`, `gi_expectation`, `gi_edition`;
  - freshness: `gi_status` with Missing, Ready, Stale(reasons), Foreign and Failed;
  - names and lookup: `gi_file_name`, `gi_search_roots`, `find_gi_file`, `read_gi_bytes`.
- **Configuration:** `gi_dir =` in the configuration file and `--gi-dir DIR`.
- **`app/gi_tool.{h,cpp}`:** `--list-worlds [--json]` and `--gi-verify FILE`.
- **Tests:** `tests/gi_file_tests.cpp` (eight headless cases and the private `retail` case) and
  `config.gi`.

## Format checks

- Every refusal in the contract has a test that reaches it past the earlier checks (size and
  trailer resealed) and checks that the output is untouched.
- 4,000 deterministic mutations are each refused or round-trip byte for byte.
- An independent Python writer of the layout (scratch only) produced files that `--gi-verify`
  read correctly: a stale file (zero albedo key), a foreign file (zero world digest) and a file
  with a flipped trailer byte.
- The first test run found a real defect: a section count one too high made the reader parse
  the trailer as a section header. Sections are now parsed from the body only, and the case
  reports "truncated section header".

## Retail results

| Check | Result |
| --- | --- |
| `--list-worlds --json` | 58 worlds (36 single-player, 22 `worlds/multi/`), edition retail, 0.33 s |
| `gi_file.retail` | 58 deterministic expectations and names, 1.0 s |
| Example name | `worlds/01_subwaya.91bc95a10ed8.ob2gi` |
| Saved file for 01_SubwayA | Ready, then Stale (albedo) after its key moved |

`--gi-verify` exit codes on the hand-made files: stale 4, foreign 4, refused 4, missing file 2.

## Test results

- Headless suite (`ctest -L headless -LE private`, dev build): 1,148 of 1,148 pass.
- `gi_file.*` and `config.*` under ASan/UBSan (`BLOOD2_SANITIZERS=ON`, debug build): 17 of 17 pass.
- No frame, save or simulation change: nothing reads a GI file at runtime yet.

## Not done in G2

- Payload decoders and their limits (texels, probes, reflection resolution): G7, G9, G11.
- The runtime and F8 states Dirty, Baking and Disabled: G3, G10.
- Writing real bakes (`--bake-gi`, `--gi-if-stale`): G5. Packaging `data/gi/baked`: G6.
