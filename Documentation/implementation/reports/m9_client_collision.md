# M9 — independent local client collision replica

2026-10-30, following `00690d3c`. This continuation replaces live-server prediction
queries with a client-owned scene. It preserves solo `CMoveMgr`, immediate server
validation and one time integration. M9 network gameplay, M2-GPU-001 and M3 remain
separate. See the [contract](../../contracts/multiplayer_contracts.md#client-collision-replica)
and [source ledger](../sources/m9_client_collision_sources.json).

## Implemented boundary

`ClientSession::collision` receives complete collider frames with world, sequence
and object generations. The publisher deep-copies primary collision geometry and
posed world-model/container shapes; object motion copies only values. The client
holds no original object, server shape, registry or service callback. Shapes are
cached by identity and copied again on replacement/rotation; primary geometry has
a revision that invalidates its copy even when rebuilt at the same address.
Teardown clears current membership and caches. A detached snapshot remains usable
after the original server is destroyed.

The independent client kernel follows the existing replacement slide, head skin,
stair, overlap and push/rider planning rules. Common transformed-shape query
arithmetic is extracted unchanged. There is no speculative touch collection,
damage, push commit or random draw. The old `ServerAdapter::PredictPlayerMovement`
entry point is removed. `CMoveMgr` itself is unchanged: it integrates once, predicts
against the delivered scene, submits the original integrated command, and accepts
the server's immediate correction. The authoritative collision and callback path
is retained. Inspection exposes scene membership, sequence, copy counts and
publication failures; a refused publication never falls back to querying a stale
scene as current data.

This completes collision-query independence for local movement prediction. It
does not extract environment/stance decisions or camera/weapon rays, and it adds
no wire codec or delayed multiplayer timing. Validated engine geometry is copied
by the local publisher; future network content admission must supply equivalent
client resources. Copying geometry costs memory and rotated shapes cost additional
CPU work. No multiplayer performance or hardware capacity is qualified here.

## Verification

[Raw evidence](evidence/m9_client_collision_2026-10-30.txt) records commands,
results, the initial fixture assertion failure and an incremental-build artifact repair.

| Check | Result and scope |
| --- | --- |
| Linux dev / Release without inspector | Build; Release movement smoke passes 240 rendered frames, resize, stance changes and unload/re-entry |
| Headless regression | **169/169**: 139 synthetic and 30 external-retail-data cases |
| Clang ASan/UBSan | **30/30**, plus a successful instrumented run of the two-world private case |
| Private replica exercise | SubwayA and CityHub, **900 ticks each**; no publication/prediction failure, one primary geometry copy per world |
| Independent receiver | Links without game/server/registry methods; detached scene still collides after server destruction |
| Motion/query parity | **400** seeded movements agree with authoritative position/velocity/support/counters; posed-shape rays agree at four rotations |
| Real GPU | **3/3**: world, movement and player restoration, NVIDIA RTX 3060 Laptop / Vulkan |
| Solo trajectory | **360 frames byte-identical** to the preceding committed trace; CMoveMgr source unchanged |
| Preservation | **6,184** pre-existing file hashes unchanged; the same 39 PBR files remain outside the commit |

The receiver tests cover bounded atomic admission, stale worlds/generations,
malformed frames, stairs, support, the Eye owner exclusion and removal. Local tests
cover independent query counters, geometry replacement at the same address,
translation/rotation, cache reuse and lifetime. The existing movement, contacts,
platform/push, container, fall/impulse, camera, imprisonment and save/restore suites
remain in the selection. CityHub's extended roam passes in 61.17 s within its
unchanged 120 s timeout. No new GPU reference is accepted.

The initial lifetime test failed because its wall-contact assertion assumed the
wrong synthetic player extent; correcting that assertion required no production
collision change. A later dev incremental link encountered an empty archive member;
replaying its generated archive/link commands after compilation repaired the
artifact. Both events and the final successful checks remain in the evidence.

These are Linux reconstruction checks with synthetic and external retail data,
not retail-engine trajectory equivalence, whole-campaign acceptance, multiplayer
qualification or cross-platform proof. No save layout, gameplay random draw,
shader, material rule or PBR resource was changed.
