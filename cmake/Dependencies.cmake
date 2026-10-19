include(FetchContent)

# SDL main and Dear ImGui master resolved on 2026-10-15. Updates resolve the
# current branch head again, then record the commit and archive checksum here.
set(BLOOD2_SDL_REVISION 6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1)
set(BLOOD2_IMGUI_REVISION c1aaac65e896a45f81f47a4d68f9959e0f86c859)
set(BLOOD2_SPDLOG_REVISION 6fa36017cfd5731d617e1a934f0e5ea9c4445b13) # v1.15.3

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
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_NOTIFICATION OFF CACHE BOOL "" FORCE)
set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)

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
    SYSTEM EXCLUDE_FROM_ALL)
FetchContent_Declare(spdlog
    URL https://codeload.github.com/gabime/spdlog/tar.gz/${BLOOD2_SPDLOG_REVISION}
    URL_HASH SHA256=5097fb362e79a2bd7247beaf1f8377ed60e274fbe83a4b33e7b73383f0279022
    DOWNLOAD_NAME spdlog-${BLOOD2_SPDLOG_REVISION}.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SDL3 spdlog)

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

