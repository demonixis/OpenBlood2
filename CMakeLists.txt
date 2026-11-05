cmake_minimum_required(VERSION 3.28)
# FetchContent populates in-process, without a sub-build. With the Xcode generator
# the sub-build is a nested xcodebuild: when Product > Archive regenerates the
# project, it inherits the archive environment and re-extracts sources being compiled.
if(POLICY CMP0168)
    cmake_policy(SET CMP0168 NEW)
endif()
project(Blood2 VERSION 0.5.2 LANGUAGES C CXX)
option(BLOOD2_BUILD_CLIENT "Build the graphical game client" ON)
option(BLOOD2_BUILD_SERVER "Build the dedicated server" OFF)
if(NOT BLOOD2_BUILD_CLIENT AND NOT BLOOD2_BUILD_SERVER)
    message(FATAL_ERROR "Enable at least one of BLOOD2_BUILD_CLIENT/BLOOD2_BUILD_SERVER")
endif()
if(APPLE AND BLOOD2_BUILD_CLIENT)
    enable_language(OBJCXX) # The setup screen's NSURLSession download.
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


option(BLOOD2_INSPECTOR "Build the in-game inspection UI" ON)
option(BLOOD2_XR "Build OpenXR support (SDL_GPU sessions; the Quest preset enables it)" OFF)
option(BLOOD2_GPU_TESTS "Register tests that open a real GPU window" OFF)
option(BLOOD2_SANITIZERS "Enable ASan/UBSan on project code (Clang/GCC)" OFF)
set(BLOOD2_TEST_DATA_DIR "" CACHE PATH "Optional private Blood II installation for tests")
set(BLOOD2_DEMO_DATA_DIR "" CACHE PATH "Optional private 1998 Blood II demo directory (B2Demo.rez) for tests")

include(cmake/ProjectOptions.cmake)
include(cmake/Packaging.cmake)
include(CTest)
include(cmake/Dependencies.cmake)
execute_process(COMMAND git describe --always --dirty
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE BLOOD2_BUILD_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT BLOOD2_BUILD_REVISION)
    set(BLOOD2_BUILD_REVISION "unknown")
endif()
configure_file(cmake/build_info.h.in generated/build_info.h @ONLY)
# Enhanced-renderer material classes, compiled into the binary (see config/materials.json).
file(READ "${PROJECT_SOURCE_DIR}/config/materials.json" BLOOD2_SURFACE_MANIFEST)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/config/materials.json")
configure_file(cmake/surface_manifest.h.in generated/surface_manifest.h @ONLY)

add_library(blood2_startup STATIC src/engine/config.cpp src/engine/resources.cpp src/engine/sha256.cpp)
target_include_directories(blood2_startup PUBLIC src)
blood2_project_options(blood2_startup)

add_library(blood2_net STATIC src/net/address.cpp src/net/transport.cpp src/net/content.cpp src/net/admission.cpp src/net/world_snapshot.cpp src/net/world_patch.cpp src/net/world_state.cpp src/net/collision_codec.cpp src/net/movement.cpp src/net/player_events.cpp src/net/player_commands.cpp)
target_include_directories(blood2_net PUBLIC src)
target_link_libraries(blood2_net PUBLIC blood2_startup PRIVATE enet)
blood2_project_options(blood2_net)

include(cmake/EngineSources.cmake)

add_library(blood2_client_movement STATIC src/game/original/MoveMgr.cpp src/client/replica_movement.cpp)
target_link_libraries(blood2_client_movement PUBLIC engine_core)
blood2_project_options(blood2_client_movement)

add_library(blood2_client_network STATIC src/client/network_weapons.cpp src/client/network_client.cpp src/client/network_world.cpp src/client/network_state.cpp src/client/network_collision.cpp src/client/network_movement.cpp src/client/network_player_events.cpp src/client/network_camera.cpp)
target_link_libraries(blood2_client_network PUBLIC engine_core blood2_net blood2_client_movement blood2_client_weapons)
blood2_project_options(blood2_client_network)

add_library(blood2_client_weapons STATIC src/game/original/ViewWeapon.cpp)
target_link_libraries(blood2_client_weapons PUBLIC engine_core)
blood2_project_options(blood2_client_weapons)

