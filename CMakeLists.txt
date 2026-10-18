cmake_minimum_required(VERSION 3.28)
project(Blood2 VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BLOOD2_INSPECTOR "Build the in-game inspection UI" ON)
option(BLOOD2_GPU_TESTS "Register tests that open a real GPU window" OFF)
option(BLOOD2_SANITIZERS "Enable ASan/UBSan on project code (Clang/GCC)" OFF)
set(BLOOD2_TEST_DATA_DIR "" CACHE PATH "Optional private Blood II installation for tests")

include(CTest)
include(cmake/Dependencies.cmake)
execute_process(COMMAND git describe --always --dirty
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE BLOOD2_BUILD_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT BLOOD2_BUILD_REVISION)
    set(BLOOD2_BUILD_REVISION "unknown")
endif()
configure_file(cmake/build_info.h.in generated/build_info.h @ONLY)

add_library(engine STATIC src/engine/world_lights.cpp src/engine/world_shade.cpp src/engine/gpu_registry.cpp src/engine/model_renderer.cpp src/engine/hud_surface.cpp src/engine/polygrid_view.cpp src/engine/sky.cpp src/engine/particles.cpp src/engine/sprite_view.cpp src/engine/model_view.cpp src/engine/texture.cpp src/engine/world_view.cpp src/engine/world_renderer.cpp src/engine/world_shaders.cpp src/engine/attachments.cpp src/engine/object_pose.cpp src/engine/config.cpp src/engine/runtime.cpp src/engine/capture.cpp src/engine/objects.cpp src/engine/resources.cpp src/engine/world.cpp)
target_sources(engine PRIVATE src/engine/sprite.cpp src/engine/audio.cpp src/engine/audio_stream.cpp src/engine/music_asset.cpp src/engine/ima_style.cpp src/engine/ima_curve.cpp src/engine/ima_curve_event.cpp src/engine/ima_variation.cpp src/engine/ima_note_event.cpp src/engine/ima_note_pitch.cpp src/engine/ima_harmony.cpp src/engine/ima_selection.cpp src/engine/ima_cursor.cpp src/engine/ima_band.cpp src/engine/ima_section.cpp src/engine/ima_clock.cpp src/engine/ima_program.cpp src/engine/ima_playlist.cpp src/engine/ima_transport.cpp src/engine/ima_timeline.cpp src/engine/dls_bank.cpp src/engine/dls_sampler.cpp src/engine/dls_midi_queue.cpp src/engine/string_resources.cpp src/engine/model.cpp src/engine/model_store.cpp)
target_sources(engine PRIVATE src/engine/collision.cpp src/engine/container.cpp src/engine/world_model.cpp src/engine/logging.cpp src/engine/crash_capture.cpp)
set_source_files_properties(src/engine/crash_capture.cpp PROPERTIES
    COMPILE_DEFINITIONS "BLOOD2_CRASH_SANITIZED=$<BOOL:${BLOOD2_SANITIZERS}>")
