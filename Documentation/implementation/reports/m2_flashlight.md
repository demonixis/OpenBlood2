# M2 — original flashlight

Date: 2026-10-17. **M2 remains incomplete.**

This increment connects CityHub's original FlashlightPU, CInvFlashlight inventory
and CFlashlightFX ray rules to a visible, bounded point light. It follows the
[remote bombs](m2_remote_bombs.md). CityHub has **453 created / 31 transferred world
sounds / 541 editor-only / zero unsupported / one failed** entry (1,026 total).
The missing `fallengrills` model still blocks safe NEW_LEVEL promotion.

Implementation commit: `d9a83f0b7c1b4d79643fc2b70e15b456416cf728` in the independent `Blood2/` repository.

See [contracts](../../contracts/flashlight_contracts.md),
[source hashes](../sources/m2_flashlight_sources.json),
[validation ledger](evidence/m2-flashlight.json) and
[implementation patch](evidence/m2-flashlight-implementation.patch).
Reference repositories, commercial resources and dependency pins are unchanged.

## Observable behavior

Collect the authored flashlight, then press **L** to toggle it; release Shift
first, as the original direct-item command rejects running. Keyboard focus,
text editing, camera/death restrictions and player replacement retain their
existing gates. Neither a flashlight nor a pickup is invented at the train spawn.

The item starts with 120 seconds of use and 100 displayed charges. Its original
rounded charge controls recharge refusal: it can still display 100 after a short
use and refuse another pickup. Active simulation time drains it; exhaustion turns
it off. Both toggle directions play the original `sounds/powerups/flashlight.wav`.
Pickup sound and the original PCX inventory HUD remain unavailable.

The source child follows the player's body position and internal pitch/yaw. Its
ray places a light at the hit surface; radius is 40% of the distance, with the
original minimum distance and intensity rules. The unavailable point-shade query
uses the original failure fallback, explicitly shown in inspection. A ray miss,
off item or removed source/owner produces no light.

SDL_GPU renders radial brightening over the current unlit world and model
textures, including transparent world faces. This replacement does not claim
retail lighting, lightmaps, normals, shadows or occlusion. Sky, sprites/FX and
near view weapons retain their existing illumination. SPIR-V and Metal/MSL are
regenerated together with the existing pinned ShaderCross tools.

The Pickups inspector exposes charge, remaining time, toggle state, child lifetime,
ray outcome, light position/radius/intensity and errors. Saved inventory inspection
shows charge and the forced-inactive restoration rule. No inspector/render call
advances gameplay clocks.

## Ownership and restoration

The item owns a checked server child; the client effect owns a light value. Removal
is deferred and checked pending identities immediately suppress rendering. Invalid
updates propagate to the session and retain diagnostics. Queue/audio failure does
not undo the source toggle. The existing bounded solid ray excludes the local
player's authoritative body; this is explicit replacement client-ray policy.

Save/Restore and KeepAlive retain count and remaining time while recreating a
hidden child under the new owner. Original InvItem::Load forces inactive in both
modes, despite the narrower comment in CInvFlashlight::Load. No old identity or
active light survives restoration. Malformed snapshot values reject before
mutation; source/destination teardown retains its owning adapter.

## Verification

Debug and inspector-free Release each pass **783/783 headless tests**, including
**128 private-data cases**. Clang ASan/UBSan with leak detection passes **45/45**
object, inventory, restoration and destination-lifetime cases (five private).
The focused flashlight/medkit/catalog/transaction/shader suite passes **20/20**.
**22/22 NVIDIA Vulkan GPU checks pass per configuration**, including 14 private
cases. They cover the lamp, world/model depth, alpha, sky, sprites, surface panning,
mouse combat, corpse passage, NEW_LEVEL, UI lifetime, pickups and both bomb types.

Nine new flashlight cases cover rounded charge/refill, exhaustion, input gates,
real aggregate updates, hit/miss/failure rays, body/pitch alignment, pending removal,
owner teardown and inactive restoration. NEW_LEVEL rejection preserves an active
source lamp; three successful synthetic promotions preserve charge while recreating
an inactive light. Malformed snapshots leave existing inventory/output unchanged.
The authored CityHub pickup and real on/off WAV playback are exercised twice.
The 57 readable base worlds contain ten FlashlightPU entries: all ten create
with their actual models. Supported pickups now total 1,526 creations, zero
missing models, 220 unchanged TypeAction rejections and 60 destruction spawns.
The existing `16_bridge.dat` reader limitation is unchanged.

All **47 changed/new non-generated C++ files** pass clang-format 23.1.3.
**10,923 reference files and six reference repositories** remain unchanged.
Builds use the existing configured directories and pinned stack. Run
`cmake --build build/<debug|release> -j3`, then
`ctest --test-dir build/<debug|release> -L headless -j3 --output-on-failure`.
The ledger records exact sanitizer/GPU selections, logs, source hashes and patch.
GPU checks explicitly select the NVIDIA Vulkan ICD and verify the actual runtime
device: NVIDIA GeForce RTX 3060 Laptop GPU. The existing ItemPickups float/GameType
comparison warning remains.

Initial catalog/GPU probes still asserted pre-flashlight class totals. Their
expected accounting now reflects the ten verified pickup creations and CityHub's
485 schema-checked entries; missing geometry and TypeAction remain rejected.
The generated Metal helper introduces a diagnostic pragma before the stdlib
include; tests verify actual includes, bindings and light fields rather than the
old first-line layout.

NVIDIA readback measures **37,859 brighter RGB channels and zero darker channels**
on the actual SubwayA floor, with original ray distance 42 and radius 16.8, across
two loads. Controlled textured-wall checks cover moved/disabled lights, posed
world coordinates, resized targets, alpha and excluded sky/FX/view-weapon passes.
The before/after captures were inspected visually and remain ignored private files.
All six generated shader headers reproduce byte-for-byte. Native Metal execution
of this increment has not been automated on this Linux host.

## Remaining gate

Read-only inspection finds no DAT world-model named `fallengrills`; the only
`grill`-named model is `boilergrill`, with eight points and six polygons. Assigning
it to the failed object would be an unsupported substitution. The failed entry
remains explicit and destination promotion retains the source session.

The unchanged [M2 exit criteria](../../history/Roadmap/03_milestones.md), remaining required
services, unsupported historical schemas, original HUD/audio gaps and broader
level/encounter validation stay open. Complete CityHub playthrough, retail lighting
parity and native Apple automation are not claimed.
