# Service contracts

- [Original menu frontend](menu_contracts.md)

- [Original default crosshair](crosshair_contracts.md)

- [Original PCX player statistics HUD](original_hud_contracts.md)

The [selected M2 acceptance contract](m2_acceptance_contracts.md) records closure with reservation M2-GPU-001, the tested scope, negative controls and the still-open GPU platform qualification.

Use these references before changing a subsystem. They describe original rules,
replacement engine behavior, ownership and explicit limitations. Earlier
increment snapshots can be superseded by later linked evidence; an old omission
is not a current support matrix. Check the [implementation records](../implementation/readme.md)
and the actual code together.

For everyday setup and contribution guidance, return to the [developer index](../readme.md).

## Core services and resources

- [Original music asset inspection](music_asset_contracts.md)

- [Collision traversal and post-transition work](collision_traversal_contracts.md)
- [Compound DAT shell validation](compound_shell_contracts.md)
- [Transient object-message lifetime](message_lifetime_contracts.md)
- [M2 Model and Collision Services](model_and_collision_contracts.md)
- [M1 Object Boundary](object_contracts.md)
- [Owned object snapshots and callback membership](object_snapshot_contracts.md)
- [Bounded service coverage](service_coverage_contracts.md)
- [Session tracing and zero-depth audio work](session_cost_contracts.md)
- [Resource, World and Session Contracts](world_contracts.md)

## Player and session

- [Original single-player death and camera](player_death_contracts.md)

- [NPC activation visibility and the authored Gideon intro](activation_visibility_contracts.md)
- [Original CDestructable state preservation](damage_state_contracts.md)
- [Read-only destination preparation](destination_contracts.md)
- [Isolated destination initialization rehearsal](destination_trial_contracts.md)
- [Original level-exit contracts](exit_contracts.md)
- [Compact player HUD and numeric weapon selection](hud_contracts.md)
- [Original inventory Save/Load subset](inventory_state_contracts.md)
- [Safe NEW_LEVEL ownership transfer](level_change_contracts.md)
- [Player Locomotion Boundary](locomotion_contracts.md)
- [Original MoveMgr Boundary](movement_contracts.md)
- [Original Player Entry Boundary](player_contracts.md)
- [Keyboard fire and relative mouse controls](player_control_contracts.md)
- [Named keep-alive player entry](player_keepalive_contracts.md)
- [Player engine restoration and restored client association](player_restore_contracts.md)
- [Selected original player fields and restore initialization](player_state_contracts.md)
- [Play entry and first Cultist combat corrections](playtest_input_contracts.md)
- [Interactive world entry and NPC preservation](session_playback_contracts.md)

- [Stopping contacts and original falling damage](fall_damage_contracts.md)

## World objects and interaction

- [Rotating and destructible world-model brushes](brush_contracts.md)
- [Original scripted cameras](camera_contracts.md)
- [Container, liquid and ladder boundary](container_contracts.md)
- [Bound-container enumeration](container_index_contracts.md)
- [Original door audio](door_audio_contracts.md)
- [Sliding doors and translating world models](door_contracts.md)
- [Original brush explosions and debris requests](explosion_contracts.md)
- [Player USE and rotating doors](interaction_contracts.md)
- [Original keyframed paths](keyframer_contracts.md)
- [Mission objectives and client resource strings](objectives_contracts.md)
- [Original pushable brushes and bounded box physics](pushable_contracts.md)
- [Conveyor, Damage and FreeFall boundary](specialized_volume_contracts.md)
- [Original triggers, door SelfTrigger and keys](trigger_contracts.md)
- [Original world-model debris](world_debris_contracts.md)
- [Keyframed translating world models](worldpath_contracts.md)

## Artificial intelligence

