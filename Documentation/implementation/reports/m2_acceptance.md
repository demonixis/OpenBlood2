# M2 — Acceptance and Closure with Reservation

Implementation commit: `a17e3244057ee1d7cdc72e3471295f9e9aa145ed`.

Review and user-authorized closure date: 2026-10-18. This is the authoritative consolidation of the
[unchanged M2 criteria](../../history/Roadmap/03_milestones.md), superseding chronological
claims that the player, NPCs, MUSIC or NEW_LEVEL are still wholly unavailable.
The selected game path and the external graphics qualification are separate.

## Decision

**M2 is closed with reservation M2-GPU-001**, explicitly authorized by the user
on 2026-10-18. The selected functional path has no reproduced progression blocker:
original SubwayA entry, intro, combat, contacts, authored exit and preserved
CityHub entry are exercised. The sole accepted closure exception is the external
NVIDIA/D-Bus GPU LeakSanitizer failure. Headless game/session sanitizers pass;
GPU sanitizer processes still fail and are not reclassified as passing. This
closure decision supersedes the earlier hold recorded in the immutable evidence.
M3 is the next milestone and awaits a separate request; it is not accepted here.

No new weapon, music subsystem or campaign feature is required to explain that
external reproduction. The next qualification action is to rerun the isolated
platform probe and selected session after a relevant graphics/platform change.
The user has assigned graphics optimization to another computer; this review
neither modifies that stack nor pulls or assumes unpublished changes.

## Reservation M2-GPU-001

**Status: open external platform issue; accepted exception for M2 closure only.**
The user expressly requested closing M2 with a documented NVIDIA/D-Bus leak
reservation, committing the documentation and disabling the M2 automation.
The original milestone criteria, tests, deadlines and failure results remain
unchanged. This exception does not waive any game-owned leak, gameplay regression,
required unsupported service or broader platform/release acceptance.

The affected tested configuration is Linux/Wayland, NVIDIA GeForce RTX 3060 Laptop
GPU, driver 615.78.08, and the pinned SDL Vulkan backend. The earlier SDL-only and
standalone Vulkan controls reproduce the allocation families without game code.
The review's full instrumented route reaches its final assertions but exits 1
with 44,024 bytes in 816 allocations. Rechecking the rebuilt SDL-only probe on
2026-10-18 at 08:09 UTC gives window-only exit 0 and GPU exit 1 with **43,992 bytes
in 816 allocations**. The small byte variation does not resolve the failure or
prove a harmless constant allocation; growth across three device lifetimes
remains in the prior evidence. No new full gameplay suite was run for closure.

Follow this issue in graphics/platform qualification (M4), retaining the
[isolated reproduction](m2_gpu_lifetime.md) and this reservation. Graphics
optimization is assigned to another computer; await the user's push notification
before integration. No graphics dependency, shader or renderer is changed here.

To lift the reservation after a relevant platform/graphics change:

1. Record the implementation/dependency revisions, OS, driver and actual GPU.
2. Rerun the isolated video/window and one/three-cycle GPU controls, plus the
   standalone Vulkan control when investigating allocation attribution.
3. Rerun the selected instrumented train-to-CityHub session through teardown,
   alongside the ordinary NVIDIA rendering/lifetime checks.
4. Require normal successful exits and no ASan/UBSan/LSan errors on these paths,
   with leak detection enabled and no suppressions, retained-library tricks or
   skipped teardown. Retain failures if the issue persists.

The [closure ledger](evidence/m2-closure.json) preserves the authorization,
reservation, unchanged reviewed code revision and the earlier recheck logs.
Original acceptance logs, patches and hash ledgers remain immutable. This is a
documentation-only closure: links, status consistency and evidence hashes are
checked; no new successful build, playtest or sanitizer result is claimed.

## Criteria and evidence