set_source_files_properties(src/engine/crash_capture.cpp PROPERTIES
    COMPILE_DEFINITIONS "BLOOD2_CRASH_SANITIZED=$<BOOL:${BLOOD2_SANITIZERS}>")
if(BLOOD2_XR AND BLOOD2_BUILD_CLIENT)
    # SDL_GPU owns the OpenXR instance; the pinned SDL vendors the OpenXR 1.1 headers.
    target_sources(engine PRIVATE src/engine/xr/xr_runtime.cpp)
    target_include_directories(engine SYSTEM PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
endif()

# Original cosmetic runtimes link independently of the authoritative game classes.
add_library(blood2_client_effects STATIC
    src/client/flashlight.cpp
    src/client/effect_services.cpp
    src/client/original_effects.cpp
    src/game/polygrids.cpp
    src/game/particle_streams.cpp
    src/game/object_particles.cpp
    src/game/explosion_fragments.cpp
    src/game/explosion_sprites.cpp
    src/game/explosion_models.cpp
    src/game/explosion_lights.cpp
    src/game/explosion_rings.cpp
    src/game/explosion_flames.cpp
    src/game/sparks.cpp
    src/game/lightning.cpp
    src/game/warp_gates.cpp
    src/game/blood_spurts.cpp
    src/game/blood_splats.cpp
    src/game/flesh_gibs.cpp
    src/game/shotgun_effects.cpp
    src/game/weapon_sprites.cpp
    src/game/laser_beams.cpp
    src/game/original/PolyGridFX.cpp
    src/game/original/ParticleStreamFX.cpp
    src/game/original/ObjectParticleFX.cpp
    src/game/original/ExplosionFragFX.cpp
    src/game/original/ExplosionSpriteFX.cpp
    src/game/original/ExplosionModelFX.cpp
    src/game/original/ExplosionLightFX.cpp
    src/game/original/ExplosionRingFX.cpp
    src/game/original/ExplosionFlameFX.cpp
    src/game/original/SparksFX.cpp
    src/game/original/LightningFX.cpp
    src/game/original/WarpGateFX.cpp
    src/game/original/ParticleExplosionFX.cpp
    src/game/original/BloodSplatFX.cpp
    src/game/original/SmokeImpactFX.cpp
    src/game/original/LaserBeamFX.cpp
    src/game/original/LightFX.cpp
)
target_link_libraries(blood2_client_effects PUBLIC engine_core)
blood2_project_options(blood2_client_effects)

add_library(blood2_game STATIC src/game/original/ScreenFade.cpp src/game/original/Credits.cpp src/game/original/PolyGrid.cpp src/game/original/ClientSplatFX.cpp src/game/original/RemoteBomb.cpp src/game/original/PlayerRemoteBombs.cpp src/game/original/BombExplosion.cpp src/game/original/ProximityBomb.cpp src/game/server_sky.cpp src/game/original/SkyObjects.cpp src/game/original/ClientParticleStreamSFX.cpp src/game/original/Grenade.cpp src/game/original/FlareProjectile.cpp src/game/original/GideonShield.cpp src/game/original/Shockwave.cpp src/game/original/WeaponProjectile.cpp src/game/original/Projectile.cpp src/game/original/EnergyBlast.cpp src/game/server_projectiles.cpp src/game/original/NagaEyes.cpp src/game/destination_trial.cpp src/game/level_change.cpp src/game/world_preparation.cpp src/game/server_attachments.cpp src/game/original/HandWeaponModel.cpp src/game/original/WeaponHand.cpp
    src/game/original/engineobjects.cpp
    src/game/original/B2BaseClass.cpp
    src/game/original/CameraSpot.cpp
    src/game/original/Key.cpp
    src/game/original/GameStartPoint.cpp
    src/game/original/PathPoint.cpp
    src/game/original/PathMgr.cpp
    src/game/original/trigger_helpers.cpp
    src/game/original/session_load.cpp
    src/game/server_effect_queries.cpp
    src/game/client_replication.cpp
    src/game/local_scene.cpp src/game/local_scene_model.cpp src/game/local_scene_sprite.cpp
    src/game/local_scene_polygrid.cpp src/game/local_scene_lighting.cpp
    src/game/local_movement.cpp src/game/local_presentation.cpp src/game/local_audio.cpp src/game/local_effects.cpp
    src/game/session.cpp
    src/game/world_validation.cpp
    src/game/player_restore.cpp
    src/game/registration.cpp
    src/game/saved_game.cpp
    src/game/server_adapter.cpp
    src/game/server_messages.cpp
    src/game/server_saves.cpp
    src/game/service_coverage.cpp)
target_sources(blood2_game PRIVATE src/game/original/NewStatusBar.cpp src/game/original/MessageMgr.cpp src/game/original/ClientExplosionSFX.cpp src/game/server_sprites.cpp src/game/original/DetailSprite.cpp src/game/server_audio.cpp src/game/original/CinematicTrigger.cpp src/game/original/RandomSoundFX.cpp src/game/original/Sound.cpp src/game/original/SoundFX.cpp)
target_sources(blood2_game PRIVATE src/game/original/PlayerLanding.cpp src/game/original/PlayerDeath.cpp)
target_sources(blood2_game PRIVATE src/game/server_lights.cpp src/game/original/ClientLightFX.cpp)
target_sources(blood2_game PRIVATE src/game/server_keyframes.cpp src/game/original/KeyData.cpp src/game/original/KeyFramer.cpp)
target_sources(blood2_game PRIVATE src/game/server_cameras.cpp src/game/original/CameraObj.cpp src/game/server_objectives.cpp src/game/original/ObjectivesTrigger.cpp src/game/original/ConversationTrigger.cpp src/game/original/ConversationStrings.cpp src/game/original/CommLink.cpp src/game/original/VoiceMgr.cpp src/game/original/PlayerVoice.cpp src/game/server_models.cpp src/game/original/WorldProperties.cpp src/game/original/SmellHint.cpp)
target_sources(blood2_game PRIVATE src/game/original/WeaponPickups.cpp src/game/server_pickups.cpp src/game/original/PickupObject.cpp
    src/game/original/ItemPickups.cpp src/game/original/GameInvItems.cpp src/game/original/Flashlight.cpp src/game/original/AmmoPickups.cpp src/game/original/PowerupPickups.cpp src/game/original/spawn_object.cpp
    src/game/original/Spawner.cpp src/game/original/TripLaser.cpp src/game/original/SeeingEye.cpp src/game/original/TheEyeInventory.cpp src/game/original/BugSprayProjectile.cpp src/game/original/InventoryClient.cpp src/game/original/NapalmProjectile.cpp src/game/original/BoneLeech.cpp src/game/original/DrudgeLord.cpp src/game/original/SoulDrudge.cpp src/game/original/ShikariAI.cpp src/game/original/GremlinAI.cpp src/game/original/AddonProjectiles.cpp src/game/original/MadScientistAI.cpp src/game/original/Tchernotronic.cpp src/game/original/SingularityProjectile.cpp src/game/original/LeechProjectile.cpp src/game/original/OrbProjectile.cpp src/game/original/Fanatic.cpp src/game/original/Prophet.cpp src/game/original/ChosenAI.cpp src/game/original/IshmaelAI.cpp
    src/game/original/ParticleSystem.cpp src/game/original/LaterEnemies.cpp src/game/original/Naga.cpp src/game/original/ZealotAI.cpp src/game/original/CampaignWeapons.cpp src/game/original/CampaignProjectiles.cpp src/game/original/CabalAttack.cpp src/game/original/HowitzerShell.cpp src/game/original/TeslaProjectile.cpp)
target_sources(blood2_game PRIVATE src/game/server_physics.cpp src/game/server_movers.cpp src/game/server_rotation.cpp src/game/original/RotatingDoor.cpp src/game/original/PlayerUse.cpp src/game/original/Door.cpp src/game/original/Trigger.cpp src/game/original/KeyPickup.cpp src/game/server_keys.cpp src/game/server_containers.cpp src/game/original/VolumeBrush.cpp src/game/original/Minefield.cpp src/game/original/SparksObj.cpp src/game/original/ClientLightningSFX.cpp src/game/original/ClientWarpGateSFX.cpp src/game/original/PlayerContainers.cpp src/game/original/PlayerPowers.cpp src/game/original/PlayerCheats.cpp)
target_sources(blood2_game PRIVATE src/game/original/PlayerLocomotion.cpp src/game/original/Anim_Sound.cpp src/game/original/CharacterSound.cpp src/game/original/AI_Mgr.cpp src/game/original/AISight.cpp src/game/original/AIFire.cpp src/game/original/AIPerception.cpp
    src/game/server_activation.cpp src/game/original/AIRuntime.cpp
    src/game/original/AIAmmo.cpp src/game/original/AISmell.cpp src/game/original/AIEscape.cpp src/game/original/AINavigationStates.cpp src/game/original/AILocomotion.cpp src/game/original/CultistAttack.cpp src/game/original/AIDodge.cpp src/game/original/AIScript.cpp src/game/original/AIScriptMovement.cpp src/game/original/AIScriptFire.cpp src/game/original/AISave.cpp src/game/original/BossSave.cpp src/game/original/Movement.cpp
    src/game/original/AIState.cpp src/game/original/AIHitReaction.cpp src/game/original/AIDeath.cpp src/game/original/Gib.cpp src/game/original/Corpse.cpp src/game/original/AITrigger.cpp src/game/original/AISound.cpp
    src/game/original/CivilianAI.cpp
    src/game/original/CultistAI.cpp src/game/original/Gideon.cpp src/game/original/EnhancedGideon.cpp src/game/original/UndeadGideon.cpp src/game/original/AncientOne.cpp src/game/original/AncientWeapons.cpp src/game/original/FlagObjects.cpp src/game/original/PlayerFlags.cpp src/game/original/ExitHint.cpp src/game/original/AITarget.cpp src/game/original/CharacterMotion.cpp src/game/original/Sparam.cpp)
target_sources(blood2_game PRIVATE src/game/server_exits.cpp src/game/original/ExitTrigger.cpp src/game/server_clients.cpp src/game/original/PlayerImprison.cpp src/game/original/PlayerObj.cpp src/game/original/PlayerSave.cpp src/game/original/CharacterNodes.cpp src/game/original/BloodShells.cpp)
target_sources(blood2_game PRIVATE src/game/server_weapons.cpp
    src/game/original/InventoryMgr.cpp src/game/original/InventoryItems.cpp src/game/original/InventorySave.cpp src/game/original/Weapon.cpp src/game/original/Voodoo.cpp src/game/original/WeaponSound.cpp src/game/original/WeaponVector.cpp src/game/original/WeaponFX.cpp src/game/local_weapon.cpp)
target_sources(blood2_game PRIVATE src/game/original/Music.cpp src/game/music_catalog.cpp)
target_sources(blood2_game PRIVATE src/game/save_slots.cpp)
target_include_directories(blood2_game PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_sources(blood2_game PRIVATE src/game/menu_resources.cpp)
if(BLOOD2_BUILD_CLIENT)
    target_sources(blood2_game PRIVATE src/game/local_scene_presenter.cpp)
    add_library(blood2_frontend STATIC src/game/original/MainMenus.cpp src/game/menu_canvas.cpp src/game/menu_audio.cpp
        src/game/original/LoadingScreen.cpp
        src/game/menu_preferences.cpp src/game/new_game.cpp)
    target_link_libraries(blood2_frontend PUBLIC blood2_game)
    blood2_project_options(blood2_frontend)
endif()
target_link_libraries(blood2_game PUBLIC blood2_client_effects blood2_client_movement engine_core)

target_link_libraries(blood2_game PUBLIC blood2_client_weapons)
target_sources(blood2_game PRIVATE src/game/original/Destructable.cpp src/game/original/DestructableSave.cpp src/game/original/DestructableBrush.cpp src/game/original/DestructableModel.cpp
    src/game/original/Rotating.cpp src/game/original/RotatingBrush.cpp src/game/original/RotatingModel.cpp
    src/game/original/WorldModelDebris.cpp
    src/game/original/Debris.cpp src/game/original/radius_damage.cpp src/game/server_effects.cpp src/game/server_world_bodies.cpp)
target_sources(blood2_game PRIVATE src/game/original/ProjectileSave.cpp src/game/original/PickupSave.cpp)
if(BLOOD2_BUILD_CLIENT)
    blood2_add_application(blood2_app src/app/model_lighting.cpp src/app/scene_presenter.cpp src/app/bench.cpp src/app/polygrid_scene.cpp src/app/hud.cpp src/app/ui_runtime.cpp src/app/path_manager_smoke.cpp src/app/ai_model_key_smoke.cpp src/app/character_motion_smoke.cpp src/app/main.cpp src/app/model_scene.cpp src/app/sprite_scene.cpp src/app/trigger_smoke.cpp src/app/interaction_smoke.cpp src/app/brush_smoke.cpp src/app/pickup_smoke.cpp src/app/objectives_smoke.cpp src/app/camera_smoke.cpp src/app/keyframe_smoke.cpp src/app/light_smoke.cpp src/app/cinematic_smoke.cpp src/app/random_sound_smoke.cpp src/app/world_sound_smoke.cpp src/app/sprite_smoke.cpp src/app/scene_explosion_smoke.cpp)
    target_sources(blood2_app PRIVATE src/app/menu_renderer.cpp src/app/frontend.cpp src/app/display_options.cpp src/app/network_frontend.cpp)
    # The setup screen: installations, mods and the demo download through the platform HTTPS stack.
    include(cmake/ContentDependencies.cmake)
    target_sources(blood2_app PRIVATE src/app/game_setup.cpp src/app/demo_install.cpp src/app/setup_screen.cpp)
    target_link_libraries(blood2_app PRIVATE miniz)
    if(APPLE)
        target_sources(blood2_app PRIVATE src/app/https_download_apple.mm)
        set_source_files_properties(src/app/https_download_apple.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        target_link_libraries(blood2_app PRIVATE "-framework Foundation")
    elseif(ANDROID)
        target_sources(blood2_app PRIVATE src/app/https_download_android.cpp)
    else()
        target_sources(blood2_app PRIVATE src/app/https_download_curl.cpp)
        target_link_libraries(blood2_app PRIVATE CURL::libcurl)
    endif()
    set_target_properties(blood2_app PROPERTIES OUTPUT_NAME blood2)
    target_include_directories(blood2_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(blood2_app PRIVATE blood2_frontend engine spdlog::spdlog imgui blood2_client_network blood2_server_runtime)
    target_compile_definitions(blood2_app PRIVATE BLOOD2_INSPECTOR=$<BOOL:${BLOOD2_INSPECTOR}>)
    target_compile_definitions(blood2_app PRIVATE BLOOD2_XR=$<BOOL:${BLOOD2_XR}>)
    if(BLOOD2_XR)
        target_sources(blood2_app PRIVATE src/app/xr_scene.cpp src/app/xr_player.cpp
                                          src/app/xr_frontend.cpp)
    endif()
    if(BLOOD2_INSPECTOR)
        target_sources(blood2_app PRIVATE src/app/inspector_paths.cpp src/app/inspector_skeleton.cpp src/app/inspector.cpp src/app/inspector_cameras.cpp src/app/inspector_keyframes.cpp src/app/inspector_lights.cpp src/app/inspector_cinematics.cpp src/app/inspector_random_sounds.cpp src/app/inspector_scene_explosions.cpp src/app/inspector_sprites.cpp src/app/inspector_world_sounds.cpp src/app/inspector_objectives.cpp src/app/inspector_pickups.cpp src/app/inspector_materials.cpp)
    endif()


    blood2_project_options(blood2_app)
    blood2_package_application(blood2_app)
endif()
blood2_project_options(blood2_game)

add_library(blood2_server_runtime STATIC src/server/session.cpp src/server/network.cpp src/server/player_movement.cpp src/server/player_events.cpp src/server/player_camera.cpp src/server/player_weapons.cpp src/server/player_commands.cpp)
target_link_libraries(blood2_server_runtime PUBLIC blood2_game blood2_net)
blood2_project_options(blood2_server_runtime)
if(BLOOD2_BUILD_SERVER)
    add_executable(blood2_server src/server/main.cpp)
    set_target_properties(blood2_server PROPERTIES OUTPUT_NAME blood2-server)
    target_link_libraries(blood2_server PRIVATE blood2_server_runtime spdlog::spdlog)
    blood2_project_options(blood2_server)
endif()

if(BUILD_TESTING)
    if(BLOOD2_BUILD_CLIENT)
        add_subdirectory(tests)
    else()
        add_subdirectory(tests/network)
    endif()
endif()

target_sources(blood2_game PRIVATE src/game/presentation_effects.cpp)
