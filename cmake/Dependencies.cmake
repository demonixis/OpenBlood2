include(FetchContent)

# SDL main and Dear ImGui master resolved on 2026-10-15. Updates resolve the
# current branch head again, then record the commit and archive checksum here.
set(BLOOD2_SDL_REVISION 6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1)
set(BLOOD2_IMGUI_REVISION c1aaac65e896a45f81f47a4d68f9959e0f86c859)
set(BLOOD2_SPDLOG_REVISION 6fa36017cfd5731d617e1a934f0e5ea9c4445b13) # v1.15.3
set(BLOOD2_ENET_REVISION 5a9c537fd464b3c6d3c55e1d3bd47588faf71b42) # Audited 2026-10-30.

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

# The game and ImGui use SDL_GPU directly, not SDL_Renderer.
# Keep GPU/OpenXR, platform audio/video (including KMSDRM's EGL dependency),
# controllers and dummy test drivers at their existing defaults.
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
# Unused capture, desktop integration and file-output audio services.
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
# The setup screen's folder/file pickers on desktop; mobile pickers return content URIs, not paths.
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
else()
    set(SDL_DIALOG ON CACHE BOOL "" FORCE)
endif()
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
if(APPLE)
    # This pinned SDL compiles Cocoa notifications unconditionally. Keep their
    # UserNotifications/Security framework dependencies enabled on Apple builds.
    set(SDL_NOTIFICATION ON CACHE BOOL "" FORCE)
else()
    set(SDL_NOTIFICATION OFF CACHE BOOL "" FORCE)
endif()
set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)

# Dedicated builds still use SDL's bounded WAV decoder and conversion routines, but
# have no video, GPU, controller or real audio-device backend.
if(NOT BLOOD2_BUILD_CLIENT)
    set(SDL_UNIX_CONSOLE_BUILD ON)
    foreach(feature VIDEO GPU OPENXR DIALOG NOTIFICATION JOYSTICK HAPTIC SENSOR CAMERA
            TRAY POWER HIDAPI ALSA PULSEAUDIO PIPEWIRE JACK SNDIO OSS WASAPI DSOUND COREAUDIO)
        set(SDL_${feature} OFF)
    endforeach()
    if(APPLE)
        # This SDL revision unconditionally builds notification/cocoa. Retain its
        # framework dependencies, as in client builds; the server initializes no UI.
        set(SDL_NOTIFICATION ON)
    endif()
endif()

set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/${BLOOD2_SDL_REVISION}
    URL_HASH SHA256=b59f24752e8ac3322853b43d6d773dfbce4d175d9e2c1db1c4305d8fe565b4e6
    DOWNLOAD_NAME SDL-${BLOOD2_SDL_REVISION}.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND "${CMAKE_COMMAND}" "-DSDL_SOURCE_DIR=<SOURCE_DIR>"
        -P "${CMAKE_CURRENT_LIST_DIR}/PatchSdlProperties.cmake"
        COMMAND "${CMAKE_COMMAND}" "-DSDL_SOURCE_DIR=<SOURCE_DIR>"
        -P "${CMAKE_CURRENT_LIST_DIR}/PatchSdlXrSwapchain.cmake"
    SYSTEM EXCLUDE_FROM_ALL)
FetchContent_Declare(spdlog
    URL https://codeload.github.com/gabime/spdlog/tar.gz/${BLOOD2_SPDLOG_REVISION}
    URL_HASH SHA256=5097fb362e79a2bd7247beaf1f8377ed60e274fbe83a4b33e7b73383f0279022
    DOWNLOAD_NAME spdlog-${BLOOD2_SPDLOG_REVISION}.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SDL3 spdlog)

# ENet is always static; keep the setting local to its fetch.
set(blood2_saved_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(enet
    URL https://codeload.github.com/lsalzman/enet/tar.gz/${BLOOD2_ENET_REVISION}
    URL_HASH SHA256=537d42f31b0c0d27f5a9635f6c224f2451c1c1484bfde50e94c884e96181c290
    DOWNLOAD_NAME enet-${BLOOD2_ENET_REVISION}.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(enet)
set(BUILD_SHARED_LIBS "${blood2_saved_shared_libs}")
target_include_directories(enet SYSTEM INTERFACE "${enet_SOURCE_DIR}/include")
if(BLOOD2_SANITIZERS AND NOT MSVC)
    target_compile_options(enet PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/licenses")
configure_file("${enet_SOURCE_DIR}/LICENSE" "${PROJECT_BINARY_DIR}/licenses/enet.txt" COPYONLY)

if(BLOOD2_BUILD_CLIENT)
    # The HUD uses the same pinned ImGui backend in inspector-free builds.
    FetchContent_Declare(imgui_source
        URL https://codeload.github.com/ocornut/imgui/tar.gz/${BLOOD2_IMGUI_REVISION}
        URL_HASH SHA256=b3d8f99838bb3ff05e14185562345f85e00c032da4d8fc277608d7898d847114
        DOWNLOAD_NAME imgui-${BLOOD2_IMGUI_REVISION}.tar.gz
        DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(imgui_source)
    add_library(imgui STATIC
        "${imgui_source_SOURCE_DIR}/imgui.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_source_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
        "${imgui_source_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp")
    target_include_directories(imgui SYSTEM PUBLIC
        "${imgui_source_SOURCE_DIR}" "${imgui_source_SOURCE_DIR}/backends")
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
    # The setup screen's proportional font, Droid Sans from the same pinned archive, embedded at
    # configure time so no asset path is needed (licenses/DroidSans-Apache-2.0.txt).
    file(READ "${imgui_source_SOURCE_DIR}/misc/fonts/DroidSans.ttf" blood2_ui_font HEX)
    string(REGEX REPLACE "(..)" "0x\\1," blood2_ui_font "${blood2_ui_font}")
    file(WRITE "${PROJECT_BINARY_DIR}/generated/ui/ui_font.h"
        "#pragma once\ninline const unsigned char ui_font[] = {${blood2_ui_font}};\n")
    target_include_directories(imgui PUBLIC "${PROJECT_BINARY_DIR}/generated/ui")
endif()

foreach(pair IN ITEMS "${sdl3_SOURCE_DIR}/LICENSE.txt|SDL.txt" "${spdlog_SOURCE_DIR}/LICENSE|spdlog.txt")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 source)
    list(GET parts 1 name)
    configure_file("${source}" "${PROJECT_BINARY_DIR}/licenses/${name}" COPYONLY)
endforeach()
