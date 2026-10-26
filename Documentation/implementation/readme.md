# Implementation records

[M2 closure](reports/m2_acceptance.md): the selected SubwayA → CityHub session is
accepted with open GPU reservation **M2-GPU-001**. M3 remains unaccepted.

| Record | Location |
| --- | --- |
| Demo_01 ventilation computer, door seams and failure diagnostics | [Report](reports/demo_computer_doors.md) / [door contract](../contracts/door_contracts.md#replacement-translation-solver) |
| Forward FX light selection per receiver | [Report](reports/forward_effect_lights.md) / [contract](../contracts/scene_effects_contracts.md#lights); 12 mobile / 32 desktop |
| Scene effect rendering (lot 10) | [Report](reports/scene_effects.md) / [contract](../contracts/scene_effects_contracts.md) / [source ledger](sources/scene_effect_sources.json) |
| Remaining destruction materials (lot 9) | [Report](reports/remaining_debris.md) / [contract](../contracts/destruction_debris_contracts.md#remaining-materials--lot-9-2026-10-25) |
| Energy/boss/rift VFX (lot 8) | [Report](reports/energy_effects.md) / [contract](../contracts/energy_effects_contracts.md) |
| Nightmares expansion (lot N1: edition and mount) | [Plan](nightmares_plan.md) / [edition contract](../contracts/nightmares_edition_contracts.md) |
| Original-code imports and adaptations | [Provenance](provenance.md) |
| Crypt / Bridge / Entrance / Lobby | [Contract](../contracts/later_levels_contracts.md) / [source ledger](sources/later_levels_sources.json) |
| Per-world gaps and implementation batches | [Level gaps](level_gaps.md) |
| Explosive weapon VFX (lot 7) | [Report](reports/explosive_effects.md) / [contract](../contracts/explosive_effects_contracts.md) / [source ledger](sources/explosive_effect_sources.json) |
| Tesla/Drudge VFX (lot 6) | [Report](reports/tesla_effects.md) / [contract](../contracts/tesla_contracts.md#original-vfx-completion--lot-6-2026-10-25) / [source ledger](sources/tesla_effect_sources.json) |
| Flare VFX (lot 5) | [Report](reports/flare_effects.md) / [contract](../contracts/flare_contracts.md#original-vfx-completion--lot-5-2026-10-25) / [source ledger](sources/flare_effect_sources.json) |
| Shared ballistic VFX (lot 4) | [Report](reports/ballistic_effects.md) / [contract](../contracts/ballistic_effects_contracts.md) / [source ledger](sources/ballistic_effect_sources.json) |
| Remaining original weapon/environment VFX (beta lot 3) | [Inventory](reports/vfx_inventory.md) / [source ledger](sources/vfx_inventory_sources.json); priorities live in the [roadmap](../roadmap.md#original-vfx-completion-beta-feedback-m37--m4). |
| Dreamcast port study (exploratory, no milestone) | [Study](dreamcast_study.md) |
| OpenXR plan (M8: Quest first, PCVR on Linux last) | [Plan](openxr_plan.md) / [first light](reports/xr_first_light.md) / [stereo world](reports/xr_stereo_world.md) / [controllers](reports/xr_controls.md) / [menus and HUD](reports/xr_menus_hud.md) / [laser and playtest](reports/xr_laser_playtest.md) / [contract](../contracts/xr_runtime_contracts.md) |
| Save/load audit (original classes, shells, LithTech engine, OpenBlood2 map) | [Audit](save_load_audit/readme.md) / [contract](../contracts/save_load_contracts.md) / [source ledger](sources/save_load_audit_sources.json) |
| Save/load lot 1 (save-message primitives) | [Report](reports/save_load_messages.md) / [source ledger](sources/save_load_messages_sources.json) |
| Save/load lot 2 (container and fingerprint) | [Report](reports/save_load_container.md) / [source ledger](sources/save_load_container_sources.json) |
| Save/load lot 3 (engine records and the clock) | [Report](reports/save_load_records.md) / [source ledger](sources/save_load_records_sources.json) |
| Save/load lot 4 (restore-mode world and candidate transaction) | [Report](reports/save_load_session.md) / [source ledger](sources/save_load_session_sources.json) |
| Save/load lot 5 (player and client) | [Report](reports/save_load_player.md) / [source ledger](sources/save_load_player_sources.json) |
| Save/load lot 8 (movers and brushes) | [Report](reports/save_load_movers.md) / [source ledger](sources/save_load_movers_sources.json) |
| Save/load lot 6 (slots, Quick and Restart, menus) | [Report](reports/save_load_slots.md) / [source ledger](sources/save_load_slots_sources.json) |
| Save/load lot 14 (whole-world certification) | [Report](reports/save_load_certification.md) / [source ledger](sources/save_load_certification_sources.json) |
| Save/load lot 7 (world logic) | [Report](reports/save_load_world_logic.md) / [source ledger](sources/save_load_world_logic_sources.json) |
| Save/load lot 10 (effect publishers and audio emitters) | [Report](reports/save_load_effects.md) / [source ledger](sources/save_load_effects_sources.json) |
| Save/load lot 13 (native state) | [Report](reports/save_load_native.md) / [source ledger](sources/save_load_native_sources.json) |
| Save/load lot 11 (AI core) | [Report](reports/save_load_ai.md) / [source ledger](sources/save_load_ai_sources.json) |
| Save/load lot 12 (bosses and special enemies) | [Report](reports/save_load_bosses.md) / [source ledger](sources/save_load_bosses_sources.json) |
| Save/load lot 9 (pickups and projectiles) | [Report](reports/save_load_pickups.md) / [source ledger](sources/save_load_pickups_sources.json) |
| Source selections, commits and hashes | [Source ledgers](sources/readme.md) |
| M0 / M1 validation | [M0](reports/m0.md) / [M1](reports/m1.md) |
| M2 criterion matrix and GPU reservation | [Acceptance](reports/m2_acceptance.md) |
| Menus / HUD / crosshair | [Menus](reports/original_menus.md) / [HUD](reports/original_hud.md) / [crosshair](reports/crosshair.md) |
| Demo / setup | [Demo](reports/demo_playable.md) / [setup screen](../contracts/setup_screen_contracts.md) ([archived launcher](../history/launcher/report_launcher_redesign.md)) |
| Increment reports | [reports/](reports) |
| Raw logs, measurements, patches and ledgers | [reports/evidence/](reports/evidence) |

Reports are dated snapshots. Old “M2 incomplete” statements retain their historical
scope; use the closure record for the current decision and [contracts](../contracts/readme.md)
plus code for current behavior. Preserve raw evidence bytes, failed results and
source hashes. Historical command paths describe their recorded checkout.