| M2 criterion | Evidence and bounded conclusion |
| --- | --- |
| 1. Strict, bounded readers | `world.rez_structure`, `rez_rejection`, `world_rejection`, `mutations`, `services.model_rejection` and the prior complete parser/resource suite exercise malformed counts, offsets, cycles and generation boundaries. No permissive parser change in this review. |
| 2. World, properties, models, transforms | `world.world_properties`, `world_geometry`, `services.model_reader`, `private`, `placement_queries` and `placement_service`; checked startup decodes world and hidden model/sprite resources. [Model basis](m2_model_view.md) and [CityHub SPR materials](m2_cityhub_entry.md) retain explicit conversions and generation limits. |
| 3. Original session, player and required classes | `world.session_start_private`, `world.level_change_private`, `coverage.private` and the continuous train tests enter original shells, restore the live player/client, select `01_egressa` and tear down. Startup now participates in the continuous route, not only a separate test. |
| 4. Actual collision, properties, targeting, lists, links, attachments | `services.collision_queries`, `collision_traversal`, `movement_contacts`, `original_services`, `objects.reasons_properties`, `links_lists` and previous attachment/mover regressions. The route crosses the train through production movement and combat; replacement physics is not claimed retail-identical. |
| 5. Animation and original callbacks independent of drawing | `services.animation_events`, `model_callbacks`, original scheduling/removal tests and both headless train rates. Actual weapon FIRE_KEY, damage, deaths, pickup contacts and delayed exit messages are asserted. Rendering checkpoints preserve the simulation clock. |
| 6. Trace and explicit refusals | `coverage.catalog` checks all 171 interface entries plus three supporting boundaries; `coverage.observations` proves failed calls remain failures. Every route checkpoint exports all 174 rows. New per-step actor/client/FX/audio checks supplement attempt counts. Unexercised rows remain explicit in the accompanying service table. |
| 7. Truthful inspection | Existing World, Objects, Services, skeleton/attachments, collision and resource views expose loaded, hidden and rejected elements. [Startup inspection](m2_session_start.md) and prior GPU service/ownership inspection remain evidence. This review adds the exact failed blood-impact sound path to live and expired records. |
| Exit: bounded malformed input and lifetime | The selected current headless regression suite and prior [complete non-GPU audit](m2_session_cost.md) pass with ASan/UBSan and leaks enabled. Generic unsupported saves remain outside the selected scenario. |
| Exit: instrumented GPU process | **Fail; accepted closure reservation M2-GPU-001, external reproduction retained.** [SDL-only and standalone Vulkan probes](m2_gpu_lifetime.md) reproduce the NVIDIA/D-Bus allocations without game objects, assets or renderer code. No sanitizer suppression or test-criterion edit; the user-authorized closure exception is recorded above. |

## Source accounting and intentional limitations

SubwayA accounts for 580 rows: 204 created objects, four transferred sounds and
372 editor-only entries; zero unknown or failed rows. CityHub accounts for 1,026:
453 created, 31 transferred sounds, 541 editor-only and one explicitly omitted
missing named BSP. `fallengrills` is not an empty successful brush. The
[documented loader adaptation](m2_cityhub_entry.md) rejects its object before
initial update; later-generation LithTech evidence is not a retail binary trace.
Other absent resources, ambiguous geometry and unsupported classes still fail.

`SaveObjects`, `RestoreObjects` and `EndHMessageRead` remain unsupported and unused
by this route. Typed player KeepAlive is not a general save format. A zero call
count is unexercised, not implemented-by-assumption or automatically unnecessary;
focused subsystem tests and the selected scenario determine the scope. The
catalogue covers the port's interface, not every historical SDK/client method.
Full CityHub/campaign traversal, every NPC action, alternative weapons and future
resource requests are not certified by a short destination-entry check.

## Service trace reconciliation

The [complete 174-row table](evidence/m2-acceptance-services.tsv) is generated from
the current inspector-free Release routes, with separate counters for each world,
rate and teardown. SubwayA reaches 137 boundaries at 30 Hz and 135 at 60 Hz;
CityHub's short entry reaches 96, growing to 101 after teardown. The union reaches
147; 27 remain unexercised. No row is unclassified and no generic unsupported call
occurs. Attempt counts are not successful-operation counts.

The 27 unexercised entries are retained, not removed from the acceptance matrix:

- Lists/strings/messages: `AddObjectToList`, `CreateObjectList`, `RemoveObjectFromList`,
  `CopyString`, `EndHMessageWrite`, `ResetRead`, `ReadKeyMessage`, `WriteKeyMessage`.
  Object ownership/message and trigger-key tests exercise their broader families;
  this train route does not exercise every public boundary.
- Query/transform/audio branches: `GetAcceleration`, `GetAttachedModelNodeTransform`,
  `GetObjectColor`, `RotateWorldModel`, `PlayLocalSound`, `SendConversation`.
  Their contracts and focused attachment, rotating-door, color, item and
  conversation tests remain separate evidence, not claimed train observations.
- Alternative projectile/effect/negative paths: `AdvanceModelProjectile`,
  `PublishBloodSpurt`, `PublishFleshGibs`, `PublishLaserBeam`, `PublishObjectParticles`,
  `RecordBrushDestruction`, `RecordDebrisEffect`, `RecordExplosionEffect`,
  `RecordProjectile`, `ReportSkyFailure`. Client blood publication is reached on
  the route; server publication is a different boundary. Broader weapon/death,
  destruction and rejection tests remain necessary when extending the scenario.
- Unsupported: `SaveObjects`, `RestoreObjects`, `EndHMessageRead`. They reject in
  negative controls and are not called by the accepted functional path.

At 30/60 Hz the route records ten authored Cultist deaths each, health and ammo
contacts, all ten relay sends and successful original transition. The session
creates/releases 83,246/65,534 transient messages, with peak three simultaneous
messages. The 30 Hz route also fires two knife FIRE_KEY shots. Seven/eight observed
blood-impact requests fail on the original absent zero file; both runs report
zero unexpected FX/client/audio failures. These are scenario observations, not
retail parity or exhaustive future-branch coverage.

