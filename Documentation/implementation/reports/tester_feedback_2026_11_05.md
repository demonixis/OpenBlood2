# Tester feedback, 2026-11-05 (0.5.2)

GitHub issues #5, #8 (its "Issues" list only), #9, #10 and #11, plus an adjustable field of
view, on top of `446933cc`. The #8 "Feedback" list (run lock semantics, remote/time bomb
prompts, screenshots, extra cheats, subtitles, bobbing, hitscan switch, reaction scaling,
chasing) and its gameplay items (civilian behaviour, gibbing rules, enemy line of sight) are
gameplay requests, deliberately left out of this bug-fix release. No commercial assets or
reference-source dependencies are added; M2's reservation and milestone acceptance are
unchanged.

## Issue #9: CDM crash, "Smoke-trail particle capacity exceeded" (fixed)

A grenade fragment's smoke trail moving far in one update asked for more samples than the
2048-particle trail or 8192-particle shared pool could hold, which failed the session step
62 s into 05_CDM. Smoke-trail, shell and cylinder emissions now keep the leading samples and
count the rest (`dropped`, inspected per trail); only non-finite or negative sample counts
fail ([object particle contracts](../../contracts/object_particle_contracts.md)).

## Issue #11: Cas Revenant (fixed)

- **Sky.** Sky world models drew both faces in index order with depth disabled, so far sides
  covered near ones. They now use back-face culling like later LithTech, with sky sprites kept
  two-sided. `SkyFog` and the world `FarZ` are now applied
  ([sky contracts](../../contracts/sky_contracts.md#skyfog-farz-and-culling-052)).
- **Barred doors.** Airship2's `enddoor10/11` leaves interleave, so each started inside the
  other and the pair stayed shut. Two GOTHRUWORLD world models no longer block each other
  ([door contracts](../../contracts/door_contracts.md)); `airship.bars` covers it.
- **Exploding wall and crates.** Exploding CDebris only recorded its explosion on the server,
  so nothing was drawn or heard; it now publishes the original `AddExplosion` effect. Wood
  crate fragments were invisible because older mask-less skins decode with alpha 0 throughout;
  those skins now load opaque ([destruction debris](../../contracts/destruction_debris_contracts.md#tester-corrections-052)).
  Fragments launched from inside the exploding wall hit it without a usable normal; they now
  keep their motion (counted as embedded) instead of failing the session.

## Issue #10: glass (fixed)

The original chroma-keys transparent faces whose texture has no alpha mask: pure black is
see-through. Without it the Museum skylights and display cases drew `black.dtx` as opaque
black (z-fighting with frames behind it). Those faces now bind a keyed copy of their material
([world alpha contracts](../../contracts/world_alpha_contracts.md)).

## Issue #8: issues list

| Report | Result |
|---|---|
| Air bar, objectives page, pickup messages | See the HUD section below. |
| Mac-10 alt-fire sound | Fixed: view-weapon `SOUND_KEY`s played only for the shotgun; all weapons now play them ([weapon contracts](../../contracts/weapon_contracts.md)). |
| Speaker/fan explosions (E1L2), floating ship explosion sound (E1L3) | Fixed by the exploding-debris change above. |
| Floating trash bag (E1L2 start) | Matches the original: the second bag's ground sweep lands on the first bag's box; its round mesh overhangs. Unchanged. |
| E1L3 window transparency | Fixed by the chroma key (#10). |
| Sky | Fixed (#11). |
| Conveyor drag, jumping out of water at the sawed-off display | Fixed: 1/120 s substeps made both original per-frame rules ineffective; a fixed 1/30 s container cadence and a one-frame dry jump restore them ([movement contracts](../../contracts/movement_contracts.md#container-cadence-and-dry-jumps-052)). |
| Flares under water | Original behaviour: no projectile or effect checks liquid; water only slows them. Unchanged. |
| Underwater palette | Fixed: the original `UpdateContainerFX` water/blood/acid/FreeFall light scale follows the camera's container ([container contracts](../../contracts/container_contracts.md)). |
| Rocking horse sound (park) | The park object is the `seesaw` RotatingDoor and already played its clank; the original plays two overlapping clanks per endpoint, which is now matched ([door audio](../../contracts/door_audio_contracts.md)). |
| Rooftop glass shards | No divergence from the original glass debris recipe was found. Unchanged. |
| Lasers (E1L3) | Fixed: the emitters sit inside the walls and the replacement ray reported the wall's inner face first ([TripLaser contracts](../../contracts/trip_laser_contracts.md#embedded-emitters-052)). |
| Gabriella missing in the cut-scene | Fixed: the spawn allowlist refused GabriellaAI ([spawner contracts](../../contracts/spawner_contracts.md)). |
| Mad Scientist cowering | Open. His script matches the original; the pose depends on whether the cut-scene camera activates before his first update (object update order), which cannot be verified for LithTech 1.0. |
| Civilians, gibbing rules, enemy line of sight | Gameplay requests, out of scope. |

## Issue #5: weapon sidebar

Already in 0.5.1: F1 (without Shift) shows and hides the original weapon tabs, sliding in
from the right in high detail.

## Field of view

Options > Display ends with `FIELD OF VIEW [60–120]` (preferences version 12); see the
[menu contracts](../../contracts/menu_contracts.md#field-of-view-052).

## Verification

Headless and private-data suites pass on macOS (Apple M5). GPU tests ran on Metal: all pass
except `gpu.character`, `gpu.viewport`, `gpu.polygrid` and `gpu.inventory_use`, which fail
identically on the unmodified `446933cc` build and are not caused by this release.
