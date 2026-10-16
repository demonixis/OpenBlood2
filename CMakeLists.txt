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
configure_file(cmake/build_info.h.in generated/build_info.h @ONLY)

add_library(engine STATIC src/engine/config.cpp src/engine/runtime.cpp src/engine/capture.cpp src/engine/objects.cpp src/engine/resources.cpp src/engine/world.cpp)
target_sources(engine PRIVATE src/engine/sprite.cpp src/engine/audio.cpp src/engine/string_resources.cpp src/engine/model.cpp src/engine/model_store.cpp)
target_sources(engine PRIVATE src/engine/collision.cpp src/engine/container.cpp src/engine/world_model.cpp)
target_include_directories(engine PUBLIC src src/game/legacy PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(engine PUBLIC SDL3::SDL3 PRIVATE spdlog::spdlog)

add_library(blood2_game STATIC
    src/game/original/engineobjects.cpp
    src/game/original/B2BaseClass.cpp
    src/game/original/CameraSpot.cpp
    src/game/original/Key.cpp
    src/game/original/GameStartPoint.cpp
    src/game/original/PathPoint.cpp
    src/game/original/trigger_helpers.cpp
    src/game/original/session_load.cpp
    src/game/session.cpp
    src/game/registration.cpp
    src/game/server_adapter.cpp)
target_sources(blood2_game PRIVATE src/game/original/ClientExplosionSFX.cpp src/game/server_sprites.cpp src/game/original/DetailSprite.cpp src/game/server_audio.cpp src/game/original/CinematicTrigger.cpp src/game/original/RandomSoundFX.cpp src/game/original/Sound.cpp)
target_sources(blood2_game PRIVATE src/game/server_lights.cpp src/game/original/LightFX.cpp src/game/original/ClientLightFX.cpp)
target_sources(blood2_game PRIVATE src/game/server_keyframes.cpp src/game/original/KeyData.cpp src/game/original/KeyFramer.cpp)
target_sources(blood2_game PRIVATE src/game/server_cameras.cpp src/game/original/CameraObj.cpp src/game/server_objectives.cpp src/game/original/ObjectivesTrigger.cpp src/game/server_models.cpp src/game/original/WorldProperties.cpp src/game/original/SmellHint.cpp)
target_sources(blood2_game PRIVATE src/game/server_pickups.cpp src/game/original/PickupObject.cpp
    src/game/original/AmmoPickups.cpp src/game/original/PowerupPickups.cpp src/game/original/spawn_object.cpp)
target_sources(blood2_game PRIVATE src/game/server_physics.cpp src/game/server_movers.cpp src/game/server_rotation.cpp src/game/original/RotatingDoor.cpp src/game/original/PlayerUse.cpp src/game/original/Door.cpp src/game/original/Trigger.cpp src/game/original/KeyPickup.cpp src/game/server_keys.cpp src/game/server_containers.cpp src/game/original/VolumeBrush.cpp src/game/original/PlayerContainers.cpp)
target_sources(blood2_game PRIVATE src/game/original/MoveMgr.cpp src/game/original/PlayerLocomotion.cpp src/game/original/Anim_Sound.cpp src/game/original/CharacterSound.cpp src/game/original/CharacterMotion.cpp src/game/original/Sparam.cpp)
target_sources(blood2_game PRIVATE src/game/server_clients.cpp src/game/original/PlayerObj.cpp src/game/original/BloodShells.cpp)
target_sources(blood2_game PRIVATE src/game/server_weapons.cpp src/game/original/WeaponDefs.cpp
    src/game/original/InventoryMgr.cpp src/game/original/Weapon.cpp src/game/original/ViewWeapon.cpp)
target_link_libraries(blood2_game PUBLIC engine)

target_sources(blood2_game PRIVATE src/game/original/Destructable.cpp src/game/original/DestructableBrush.cpp
    src/game/original/Rotating.cpp src/game/original/RotatingBrush.cpp
    src/game/original/Debris.cpp src/game/original/radius_damage.cpp src/game/server_effects.cpp src/game/server_world_bodies.cpp)
add_executable(blood2_app src/app/character_motion_smoke.cpp src/app/main.cpp src/app/trigger_smoke.cpp src/app/interaction_smoke.cpp src/app/brush_smoke.cpp src/app/pickup_smoke.cpp src/app/objectives_smoke.cpp src/app/camera_smoke.cpp src/app/keyframe_smoke.cpp src/app/light_smoke.cpp src/app/cinematic_smoke.cpp src/app/random_sound_smoke.cpp src/app/world_sound_smoke.cpp src/app/sprite_smoke.cpp src/app/scene_explosion_smoke.cpp)
set_target_properties(blood2_app PROPERTIES OUTPUT_NAME blood2)
target_include_directories(blood2_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(blood2_app PRIVATE blood2_game spdlog::spdlog)
target_compile_definitions(blood2_app PRIVATE BLOOD2_INSPECTOR=$<BOOL:${BLOOD2_INSPECTOR}>)
if(BLOOD2_INSPECTOR)
    target_sources(blood2_app PRIVATE src/app/inspector_skeleton.cpp src/app/inspector.cpp src/app/inspector_cameras.cpp src/app/inspector_keyframes.cpp src/app/inspector_lights.cpp src/app/inspector_cinematics.cpp src/app/inspector_random_sounds.cpp src/app/inspector_scene_explosions.cpp src/app/inspector_sprites.cpp src/app/inspector_world_sounds.cpp src/app/inspector_objectives.cpp src/app/inspector_pickups.cpp)
    target_link_libraries(blood2_app PRIVATE imgui)
endif()

function(blood2_project_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
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