The separate view-model key inventory finds six Beretta clips and five knife
clips; every nonempty event is `FIRE_KEY`. The existing Release weapon harness
repeats this inventory over three loads and exits 0. This read-only asset evidence
uses the previously built, internally consistent weapon executable; it is not
included in the 64 current rebuilt regression cases. Its hash and command are
retained. The original `SOUND_KEY` branch actively sends `CMSG_WEAPON_SOUND`, even
though its direct local playback line is commented out. That message path and
loop/stop/show/hide extensions must not be claimed supported merely because a
resource is indexed. They are not requested by these two selected view models;
other weapon assets remain outside this route's acceptance.

## What the review corrected

The previous route only checked generic unavailable calls, message refusals, NPC
runtime failure and door audio. It now also checks selected actor/inventory
errors, retained FX reports, stream/output/music errors and client sound failures
throughout execution. The scene still must render real pixels on NVIDIA.

The stricter probe found original blood-impact requests for
`sounds/gibs/flesh/gib_impact0.wav`, which is absent in the installed content.
Original `ParticleExplosionFX.cpp` deliberately draws zero through `GetRandom`;
`SharedDefs.h` defines the selected B2_LITE range. This is an original content
anomaly, not a missing engine audio implementation. The actual requested path was
missing from the inspector/report, so it is now retained and displayed. No sound
alias, gameplay rule, RNG order or successful silent voice was introduced.

The test accepts only that exact absent-file case as a separately counted failed
cosmetic request. A different absent file, decode failure, FX failure or client
voice failure still rejects acceptance; dedicated negative controls verify this.
The synthetic missing-zero test checks that splats survive audio refusal, valid
variants really start voices, and the failed path survives emitter expiry.

The initial expanded route failed on the missing sound. A later audit incorrectly
treated the historical `client.blocker` scope note as a runtime error; it is not
one and is no longer included as such. An initial new expiry test used a two-second
step outside the existing one-second effect limit; it now advances through valid
bounded steps. No production limit or existing gameplay assertion was relaxed.
The first compile also used a map entry as a voice; the structured binding was
corrected. Failed attempts remain in the evidence ledger.

## Validation

All three configurations build successfully, with at most three compiler jobs.
The **64 selected headless cases pass in each configuration** (192 executions),
including malformed readers, startup/refusal, original callbacks, lifetime,
private-data transitions, both complete train rates and the new negative controls.
This is targeted regression coverage; the earlier complete non-GPU audit remains
separate historical evidence, not a claim that this turn reran the whole suite.

| Configuration | Headless | NVIDIA GPU checks | Headless time |
| --- | ---: | ---: | ---: |
| Debug, inspector enabled | 64/64 | 5/5 | 82.71 s |
| Release, inspector disabled | 64/64 | 5/5 | 14.09 s |
| Clang ASan/UBSan | 64/64 | Process fails LSan; see below | 496.48 s |

The ten ordinary GPU checks cover device/application lifecycle, world presentation,
synthetic/private blood effects and complete train-to-CityHub rendering. All log
NVIDIA GeForce RTX 3060 Laptop GPU through the NVIDIA Vulkan ICD. The CityHub
checkpoint was visually reviewed. Dummy audio verifies submission/lifetime, not
a listening comparison. No native Metal execution is claimed.

Fresh unsuppressed sanitizer GPU checks remain failures: SDL-only reports
**44,024 bytes / 816 allocations**, and the current full route reports
**44,024 / 816** after reaching its final teardown assertion. Both exit 1.
Leak detection and halt-on-error stay enabled; no suppression or successful-process
reclassification is introduced. This matches the separate platform reproduction
in allocation count and retained NVIDIA/D-Bus failure pattern. The older standalone
Vulkan/loader-attribution evidence is retained, not rerun or resymbolicated here.

The [evidence ledger](evidence/m2-acceptance.json) retains exact commands, configuration,
individual results, complete Release/sanitizer service rows, current source delta,
source hashes and failed attempts. Passing-test output retention was increased only
for logs; test deadlines and assertions are unchanged. Changed C++ formatting,
maintained Markdown links and whitespace checks pass. All 10,923 inventoried
reference files and six reference repositories remain unchanged. The user-edited
workspace README is byte-identical; no commercial images or binaries are committed.


## Graphics failure and later milestones

The prior unsuppressed GPU route completes its behavior/render/teardown assertions
then exits 1 with 44,056 bytes in 816 allocations. SDL_GPU alone reports 44,024 /
816; direct Vulkan alone reports 21,996 / 408. Fresh loader maps locate the allocator
callers in NVIDIA modules and D-Bus. The 32-byte D-Bus difference and growth across
three device cycles remain explicit; allocation-site attribution is not a claim
about exact ownership or a harmless constant cache. See the immutable
[GPU investigation and evidence](m2_gpu_lifetime.md).

M3 still needs the declared reference comparison and reproducible interactive
slice acceptance. M4 covers rendering/inspection completeness and platform
validation. M5 covers campaign behavior and persistence; M6 measured desktop
optimization/releases; M7 mobile; M8 OpenXR. Partial implementations and successful
user playtests do not complete those milestones. Graphics optimization can proceed
independently, but its eventual push requires integration review and revalidation.
