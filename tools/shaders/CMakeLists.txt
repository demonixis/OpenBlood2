cmake_minimum_required(VERSION 3.28)
project(Blood2ShaderTools LANGUAGES C CXX)
# Optional offline toolchain, never linked into the game. Point SDL3_DIR at the
# configured game's _deps/sdl3-build directory (build SDL3-static there first).
find_package(SDL3 3.1.3 REQUIRED CONFIG)
include(FetchContent)
get_filename_component(BLOOD2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# SDL_shadercross main resolved 2026-10-17; SPIRV-Cross is its exact submodule.
set(BLOOD2_SHADERCROSS_REVISION 1ff05bec573988a98ef9e0260b4da44f512b8367)
set(BLOOD2_SPIRVCROSS_REVISION 1a6169566c73d3da552748fc372fe2bbb856e46e)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
# Keep dependency export sets for shadercross's static-library export, even
# though this project never installs either dependency.
set(SPIRV_CROSS_SKIP_INSTALL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spirvcross
    URL https://codeload.github.com/KhronosGroup/SPIRV-Cross/tar.gz/${BLOOD2_SPIRVCROSS_REVISION}
    URL_HASH SHA256=0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e
    DOWNLOAD_NAME SPIRV-Cross-${BLOOD2_SPIRVCROSS_REVISION}.tar.gz
    DOWNLOAD_DIR "${BLOOD2_ROOT}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(spirvcross)
# Upstream's non-vendored find guard uses this spelling, while its link target
# uses spirv-cross-c. Both refer to the same pinned build; no system fallback.
add_library(spirv_cross_c ALIAS spirv-cross-c)
set(SDLSHADERCROSS_DXC OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_STATIC ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI_STATIC ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(shadercross
    URL https://codeload.github.com/libsdl-org/SDL_shadercross/tar.gz/${BLOOD2_SHADERCROSS_REVISION}
    URL_HASH SHA256=60aae35ff71bd1d5f85b363e9d077a86bb27dd36333c42685e56c2fd541174d2
    DOWNLOAD_NAME SDL_shadercross-${BLOOD2_SHADERCROSS_REVISION}.tar.gz
    DOWNLOAD_DIR "${BLOOD2_ROOT}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(shadercross)
add_custom_target(regenerate
    COMMAND "${CMAKE_COMMAND}" "-DSHADERCROSS=$<TARGET_FILE:shadercross>"
        -P "${BLOOD2_ROOT}/cmake/GenerateWorldShaders.cmake"
    DEPENDS shadercross VERBATIM)
