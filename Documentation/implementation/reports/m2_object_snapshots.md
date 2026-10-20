# M2 — Occupied-object snapshot index

The session now obtains owned object snapshots from a compact, slot-ordered index
of checked identities. It no longer scans and dereferences every registry slot for
each snapshot. Original game code, collision arithmetic, AI cadence, animation
clocks, callback order and dependency pins are unchanged.

The index includes construction, inactive/hidden and pending/destroying records,
matching the former scan. It publishes before constructors and erases after
original destructors return, so nested creation/removal remains visible in fresh
snapshots while already-returned copies stay unchanged. Low-slot reuse preserves
ordering and changes generations. The Objects inspector now labels its count
accurately as indexed records, including pending removal. See
[contracts](../../contracts/object_snapshot_contracts.md).

## Why this change

The prior [door-audio audit](m2_door_audio.md) recorded the unchanged
`ai_runtime.clearance_private` test exceeding its 90-second sanitizer deadline.
A fresh baseline here completed in 88.90 seconds, leaving little headroom; that
pass does not erase the previous timeout. The Debug baseline took 13.76 seconds.
The fixture runs the real intro, two explicitly relocated/invulnerable encounters
and ordinary death/corpse cleanup, twice. It is not a retail playthrough.

A GDB sampling diagnostic collected 500 interrupted stacks: 465 included session
step and 80 included Objects::snapshot. This is a local indicative profile, not
an exact CPU attribution or a release-performance benchmark. The profile led to
this narrow bookkeeping change; no physics or model-pose approximation was added.
The first updated Debug run took 12.57 seconds, with the same eight blocked resizes
and 93 original AI shots in each lifetime, and successful corpse/removal checks.
The updated Clang ASan/UBSan run passes in 88.16 seconds with the same observations.
That 0.74-second difference is too small to claim dependable sanitizer headroom:
the 90-second timeout remains unchanged, and future timing variation is still a risk.
The roughly 9% Debug reduction is one local before/after measurement, not an FPS
claim or a statistically established speedup across machines.

## Verification

[The evidence ledger](evidence/m2-object-snapshots.json) records final build/test
commands, timings, profile data, reference integrity and limitations.
Debug and inspector-free Release each pass all 91 selected headless/private tests.
Clang ASan/UBSan with leak detection checks the same 91 cases: the clearance test
runs separately without competing builds/tests, followed by the other 90 cases.
Five NVIDIA scenarios pass in each Debug/Release configuration. The captured
post-transition scene was inspected; it renders CityHub geometry and characters.
These are selected regressions, not a rerun of the entire test suite.

New tests exercise construction/precreate/destructor reentrancy, null factories,
rejected precreate, sparse slots, old snapshots, pending removal, hidden/inactive
records, generation reuse and unload/reload. Existing tests cover the original
service and gameplay paths; no assertion or timeout was weakened.

NVIDIA checks exercise inspector lifetime, player movement, containers and
continuous train combat/traversal/NEW_LEVEL as selected in the ledger. Captures
remain untracked with external commercial imagery. Inspector-free Release and
Clang ASan/UBSan are verified separately; native Metal is not run for this change.

M2 remains incomplete. This optimization does not resolve remaining unsupported
services or certify the whole campaign. The exploratory NVIDIA/libdbus
LeakSanitizer baseline in the earlier audit is still unresolved and is not
silenced by this increment's headless checks. No source reference or commercial
asset was modified.
