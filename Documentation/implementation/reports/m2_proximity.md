# M2 — original proximity bombs

Date: 2026-10-17. **M2 remains incomplete.**

This increment connects the two authored `ProximitiesPU` entries in CityHub to
original inventory-weapon acquisition, charged throwing and `CProximityBomb`
arming/detection/detonation. It follows the [sky continuation](m2_sky.md) and
reduces that world's unsupported source entries from five to three. No commercial
resource, reference repository or pinned dependency is modified.

Implementation commit: `e7726e99f70fadad2487885894ba8505b770dd06` in the independent `Blood2/` repository. Read the
[contracts](../../contracts/proximity_contracts.md),
[source ledger](../sources/m2_proximity_sources.json) and
[validation ledger](evidence/m2-proximity.json) for boundaries and evidence.
The patch is retained in [m2-proximity-implementation.patch](evidence/m2-proximity-implementation.patch).

## Observable behavior

- Collect a proximity pickup, then press **P** to select it. The inspector also
  exposes reserved slot 10. Each pickup adds one bomb; the original pool caps at
  ten and refuses full-pool collection. Existing focus, text-entry, camera and
  dead-player restrictions apply. The original bomb command permits running.
- Hold primary fire to charge the throw, then release. The actual view model's
  `end_fire` FIRE_KEY creates the projectile; the original charge curve spans
  250–1750 units/s. Holding beyond 1.25 seconds produces the original in-hand
  explosion and direct self-damage. It does not fabricate an area-damage pass.
- Secondary fire drops a bomb from the player's center with downward direction,
  zero initial speed and gravity. A normal throw/drop consumes the last bomb and
  switches to the next ordinary weapon. The in-hand early return preserves the
  source's different exhaustion behavior.
- Solid contact sticks the bomb and clears gravity. Player-fired bombs arm after
  0.75 seconds; a valid AI shooter changes this to two seconds. Detection scans
  living characters in original destructible-list order within inclusive radius
  225, including the shooter and without a visibility/faction filter. Detection
  latches a half-second delay. Direct living-character contact bypasses it.
- Original WAVs play through existing checked audio. The projectile uses actual
  `c4.abc`/`c4_prox.dtx`; the hand/view/pickup models use their original resources.
  Available explosion components render independently; missing components remain
  recorded failures. The inherited ten-second lifetime is deliberately not a
  fuse: the original override never calls the base Update.

The Objects inspector shows current and hidden bomb resources, arming/contact
state, detected-target liveness, detection scans, delays, spin and sound failures. Weapons
shows charge time, speed and ammo. Terminal projectile history retains the final
arming/detection/contact record after removal. Inspection/rendering does not
advance the bomb clock. Scene explosion records report requested damage/radius;
actual damage deliveries remain in the projectile report.

## Source and engine boundaries

The game code comes from original `GameProjectiles.cpp`, `GameWeapons.cpp`,
`InventoryMgr.cpp`, `ItemPickups.cpp`, `Shared/WeaponDefs.cpp` and the original
client weapon/shell branches. The source ledger records the complete consulted
file set and hashes. Typed messages replace packet parsing and its special float;
malformed/nonfinite throw speed rejects before ammo consumption.

Thirteen owned weapon slots retain the original ten ordinary plus three reserved
layout. Only proximity slot 10 is implemented; remote/time slots remain empty.
Ordinary weapon search and `FindBestWeapon` still scan only the original first ten.
Supported player restore and KeepAlive retain slot 10, raw ammo and selection;
invalid reserved-slot arrangements reject before mutation. A separate legacy
`CInvProxBomb` menu node is unnecessary for this supported representation; full
menu/PCX HUD and projectile save/load are not claimed.

The existing grenade flight service becomes `AdvanceModelProjectile`, restricted
to the two implemented model-projectile classes. It preserves bounded swept-box
gravity, container evaluation and deferred contact callbacks; sticking ends the
remaining substeps. This is replacement collision, not recovered retail physics
or attachment to moving surfaces. Removing the shooter clears attribution without
destroying the bomb. Detection uses a non-owning historical token. Damage callbacks
may remove themselves and the bomb without freeing either during dispatch.

## Validation

- Debug and inspector-free Release: **764 headless tests each**, including **125
  private-data tests**. All 65 relevant Debug object/inventory/projectile/player/
  pickup/CLI cases pass again after the final lifetime guards and report fields.
- Clang ASan/UBSan with leak detection: **49 tests**, including seven private-data
  cases. Coverage includes owner removal, damage-callback removal, teardown,
  inventory restoration and KeepAlive.
- NVIDIA Vulkan: **ten unique GPU checks per build**, including nine private-data
  cases. Actual bomb view/world models render through two load/unload cycles;
  pixel readback measures 12,464 view pixels and 8,976 world-model pixels per cycle.
  Hidden/removal cleanup and a read-only render clock are asserted. Captures were
  visually inspected and remain ignored local artifacts.
- Regression coverage includes M16 grenades, mouse primary/secondary combat,
  corpse passage, safe NEW_LEVEL refusal, objectives, pickups, glass, sky and UI
  lifetime. Release objectives required exact CityHub counter updates; its final
  rerun passes. Debug passes all ten GPU checks with those final assertions.
- **52 changed/new C++ files** pass clang-format 23.1.3. All local Markdown links
  resolve; **10,923 reference files and six reference repositories** remain
  unchanged. The ledger hashes fifteen original source files.

Builds use the existing configured Debug/Release and Clang sanitizer directories.
Full native suites use `ctest --test-dir build/<configuration> -L headless -j4
--output-on-failure`. GPU runs explicitly set
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json` and report the
NVIDIA GeForce RTX 3060 Laptop GPU. The evidence ledger contains exact focused
commands, logs, hashes and development failures. The pre-existing ItemPickups
float/GameType comparison warning remains; no failing assertion was suppressed.

The private tests use the external Blood II installation. CityHub is loaded twice;
both authored pickups are validated and collected by explicit test contacts,
followed by P selection and actual view-model binding. This is a collection/service
probe, not a claim that a full CityHub playthrough passed. A separate SubwayA
diagnostic grant drives the real primary, secondary and overheld animation-key
sequences. Synthetic fixtures cover precise boundaries and unavailable resources.

The base-world pickup catalog finds **161 ProximitiesPU entries**: 133 create with
their model, and 28 reject the still-unsupported historical `TypeAction` schema.
Across all supported pickups this yields 1,423 creations, zero missing models,
206 explicit schema rejections and 60 supported destruction spawns in 57 readable
worlds. The existing `16_bridge.dat` reader limitation remains unchanged.

The initial regression correctly failed its old catalog totals after registration.
The revised assertions account for the exact new class, 161 entries and 28
rejections; no unsupported schema or failing content was silently accepted.

## Remaining gate

CityHub source accounting is **450 created / 31 transferred world sounds /
541 editor-only / three unsupported / one failed**, totaling 1,026. The remaining
unsupported entries are two `RemotesPU` and one `FlashlightPU`; `fallengrills`
still lacks a usable named world-model binding. Strict NEW_LEVEL preparation
therefore continues to refuse that destination. Other required services and the
unchanged [M2 exit criteria](../../history/Roadmap/03_milestones.md) remain open.

No complete campaign, retail physics equivalence, original inventory HUD,
multiplayer, moving-surface bomb attachment, full explosion illumination or native
Apple validation of this increment is claimed.
