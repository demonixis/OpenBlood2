# Implementation records

[M2 closure](reports/m2_acceptance.md): the selected SubwayA → CityHub session is
accepted with open GPU reservation **M2-GPU-001**. M3 remains unaccepted.

| Record | Location |
| --- | --- |
| Original-code imports and adaptations | [Provenance](provenance.md) |
| Crypt / Bridge / Entrance / Lobby | [Contract](../contracts/later_levels_contracts.md) / [source ledger](sources/later_levels_sources.json) |
| Per-world gaps and implementation batches | [Level gaps](level_gaps.md) |
| Save/load audit (original classes, shells, LithTech engine, OpenBlood2 map) | [Audit](save_load_audit/readme.md) / [contract](../contracts/save_load_contracts.md) / [source ledger](sources/save_load_audit_sources.json) |
| Save/load lot 1 (save-message primitives) | [Report](reports/save_load_messages.md) / [source ledger](sources/save_load_messages_sources.json) |
| Save/load lot 2 (container and fingerprint) | [Report](reports/save_load_container.md) / [source ledger](sources/save_load_container_sources.json) |
| Save/load lot 3 (engine records and the clock) | [Report](reports/save_load_records.md) / [source ledger](sources/save_load_records_sources.json) |
| Save/load lot 4 (restore-mode world and candidate transaction) | [Report](reports/save_load_session.md) / [source ledger](sources/save_load_session_sources.json) |
| Save/load lot 5 (player and client) | [Report](reports/save_load_player.md) / [source ledger](sources/save_load_player_sources.json) |
| Save/load lot 8 (movers and brushes) | [Report](reports/save_load_movers.md) / [source ledger](sources/save_load_movers_sources.json) |
| Save/load lot 6 (slots, Quick and Restart, menus) | [Report](reports/save_load_slots.md) / [source ledger](sources/save_load_slots_sources.json) |
| Save/load lot 7 (world logic) | [Report](reports/save_load_world_logic.md) / [source ledger](sources/save_load_world_logic_sources.json) |
| Save/load lot 10 (effect publishers and audio emitters) | [Report](reports/save_load_effects.md) / [source ledger](sources/save_load_effects_sources.json) |
| Save/load lot 13 (native state) | [Report](reports/save_load_native.md) / [source ledger](sources/save_load_native_sources.json) |
| Save/load lot 11 (AI core) | [Report](reports/save_load_ai.md) / [source ledger](sources/save_load_ai_sources.json) |
| Source selections, commits and hashes | [Source ledgers](sources/readme.md) |
| M0 / M1 validation | [M0](reports/m0.md) / [M1](reports/m1.md) |
| M2 criterion matrix and GPU reservation | [Acceptance](reports/m2_acceptance.md) |
| Menus / HUD / crosshair | [Menus](reports/original_menus.md) / [HUD](reports/original_hud.md) / [crosshair](reports/crosshair.md) |
| Demo / launcher | [Demo](reports/demo_playable.md) / [launcher](reports/launcher_redesign.md) |
| Increment reports | [reports/](reports) |
| Raw logs, measurements, patches and ledgers | [reports/evidence/](reports/evidence) |

Reports are dated snapshots. Old “M2 incomplete” statements retain their historical
scope; use the closure record for the current decision and [contracts](../contracts/readme.md)
plus code for current behavior. Preserve raw evidence bytes, failed results and
source hashes. Historical command paths describe their recorded checkout.
