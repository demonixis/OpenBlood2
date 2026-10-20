# M2 — NPC animation clearance and continued AI updates

Status: implemented for the checked animation/resize service; **M2 remains incomplete**.
Implementation commit: `43aeaf8032dfb9cc7db5ad3277ce5cdc1904ec45`.

## Reproduced issue and source behavior

Following the report of stationary Cultists, a diagnostic encounter in SubwayA
reproduced a latched `AI_Update: AI animation resize blocked` failure. The old port
sent a blocked standing animation toward CrawlUnder; another resize refusal then
stopped scheduling and left a manually inactive actor. Ordinary activation pings
cannot revive a runtime failure.

Original `AI_Mgr::SetAnimation` ignores SetObjectDims2's result, selects the clip
and caches the mutable dimensions argument. The correction distinguishes an
ordinary collision refusal from unavailable/invalid services. Valid animations
continue while the replacement solver retains the existing physical box and
position. The AI caches those retained dimensions; the requested dimensions remain
inspectable. Same-clip reset still does not retry resizing.

The later LithTech SetObjectDims/ChangeObjectDimensions implementation corroborates
an in/out dimensions argument, but returns an attempted partial size while rolling
back the body. Our atomic solver has no partial result; caching actual retained
bounds is an explicit replacement policy, not recovered 1998 physics. It also keeps
the existing corpse-placement source box consistent after blocked animation changes.
No activation override, damage/health tuning, extra physics integration or rendering
clock was added. Missing clips, invalid dimensions and unavailable collision still
fail. The default session difficulty remains Medium; existing original difficulty
multipliers are unchanged. High perceived durability has not been attributed to
this animation defect.

The Skeletons inspector shows the blocked-resize count, last-call blocked flag,
last requested dimensions, cached AI dimensions and physical dimensions. Hidden
and inactive actors remain inspectable. See [contracts](../../contracts/ai_animation_clearance_contracts.md),
[source ledger](../sources/m2_ai_animation_clearance_sources.json) and
[retained evidence](evidence/m2-ai-animation-clearance.json).

## Validation

The retained negative control compiles the previous AIState.cpp against the same
current services and object layout. It fails the new private regression with the
original latched resize error. The corrected implementation passes two complete
reloads. Each run completes the authored intro, then uses two explicitly labelled
player relocations in the train, with player damage disabled and NPCs otherwise
using their original updates. Each records **eight blocked resizes and 93 original
AI shots**, with no suspended Cultist. An additional explicit NEVERGIB/death packet
checks that an affected actor produces a live nonsolid CCorpse and is removed.
This is a controlled regression, not a continuous human playthrough.

Synthetic tests verify safe body retention, exact requested/cached dimensions,
continued animation and state phases, hidden updates, same-clip reset, later clear
resize, invalid/unavailable services, pending removal and the standing-fire,
walking, script-running and crawl callers. Existing AI, object ownership,
player restore, corpse and level-transfer checks are included. The GPU character
smoke now contains a labelled blocked-clearance fixture and requires continued
original action playback, including its existing hidden-inspector/reentry checks.

**157 distinct headless checks ultimately pass in each of Debug, inspector-free
Release and Clang ASan/UBSan**, including 24 private-data checks. The initial run
caught a new test assertion sampling at a loop boundary, then comparing milliseconds
with seconds; its final 25-ms sample and the strengthened private corpse assertion
were rerun. Earlier logs and final per-test results remain in the evidence manifest.
Four NVIDIA GPU scenarios pass in each graphical build: character inspection,
model depth, private models and repeated NEW_LEVEL. They select
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json` and report the
NVIDIA GeForce RTX 3060 Laptop GPU. The local inspector capture was inspected;
commercial data and images remain outside Git. No shaders/dependencies changed,
and native Metal was not retested.

All 10,923 inventoried reference files and six reference repository states remain
unchanged. The implementation and documentation are committed independently; exact
revision and patch/file hashes are recorded in the evidence manifest.

## Remaining limits

This fixes one reproduced permanent suspension and checks its death continuation.
It does not prove that every inactive Cultist should activate, replace retail PVS,
implement partial resizing/pushing, or establish complete encounter/corpse parity.
Other missing services can still suspend actors explicitly. Missing CityHub water
presentation, broader balance/fire reports, remaining frame costs and M2's actual
acceptance gate remain open. The README milestone estimates are unchanged.