target_include_directories(engine PUBLIC src src/game/legacy PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(engine PUBLIC SDL3::SDL3 PRIVATE spdlog::spdlog)

add_library(blood2_game STATIC src/game/polygrids.cpp src/game/original/ScreenFade.cpp src/game/original/PolyGrid.cpp src/game/original/PolyGridFX.cpp src/game/sparks.cpp src/game/original/SparksFX.cpp src/game/blood_spurts.cpp src/game/original/ParticleExplosionFX.cpp src/game/blood_splats.cpp src/game/original/BloodSplatFX.cpp src/game/original/ClientSplatFX.cpp src/game/original/RemoteBomb.cpp src/game/original/PlayerRemoteBombs.cpp src/game/original/BombExplosion.cpp src/game/original/ProximityBomb.cpp src/game/server_sky.cpp src/game/original/SkyObjects.cpp src/game/particle_streams.cpp src/game/original/ParticleStreamFX.cpp src/game/original/ClientParticleStreamSFX.cpp src/game/original/Grenade.cpp src/game/original/FlareProjectile.cpp src/game/original/GideonShield.cpp src/game/object_particles.cpp src/game/original/Shockwave.cpp src/game/original/WeaponProjectile.cpp src/game/original/Projectile.cpp src/game/original/EnergyBlast.cpp src/game/server_projectiles.cpp src/game/original/NagaEyes.cpp src/game/original/LaserBeamFX.cpp src/game/laser_beams.cpp src/game/weapon_sprites.cpp src/game/original/SmokeImpactFX.cpp src/game/destination_trial.cpp src/game/level_change.cpp src/game/world_preparation.cpp src/game/explosion_flames.cpp src/game/original/ExplosionFlameFX.cpp src/game/original/ObjectParticleFX.cpp src/game/explosion_fragments.cpp src/game/original/ExplosionFragFX.cpp src/game/explosion_rings.cpp src/game/original/ExplosionRingFX.cpp src/game/explosion_lights.cpp src/game/original/ExplosionLightFX.cpp src/game/explosion_models.cpp src/game/original/ExplosionModelFX.cpp src/game/explosion_sprites.cpp src/game/original/ExplosionSpriteFX.cpp src/game/server_attachments.cpp src/game/original/HandWeaponModel.cpp src/game/original/WeaponHand.cpp
    src/game/original/engineobjects.cpp
    src/game/original/B2BaseClass.cpp
    src/game/original/CameraSpot.cpp
    src/game/original/Key.cpp
    src/game/original/GameStartPoint.cpp
    src/game/original/PathPoint.cpp
    src/game/original/PathMgr.cpp
    src/game/original/trigger_helpers.cpp
    src/game/original/session_load.cpp
    src/game/session.cpp
    src/game/world_validation.cpp
    src/game/player_restore.cpp
    src/game/registration.cpp
    src/game/server_adapter.cpp
    src/game/service_coverage.cpp)
target_sources(blood2_game PRIVATE src/game/original/ClientCrosshair.cpp src/game/original/NewStatusBar.cpp src/game/original/ClientExplosionSFX.cpp src/game/server_sprites.cpp src/game/original/DetailSprite.cpp src/game/server_audio.cpp src/game/original/CinematicTrigger.cpp src/game/original/RandomSoundFX.cpp src/game/original/Sound.cpp src/game/original/SoundFX.cpp)
target_sources(blood2_game PRIVATE src/game/original/PlayerLanding.cpp src/game/original/PlayerDeath.cpp src/game/original/PlayerDeathCamera.cpp)
target_sources(blood2_game PRIVATE src/game/server_lights.cpp src/game/original/LightFX.cpp src/game/original/ClientLightFX.cpp)
target_sources(blood2_game PRIVATE src/game/server_keyframes.cpp src/game/original/KeyData.cpp src/game/original/KeyFramer.cpp)
target_sources(blood2_game PRIVATE src/game/server_cameras.cpp src/game/original/CameraObj.cpp src/game/server_objectives.cpp src/game/original/ObjectivesTrigger.cpp src/game/original/ConversationTrigger.cpp src/game/original/ConversationStrings.cpp src/game/original/CommLink.cpp src/game/original/VoiceMgr.cpp src/game/original/PlayerVoice.cpp src/game/server_models.cpp src/game/original/WorldProperties.cpp src/game/original/SmellHint.cpp)
target_sources(blood2_game PRIVATE src/game/original/WeaponPickups.cpp src/game/server_pickups.cpp src/game/original/PickupObject.cpp
    src/game/original/ItemPickups.cpp src/game/original/GameInvItems.cpp src/game/original/Flashlight.cpp src/game/original/AmmoPickups.cpp src/game/original/PowerupPickups.cpp src/game/original/spawn_object.cpp)
target_sources(blood2_game PRIVATE src/game/server_physics.cpp src/game/server_movers.cpp src/game/server_rotation.cpp src/game/original/RotatingDoor.cpp src/game/original/PlayerUse.cpp src/game/original/Door.cpp src/game/original/Trigger.cpp src/game/original/KeyPickup.cpp src/game/server_keys.cpp src/game/server_containers.cpp src/game/original/VolumeBrush.cpp src/game/original/PlayerContainers.cpp)
target_sources(blood2_game PRIVATE src/game/original/MoveMgr.cpp src/game/original/PlayerLocomotion.cpp src/game/original/Anim_Sound.cpp src/game/original/CharacterSound.cpp src/game/original/AI_Mgr.cpp src/game/original/AISight.cpp src/game/original/AIFire.cpp src/game/original/AIPerception.cpp
    src/game/server_activation.cpp src/game/original/AIRuntime.cpp
    src/game/original/AIAmmo.cpp src/game/original/AISmell.cpp src/game/original/AIEscape.cpp src/game/original/AINavigationStates.cpp src/game/original/AILocomotion.cpp src/game/original/CultistAttack.cpp src/game/original/AIDodge.cpp src/game/original/AIScript.cpp src/game/original/AIScriptMovement.cpp src/game/original/AIScriptFire.cpp src/game/original/Movement.cpp
    src/game/original/AIState.cpp src/game/original/AIHitReaction.cpp src/game/original/AIDeath.cpp src/game/original/Gib.cpp src/game/flesh_gibs.cpp src/game/original/Corpse.cpp src/game/original/AITrigger.cpp src/game/original/AISound.cpp
    src/game/original/CivilianAI.cpp
    src/game/original/CultistAI.cpp src/game/original/Gideon.cpp src/game/original/ExitHint.cpp src/game/original/AITarget.cpp src/game/original/CharacterMotion.cpp src/game/original/Sparam.cpp)
target_sources(blood2_game PRIVATE src/game/server_exits.cpp src/game/original/ExitTrigger.cpp src/game/server_clients.cpp src/game/original/PlayerObj.cpp src/game/original/PlayerSave.cpp src/game/original/CharacterNodes.cpp src/game/original/BloodShells.cpp)
target_sources(blood2_game PRIVATE src/game/server_weapons.cpp src/game/original/WeaponDefs.cpp
    src/game/original/InventoryMgr.cpp src/game/original/InventorySave.cpp src/game/original/Weapon.cpp src/game/original/WeaponSound.cpp src/game/original/WeaponVector.cpp src/game/original/WeaponFX.cpp src/game/original/ViewWeapon.cpp)
target_sources(blood2_game PRIVATE src/game/original/Music.cpp src/game/music_catalog.cpp)
target_sources(blood2_game PRIVATE src/game/original/MainMenus.cpp src/game/menu_canvas.cpp src/game/menu_audio.cpp
    src/game/menu_preferences.cpp src/game/new_game.cpp)
target_link_libraries(blood2_game PUBLIC engine)

target_sources(blood2_game PRIVATE src/game/original/Destructable.cpp src/game/original/DestructableSave.cpp src/game/original/DestructableBrush.cpp src/game/original/DestructableModel.cpp
    src/game/original/Rotating.cpp src/game/original/RotatingBrush.cpp src/game/original/WorldModelDebris.cpp
    src/game/original/Debris.cpp src/game/original/radius_damage.cpp src/game/server_effects.cpp src/game/server_world_bodies.cpp)
add_executable(blood2_app src/app/model_lighting.cpp src/app/scene_presenter.cpp src/app/bench.cpp src/app/polygrid_scene.cpp src/app/hud.cpp src/app/ui_runtime.cpp src/app/path_manager_smoke.cpp src/app/ai_model_key_smoke.cpp src/app/character_motion_smoke.cpp src/app/main.cpp src/app/model_scene.cpp src/app/sprite_scene.cpp src/app/trigger_smoke.cpp src/app/interaction_smoke.cpp src/app/brush_smoke.cpp src/app/pickup_smoke.cpp src/app/objectives_smoke.cpp src/app/camera_smoke.cpp src/app/keyframe_smoke.cpp src/app/light_smoke.cpp src/app/cinematic_smoke.cpp src/app/random_sound_smoke.cpp src/app/world_sound_smoke.cpp src/app/sprite_smoke.cpp src/app/scene_explosion_smoke.cpp)
target_sources(blood2_app PRIVATE src/app/menu_renderer.cpp src/app/frontend.cpp)
set_target_properties(blood2_app PROPERTIES OUTPUT_NAME blood2)
target_include_directories(blood2_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(blood2_app PRIVATE blood2_game spdlog::spdlog imgui)
target_compile_definitions(blood2_app PRIVATE BLOOD2_INSPECTOR=$<BOOL:${BLOOD2_INSPECTOR}>)
if(BLOOD2_INSPECTOR)
    target_sources(blood2_app PRIVATE src/app/inspector_paths.cpp src/app/inspector_skeleton.cpp src/app/inspector.cpp src/app/inspector_cameras.cpp src/app/inspector_keyframes.cpp src/app/inspector_lights.cpp src/app/inspector_cinematics.cpp src/app/inspector_random_sounds.cpp src/app/inspector_scene_explosions.cpp src/app/inspector_sprites.cpp src/app/inspector_world_sounds.cpp src/app/inspector_objectives.cpp src/app/inspector_pickups.cpp)
endif()

function(blood2_project_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:/Zi>")
        target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        # Retain source locations for post-mortem diagnosis without changing Release optimization.
        target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:-g1>")
        if(BLOOD2_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()
blood2_project_options(engine)
blood2_project_options(blood2_game)
blood2_project_options(blood2_app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

