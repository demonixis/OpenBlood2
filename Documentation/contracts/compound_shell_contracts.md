# Compound DAT shell validation

The [CityHubC/SteamTunnels extension](cityhubc_steamtunnels_contracts.md) documents the later, narrowly scoped dynamic polygon boundary and authored music filename policies. Historical limits below retain their original scope.

This extends [container geometry](container_contracts.md) and
[world-model binding](door_contracts.md). Evidence is retained in
[m2_compound_shells.md](../implementation/reports/m2_compound_shells.md).
The [source ledger](../implementation/sources/m2_compound_shell_sources.json) records the unchanged original rules.
It changes replacement engine validation, not original gameplay code or DAT assets.

A named DAT model may contain several closed components that touch along an edge.
CityHub's `startfences` has three such components, `Buildings` two. Each has two
shared edges with four oriented face incidences. The former requirement of exactly
two incidences over the entire model rejected these otherwise independently closed
components. Simply accepting balanced four-face edges would also accept ambiguous
geometry; the new validator requires an independent closure proof.

After the existing base-ring selection, plane checks, 0.01-unit topology welding,
face orientation and T-junction splitting, connect faces only across edges with
exactly two oppositely directed uses. Weighted disjoint sets with path compression
keep this build-time grouping bounded. Group every edge's incidences by component:
each group must have exactly two distinct faces with opposite directions. An open,
misoriented, duplicated or still non-manifold component fails the whole build and
clears geometry and diagnostics counters. Coincident faces are not automatically
removed or merged. Vertex-only contacts and disconnected closed components remain
valid. Ambiguous pinches that cannot be separated this way still fail.

The existing 4,096-point / 4,096-face / 512-indices-per-face and 16-million
edge/vertex comparison limits remain. Face incidence storage is bounded by that
same splitting work; component storage is bounded by the face limit. No new work
or allocations are introduced in per-frame contents/collision queries. This is
not a general self-intersection or constructive-solid-geometry solver; those
capabilities were not present in the earlier validator either.

All faces, planes, triangle positions and materials retain their actual values.
The existing solid-angle contents and triangle BVH queries handle the validated
compound model. Broad-phase bounds do not fill gaps. Original `BoxPhysics` remains
an authored rule: CityHub `startfences` uses polygon collision, while `Buildings`
uses its overall box. No malformed model receives an automatic box fallback.

Ground placement now also uses `sweep_object` for each checked solid candidate.
The previous `MoveObjectToGround` implementation tested every object as a box at
its pivot, incorrectly filling polygon gaps and misplacing offset box bounds.
The original helper's downward query (`ObjectUtilities.cpp::MoveObjectToGround`),
character dimensions, distance formula and final collision-checked movement are
unchanged. An unavailable geometry query returns an error. Synthetic ground probes
verify support over each actual piece, empty gaps and the authored box mode with
a deliberately offset pivot; CityHub retains the prior 18 ground no-hit results
for its 98 gravity-enabled CDestructableModel objects.
Source references are `ObjectDLL/DestructableBrush.cpp::PostPropRead` and
`ObjectDLL/Door.cpp::PostPropRead` in `sources/blood2/`; neither is modified.

`closed_components` and `shared_edges` belong to the immutable shape, survive
checked transforms and are zero after failure. The common selected-object,
Containers and Doors inspection panels display them, including hidden/inactive
objects. Source failures remain listed separately. Draw submission still checks
current visibility and lifetime; pending removal makes a model disappear before
deferred destruction. World unload/reload does not retain stale object bindings.

The private door catalog now creates 619 doors (14 newly accepted), with 149 owned
SelfTriggers and 114 polygon-mode doors. The 79 rejections remain explicit: 47
historical TypeAction schemas, 19 ambiguous names, 12 missing models and one
unresolved shell (`worlds/multi/bb_condemned.dat`, `Buildings`). The latter has four
uses in a single connected component, so balanced winding is insufficient.
The later [object contracts](object_contracts.md#retired-world-properties) supersede TypeAction rejection: the retired flag is accepted and never read.
Duplicate names now bind the first model in file order ([duplicate-name rule](object_contracts.md#duplicate-bsp-names)).
The later [box-world model admission](../implementation/reports/all_level_loading.md) creates BoxPhysics
`Buildings` as a boundary-only mesh (one open, one shared edge): the catalog now creates 690 doors and
rejects 12, all missing models.

The brush catalog additionally recovers three `startfences` instances across the
CityHub variants: 2,298 CDestructableBrush instances create, 157 reject, and all
98 supported RotatingBrush instances remain unchanged. Existing explosive/spawn
counts and failure categories stay checked. Two invalid brush shells still fail.

CityHub now has 421 created entries, 31 transferred sounds, 541 editor-only,
31 unsupported and two failed entries. `fallengrills` still lacks a named model;
CultistAI0 still requests the unimplemented FLAREGUN. Structural destination
preparation remains 454 checked / 31 unknown / zero invalid. Destination rehearsal
retains the source session; no automatic NEW_LEVEL transfer is enabled.
Required remaining services keep M2 incomplete. This increment does not claim
retail physics equivalence or complete CityHub playability.