- [CMovement parallel-axis alignment](ai_alignment_contracts.md)
- [Original NPC ammunition recovery](ai_ammo_contracts.md)
- [NPC animation under blocked clearance](ai_animation_clearance_contracts.md)
- [Original AI escape, hiding and guard states](ai_escape_contracts.md)
- [Original AI weapon choice and standing fire](ai_fire_contracts.md)
- [Original scripted locomotion and route following](ai_follow_contracts.md)
- [Original NPC hit localization and recoil](ai_hit_contracts.md)
- [Original autonomous locomotion and Cultist attacks](ai_locomotion_contracts.md)
- [Original AI animation keys](ai_model_key_contracts.md)
- [Original CMovement cache and obstacle paths](ai_navigation_contracts.md)
- [Original AI perception and candidate lists](ai_perception_contracts.md)
- [Original AI recovery and navigation states](ai_recovery_contracts.md)
- [Original Cultist/Civilian initialization and AI_Update](ai_runtime_contracts.md)
- [Original AI script preparation and state dispatch](ai_script_contracts.md)
- [Original scripted opportunity fire](ai_script_fire_contracts.md)
- [Original AI visibility queries](ai_sight_contracts.md)
- [Original player smell trail and AI search state](ai_smell_contracts.md)
- [Original AI state decisions and stationary actions](ai_state_contracts.md)
- [Original explicit AI targets and facing](ai_target_contracts.md)
- [Original AI trigger commands and hearing](ai_trigger_contracts.md)
- [Original Gideon attack, dodge and recoil sequencing](gideon_attack_contracts.md)
- [Original Gideon combat actions](gideon_combat_contracts.md)
- [Original Gideon initialization and boss-health delivery](gideon_runtime_contracts.md)
- [Original Gideon fade and teleport states](gideon_teleport_contracts.md)
- [Original AI path snapshots and selection](path_manager_contracts.md)

## Weapons, inventory and damage

- [Original M16 and grenade](assault_contracts.md)
- [Corpse clearance at doorway ends](corpse_clearance_contracts.md)
- [Ordinary original NPC death and CCorpse](corpse_contracts.md)
- [Initial ordinary corpse placement](corpse_placement_contracts.md)
- [Original burning deaths and dismemberment](death_effect_contracts.md)
- [Original energy projectile boundary](energy_projectile_contracts.md)
- [Original Flare Gun and projectile family](flare_contracts.md)
- [Original flashlight and bounded point illumination](flashlight_contracts.md)
- [Original Ingram Mac-10](mac10_contracts.md)
- [Original MedKit pickup and inventory action](medkit_contracts.md)
- [Original Naga eye weapon and red laser beam](naga_weapon_contracts.md)
- [Original pickup collection audio](pickup_audio_contracts.md)
- [Original pickups and destruction spawns](pickup_contracts.md)
- [Original proximity bombs](proximity_contracts.md)
- [Original remote bombs](remote_bomb_contracts.md)
- [Original Gideon shield and owner-bound spell particles](shield_weapon_contracts.md)
- [Original weapon fire audio and hearing](weapon_audio_contracts.md)
- [Original Inventory and Vector Weapon Boundary](weapon_contracts.md)
- [Original NPC weapon drops and Beretta pickups](weapon_drop_contracts.md)
- [Original vector impacts and material audio](weapon_impact_contracts.md)
- [Original weapon impact flash and smoke](weapon_sprite_contracts.md)
- [Original Gideon wind and one-shot shockwave boundary](wind_weapon_contracts.md)

## Audio and conversation

- [Original trigger audio](trigger_audio_contracts.md)
- [Original KeyFramer audio](keyframe_audio_contracts.md)

- [Disk-backed PCM audio and historical audio metadata](audio_stream_contracts.md)
- [Original character voices and player animation sounds](character_audio_contracts.md)
- [Original cinematic cues and bounded audio](cinematic_contracts.md)
- [Original conversation triggers and client communication](conversation_contracts.md)
- [Original player VoiceMgr and SOUND commands](player_voice_contracts.md)
- [Original random sound emitters](random_sound_contracts.md)
- [Original SoundFX controllers](sound_fx_contracts.md)
- [Original world-owned Sound loops](world_sound_contracts.md)

## Rendering, models and effects

- [Held models and attachments](attachment_contracts.md)
- [Original corpse blood pools and client splats](blood_splat_contracts.md)
- [Original blood spurts and debris impact contracts](blood_spurt_contracts.md)
- [Original built-in Model](builtin_model_contracts.md)
- [Original character animations and node queries](character_animation_contracts.md)
- [Original character motion keys](character_motion_contracts.md)
- [Character node visibility and selected player Load tail](character_node_contracts.md)
- [CityHub entry and missing named world models](cityhub_entry_contracts.md)
- [Original destructible models](destructable_model_contracts.md)
- [Original explosion flame contracts](explosion_flame_contracts.md)
- [Original explosion fragment contracts](explosion_fragment_contracts.md)
- [Original explosion point-light contracts](explosion_light_contracts.md)
- [Original explosion model contracts](explosion_model_contracts.md)
- [Original explosion particle-ring contracts](explosion_ring_contracts.md)
- [Original explosion sprite contracts](explosion_sprite_contracts.md)
- [Original dynamic light contracts](light_contracts.md)
- [Packaged SPIR-V and Metal shaders](metal_shader_contracts.md)
- [Presentation performance: single scene pass, batching, culling and mips](render_performance_contracts.md)
- [Baked world lighting and replacement model light](baked_lighting_contracts.md)
- [Original DAT lightmaps](lightmap_contracts.md)
- [Static light objects and model lighting](static_light_contracts.md)
- [FLAG_SHADOW model shadows](model_shadow_contracts.md)
- [Prepared ABC geometry and CPU sampling](model_preparation_contracts.md)
- [Animated model presentation](model_view_contracts.md)
- [Current budget continuation](object_particle_contracts.md)
- [Original authored particle streams](particle_stream_contracts.md)
- [Original PolyGrid surfaces](polygrid_contracts.md)
- [Original scene explosion emitters](scene_explosion_contracts.md)
- [Original player screen fades](screen_fade_contracts.md)
- [Original sky objects and world-model presentation](sky_contracts.md)
- [Original weapon impact sparks](spark_contracts.md)
- [Original DetailSprite and SPR resources](sprite_contracts.md)
- [Sprite presentation contracts](sprite_view_contracts.md)
- [Authored world surface panning](surface_effect_contracts.md)
- [DTX textures and opaque world materials](texture_contracts.md)
- [Transparent world surfaces](world_alpha_contracts.md)
- [Diagnostic 3D world view](world_view_contracts.md)

