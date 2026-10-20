# M2: Original M16 and grenade

The original M16 weapon, AssaultRiflePU, alternate-ammo selection and CGrenade
callbacks now execute in the new project. Read the [contracts](../../contracts/assault_contracts.md)
and [source ledger](../sources/m2_assault_sources.json). M2 remains incomplete.

Primary FIRE_KEY events produce original bullet damage and fire/impact audio.
Alternate events consume five DieBugDie charges and launch the actual grenade
model with gravity, original contacts/spin, loop/bounce sounds, a strict three-second
fuse and radius damage. Initial living-character impact and damage death preserve
the source's two sequential explosion passes; fuse expiry performs one. Existing
available grenade explosion components are reused; incomplete FX stay explicit.
The source M16 does not support a second left-hand acquisition.

AssaultRiflePU supplies 50 bullets, with original full-pool refusal. Independent
NPC hand drops create that pickup after landing. Inventory restoration preserves
both ammo pools and rejects a fake dual M16. Typed pickup IDs remain explicitly
allowlisted. The inspector exposes per-mode ammunition, original resources,
grenade contact/spin/fuse state, lifetime history and failures independently of UI
visibility. The diagnostic grants are separate from authored collection.

The original male Cultist has no rifle/automatic-rifle animation. Direct original
AI Fire succeeds in a labelled synthetic arena, but autonomous AI_Update correctly
fails at the missing animation. This increment does not fabricate a pistol fallback
or claim autonomous M16 NPC combat. Rifle-bearing Fanatic/Prophet remain outside
this subset.

The private M16 view ABC also exposed finite inverted per-key bounds in
end_alt_fire_craig. Both reference readers discard this metadata. The new reader
preserves and marks it, while retaining strict finite/range, time, hierarchy,
pose, deformation and effective animation-bound validation. GPU rendering uses
the actual ABC/DTX. No commercial file is patched or copied into the repository.

The grenade uses a bounded replacement box solver, with one integrator, existing
volume callbacks, conservative contact placement and source coefficient 18 under
the existing ground-friction policy. Inward velocity is projected before the
original bounce expression; no retail restitution is claimed. An embedded start
or unavailable query fails visibly. Source smoke-trail binding, screen shake,
retail trajectory parity and complete combat presentation remain open.

CityHub now reports **433 created / 31 transferred sounds / 541 editor-only /
20 unsupported / one failed**. Its authored M16 pickup is created. Structural
preparation reports 465 checked / 20 unknown / zero invalid entries. The missing
fallengrills world geometry, remaining classes/services and safe automatic
NEW_LEVEL still prevent M2 completion. SubwayA's existing class counts are unchanged.

The private pickup catalog checks all 57 parseable base-world files. It now creates
1,270 supported pickups, including five authored M16 pickups, with no missing
models. All 176 unsupported historical TypeAction entries remain rejected; the
60 supported destruction-spawn requests are unchanged. This is catalog coverage,
not a playthrough of those levels.

## Validation

Implementation commit: `18752f58a367073030dc9dbed916f9862bbb9743`.

Commands, final logs, initial catalog failure and capture hashes are recorded in
[evidence](evidence/m2-assault.json). Linux/Vulkan only; Debug includes Dear ImGui,
while Release disables the inspector. Synthetic arenas are labelled separately
from actual world entry; none establishes a complete campaign playthrough.

| Check | Result |
| --- | --- |
| Debug headless | 714/714, including 115 private-data tests (114.31 s) |
| Release headless, inspector disabled | 714/714, including 115 private-data tests (16.84 s) |
| Debug GPU | 7/7, including five private-data tests (45.30 s) |
| Release GPU | 7/7, including five private-data tests (12.08 s) |
| Clang ASan/UBSan, leak detection enabled | 102/102, including 14 private-data tests (331.65 s), no finding |
| Formatting/reference integrity | 37 C++ files checked; 10,923 reference files unchanged; six reference repositories clean |

The fourteen new behavioral cases cover exact ammo consumption, FIRE_KEY-driven
primary/alternate transitions, dry refusal, damage, independent gravity, original
bounce/spin, living/dead/nonsolid contacts, strict fuse expiry, sequential explosion
passes, callback removal, failed resources/queries, pickup/drop/restoration and
real external assets. Parser cases retain malformed finite metadata while rejecting
nonfinite coordinates and out-of-duration keys. Existing weapon, object, service,
destination and inventory lifetimes also pass the selected sanitizer run.

Initial full Debug/Release runs each passed 713/714. The sole failure was the old
pickup-catalog expectation of 1,265 creations. Registering AssaultRiflePU adds five
real authored pickups. The final test requires exactly five, verifies their type,
50 bullets and resource ID, and expects 1,270 total; all historical rejection,
missing-resource, spawn and lifetime assertions remain. Both complete suites were
rerun successfully. The initial failure logs are retained.

GPU checks cover application/selection teardown, temporary destination rehearsal,
existing Mac-10/Flare Gun/energy presentation and the new original models. Each of
two M16 load/unload cycles renders 30,030 weapon pixels and 5,936 isolated grenade
pixels in both builds. Hidden/removal/unload state, independent clock/flight state,
attachments and voices are checked. Both configurations' first-cycle captures were
visually inspected; commercial images remain in ignored build directories. No GPU
validation error was reported.

## Remaining M2 work

The original male Cultist still cannot autonomously fire an M16 without a rifle
animation; the separate direct-fire check does not hide that failure. Original
rifle-bearing NPCs, complete grenade presentation and verified retail trajectories
remain outside this increment. CityHub's 20 unsupported entries, missing
fallengrills geometry, required remaining services and safe live NEW_LEVEL transfer
keep the existing M2 gate open. M3–M8 remain planned.
