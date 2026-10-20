# M2 — original remote bombs

Date: 2026-10-17. **M2 remains incomplete.**

This increment follows [proximity bombs](m2_proximity.md), connecting both authored
CityHub `RemotesPU` entries to the original inventory weapon, charged throwing,
`CRemoteBomb` and the player's remote charge list. Unsupported CityHub entries fall
from three to one; the missing world-model binding remains an explicit failure.

Implementation commit: `370888772d481a1ea66db7f030e0c4bb0abac1bc` in the independent `Blood2/` repository.
See [contracts](../../contracts/remote_bomb_contracts.md),
[source hashes](../sources/m2_remote_sources.json),
[validation ledger](evidence/m2-remote.json) and the
[implementation patch](evidence/m2-remote-implementation.patch).
No commercial resource, reference repository or pinned dependency was modified.

## Observable behavior

- Collect `RemotesPU`, then press **R** to select remote bombs. Each adds one
  charge, capped at ten. Weapon50 uses reserved slot11 and ammo11; ordinary
  selection and AI weapon search still use slots0..9. Slot10 remains proximity.
- Hold primary fire to charge, then release. The real view model's `end_fire`
  FIRE_KEY throws with the original 250–1750 units/s curve. Unlike proximity
  bombs, remote bombs can remain held indefinitely: the original overhold
  explosion branch is commented out. Secondary drops a charge with zero initial
  velocity. Exhausting ammo switches to the next ordinary weapon.
- Press **B** to signal every charge still registered to the player, regardless
  of selected weapon or remaining ammo. Secondary fire is still a drop. The
  separate command latches explosion; damage happens on the next scheduled update.
  Camera/death/movement restrictions, text suppression and focus resets apply.
  R/B are application bindings, not a recovered retail configuration.
- First contact with a character, including a dead one, zeros velocity once and
  retains gravity/spin. Other solid contact aligns the bomb with the surface,
  clears gravity and sticks it. There is no fuse, automatic proximity detection,
  arming delay or sticking sound. Original small explosion and `time/explode.wav`
  use existing checked FX/audio and radius damage services.

The Weapons inspector exposes the registered list size, command/signal counts,
errors, selected ammo/charge and a detonation button queued at the frame boundary.
Objects exposes hidden/live resources, Armed/Falling, contacts and spin. Terminal
reports retain the last actual state after removal. Rendering reads existing model
clocks and never advances gameplay.

## Ownership, restoration and adaptation

Original player list order is newest-first. A bomb is registered only after
successful configuration and carries checked player-to-bomb and bomb-to-shooter
links. External bomb removal updates the living owner's list. Bomb explosion
removes itself before damage callbacks; callbacks and destruction stay deferred.

The original player destructor and Respawn remove all still-listed charges.
Already signalled charges have left that list and can finish after player removal,
with null shooter attribution. Restore/KeepAlive retain slot11, raw ammo and
selection, but never carry old projectile identities. World projectile save/load
and whole-world checkpoints remain unavailable.

A new lifetime regression exposed an engine-boundary mismatch: this engine also
notifies a pending owner about its outgoing links before its destructor. Processing
that notification as live-player unlink emptied the list prematurely and left
charges alive. The player now processes remote unlink only while alive, retaining
the list for original destruction. Reentrant damage, leave, Respawn and restore
checks cover the correction; global link-notification semantics are unchanged.

The existing grenade/proximity swept-box solver also accepts CRemoteBomb, retaining
one integration, bounded steps and deferred contacts. This is replacement physics,
not verified retail collision or moving-surface attachment. Typed commands replace
legacy packets, including bounded finite throw speed. A shared explosion publisher
factors identical proximity/remote dispatch without changing proximity rules.
The separate inventory menu node remains represented by the owned weapon/pool;
full original HUD/PCX behavior is not claimed.

## Validation

- GCC Debug and inspector-free Release: **774 headless tests each**, including
  **127 private-data cases**. All 21 focused remote/proximity/catalog checks also
  pass after the lifetime and typed-item fixes.
- Clang ASan/UBSan with leak detection: **59 tests**, including nine private-data
  cases. These cover deferred/reentrant removal, player teardown, inventory
  restoration and KeepAlive; no sanitizer defect was reported.
- NVIDIA Vulkan: **11 GPU checks per configuration**, including ten private-data
  cases. Both charge view/world models render in two load/unload cycles; readback
  measures 12,464 view pixels and 8,976 world-model pixels per cycle. Hidden/removal
  cleanup and unchanged simulation clocks are asserted. Captures were inspected
  visually and remain ignored local artifacts.
- Regressions include proximity bombs, M16 grenades, mouse-driven combat, corpse
  passage, safe NEW_LEVEL refusal, objectives, pickups, glass, sky and UI lifetime.
- All **52 changed/new C++ files** pass clang-format 23.1.3. Local Markdown link
  targets resolve. **10,923 reference files and six reference repositories** are
  unchanged; fourteen original source hashes are retained in the source ledger.

Builds use the existing configured Debug/Release and Clang sanitizer directories.
Native suites run with `ctest --test-dir build/<configuration> -L headless -j3
--output-on-failure`. GPU runs explicitly select
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json`; the actual runtime
reports NVIDIA GeForce RTX 3060 Laptop GPU. The ledger records the exact commands,
logs, hashes and development failures. The pre-existing ItemPickups float/GameType
comparison warning remains. No native Apple execution of this increment is claimed.

Ten new headless cases cover source rules, contacts, real swept floor collision,
owner links/removal, damage-delay order, reentrant damage, collection/full-pool
refusal, input commands, restore and private resources. The private CityHub test
loads twice and collects both authored pickups through explicit contact probes.
A separately labelled SubwayA grant drives real hold/release/drop animation keys,
including a 2.4-second remote hold with no automatic shot. This is service evidence,
not a complete CityHub playthrough.

The base-world catalog contains **107 RemotesPU entries**: **93 create** with their
models and **14 reject** the still-unsupported historical TypeAction schema.
All supported pickup classes total **1,516 creations**, zero missing models,
**220 explicit schema rejections** and 60 supported destruction spawns across
57 readable worlds. The existing `16_bridge.dat` reader limitation is unchanged.

Initial targeted tests also exposed an Item request limit still capped at type7;
the limit now includes type8 while explicitly rejecting other unsupported IDs.
Catalog/accounting assertions record the exact registered-class delta; no
unsupported schema, missing resource or failed destination is accepted silently.

## Remaining gate

CityHub is **452 created / 31 transferred world sounds / 541 editor-only /
one unsupported / one failed**, totaling 1,026. `FlashlightPU` remains unsupported,
and `fallengrills` still lacks its required named world-model geometry. Strict
NEW_LEVEL preparation continues to refuse that destination and retain the source.
These and the unchanged [M2 acceptance criteria](../../history/Roadmap/03_milestones.md)
remain open. Pickup sound output, original item HUD, full explosion illumination,
time bombs, multiplayer and native Apple automation are not claimed.
