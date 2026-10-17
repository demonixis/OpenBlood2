# Owned object snapshots

`Objects::snapshot()` now copies a compact index of occupied slot identities,
rather than walking every slot and dereferencing each heap record on every call.
This extends [object lifetime contracts](object_contracts.md); the independent
[bound-container index](container_index_contracts.md) is unchanged. See
[measurements and validation](../../../Documentation/Implementation/m2_object_snapshots.md).
This is replacement engine bookkeeping, with no original game-code change.

## Membership and callback safety

Keep `records_` sorted by slot, containing the full generation for every occupied
slot, independently of visibility, activity or `Life`. Publish the identity after
the record occupies its slot and **before** invoking its original constructor.
Constructor/precreate/initial-update callbacks therefore see the same membership
as the previous direct scan, including a record whose game instance is not yet
assigned. A rejected precreate or null factory remains indexed while pending
normal deferred reclamation. Invalid input rejected before slot publication adds
nothing.

Removal marks pending without erasing the identity. Link callbacks and original
destructors can query the registry, create objects or queue further removals.
Erase the exact checked identity only after the game destructor returns and
immediately before slot reclamation/generation increment. Retain no vector iterator
across a callback; nested insertion may reallocate or reorder the vector.
A reused low slot is inserted ahead of older higher slots with its new generation.
Clear/unload uses ordinary destruction; it must not clear this index early or
reset generations. The existing 65536 lifetime handle limit bounds index membership.

Each snapshot still owns a **fresh copy**. Callers may mutate that copy, and
creation/removal cannot extend or overwrite an already running traversal.
Queries still resolve each identity and check current life, transforms, flags and
properties. Old generations cannot select newly reused slots. No collision result,
AI decision, update schedule, animation pose, resource, or original callback result
is cached. Simulation/callback order and all geometry arithmetic are unchanged.

Copying costs O(occupied records) with one vector allocation for nonempty results,
independent of vacant high-water slots. Insert/reclaim cost O(occupied records)
and retained index storage follows the bounded population high-water mark. This
is not an allocation-free traversal or a concurrent registry. The gain trades
infrequent membership maintenance for cheaper repeated owned snapshots.

## Inspection and checks

`record_count()` reads index size without making a snapshot. The Objects inspector
labels this count as indexed records, explicitly including pending removal;
it does not claim every record is alive or successfully initialized. The existing
rows still show checked states, missing resources and individual lifetime.

`object_snapshots.membership` checks exact slot order, pending membership,
hidden/inactive records, independent mutable copies, low-slot reuse and generation
changes, unload and reload. `callbacks` checks visibility during construction and
precreate, nested creation, destructor-time observation/creation/removal, rejected
precreate and null factory reclamation. `churn` retains an old snapshot across a
sparse 1024-slot registry and repeated low-slot reuse without stale aliasing.

Regression checks cover original object messages/lifetime, models/collision,
movement, containers, attachments, NPC initialization/combat/death, service coverage
and continuous 30/60 Hz train traversal. The original AI clearance scenario and its
90-second sanitizer timeout are unchanged. Measurements are local diagnostic runs,
not retail physics parity, FPS guarantees or completion of M2. No rendering,
skinning, dependency revision, original rule or commercial asset was changed.