- [Original combat voice callers](combat_voice_contracts.md)

- [Original jump voices and landing sounds](movement_audio_contracts.md).

- [DLS instrument data](music_dls_contracts.md)

- [DLS diagnostic voices](music_sampler_contracts.md): bounded synthesis, graph selection, shared output, lifetime and explicit unsupported music.

- [IMA pattern-note data](music_note_contracts.md): original record order, signed fields, opaque events and inspected world ownership.
- [IMA bands and setup](music_band_contracts.md): source channel records, packed patches, original controller order and explicit sequencing limits.
- [IMA section records and authored timing](music_section_contracts.md): header timing, command/mute/chord data, original-reader checks and explicit sequencing limits.

- [IMA style meter and section timeline](music_timeline_contracts.md): binding, signed times, mute sampling and bounded immutable ownership.

- [IMA pattern channels and variations](music_variation_contracts.md): raw/effective fields, original decision rules and explicit random inputs.

- [IMA note event preparation](music_event_contracts.md): signed variation, transpose, paired messages and explicit unresolved pitch context.

- [IMA harmonic note conversion](music_pitch_contracts.md): effective-context mode/scale/chord arithmetic, bounded failure and diagnostic event preparation.

- [IMA prepared harmony](music_harmony_contracts.md): bounded personality metadata, original chord flags and explicit live-selection limits.

- [IMA grid cursor](music_cursor_contracts.md): original stateful section progression, explicit draws and world-owned diagnostic lifetime.

- [IMA active note lifetime](music_active_note_contracts.md): original chord deadlines, serial exemption and ordered due/retained lists; live MUSIC remains unavailable.

- [IMA style curves](music_curve_contracts.md): original authored curve/point records, strict bounds and independent world-owned inspection.

- [IMA style curve events](music_curve_event_contracts.md): original point expansion with explicit ticks and raw messages; no controller playback.

- [IMA ordered grid dispatch](music_grid_dispatch_contracts.md): curve filters and source-order merged note/curve messages, then due stops; no playback.

- [DLS live voice controls](music_live_control_contracts.md): current-voice controller mutation, original raw bend arithmetic and checked lifetime.

- [Immediate MIDI receiver](music_midi_receiver_contracts.md): checked channel controls, held notes and original repeat/stop selection; timed transport remains unavailable.

- [Timed MIDI delivery](music_timing_contracts.md): sample-boundary dispatch, original queue order, bank capture and checked world lifetime.

- [IMA runtime clock](music_clock_contracts.md): original tempo boundaries and tick-to-sample queue conversion.

- [Finite IMA section playback](music_program_contracts.md): complete authored audition, terminal-note deadlines, explicit preparation policies and audio lifetime.

- [Finite music playlists](music_playlist_contracts.md): original level choices, ordered one-pass playback and shared tempo/lifetime limits.

- [Repeating music transport](music_transport_contracts.md): bounded continuous audio, fractional clocks, cancellation and remaining live-MUSIC limits.

- [Checked session startup](session_start_contracts.md): original entry, strict resource/service checks, failure ownership and the finite CLI harness.

- [Platform lifetime diagnostics](gpu_lifetime_contracts.md): isolated SDL controls, explicit sanitizer failures and NVIDIA module attribution.

- [Original demo edition](demo_contracts.md): `--demo` archive layout, first level, runtime `_DEMO` weapon/pickup rules and the remaining demo blockers.
