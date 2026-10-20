# Bound-container enumeration

This extends [container contracts](container_contracts.md) and supersedes the
full object-slot scan described in the scene-explosion hot-query adjustment.
See [evidence](../implementation/reports/m2_container_index.md).
This is replacement engine bookkeeping; original VolumeBrush, MoveMgr and
specialized volume rules are unchanged. M2 remains incomplete.

`Objects` maintains a vector of checked `ObjectId` values for successfully bound
container shapes. Insert in ascending slot order after geometry validation and
before `MID_INITIALUPDATE`. A reused low slot must precede older higher slots.
Retain the full generation; never substitute a raw index or instance pointer.
Rejected precreate and missing/invalid named geometry add no entry.

Shape ownership is private to `Objects`; `ObjectRecord::container()` exposes a
read-only shared pointer to immutable geometry. Binding and index insertion must
stay together. Runtime motion, shape replacement and direct external assignment
remain unsupported. The index holds no shared ownership of shapes or objects.

Pending and destroying records remain indexed until their original destructor
returns. Erase by checked identity immediately before record reclamation. Do not
retain vector iterators across callbacks or destructors, which can create/remove
other objects. Clear/unload follows ordinary destruction; do not clear this index
early, reset generations or allow stale identities into a replacement world.

`container_snapshot()` returns a fresh owned copy on every call. Query loops still
resolve each identity and check current `Life::Alive`, shape, flags and properties.
A callback-created volume appears in subsequent snapshots; it cannot extend an
already running traversal. A callback-removed volume is skipped immediately even
though it remains in that traversal's snapshot until deferred reclamation.
No point contents, body overlap, hidden flag, gravity, current or damage result
is cached. Head/body separation, hidden-first precedence, four body records and
original physics/damage cadence are unchanged.

Enumeration costs O(bound containers), independent of unrelated model/FX slots;
creation and reclamation are O(bound containers). The existing object/handle
limit bounds storage. A snapshot still allocates its owned vector; this change
claims neither allocation-free queries nor parallel simulation support.

The Containers inspector displays the bound-index count (including pending
reclamation) and enumerates current live supported brushes from the same index.
Hidden brushes remain listed; rejected source entries remain in World. Reading
this state performs no geometry query and advances no clock. Count alone does
not establish correct behavior or a completed M2 session.
