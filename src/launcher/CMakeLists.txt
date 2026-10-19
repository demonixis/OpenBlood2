find_package(Threads REQUIRED)
include("${PROJECT_SOURCE_DIR}/cmake/LauncherDependencies.cmake")
if(BLOOD2_SANITIZERS)
    blood2_project_options(miniz)
endif()
add_library(blood2_launcher_core STATIC state.cpp dialog.cpp content.cpp process.cpp controller.cpp demo_install.cpp mods.cpp artwork.cpp)
target_link_libraries(blood2_launcher_core PUBLIC blood2_startup SDL3::SDL3)
target_link_libraries(blood2_launcher_core PRIVATE Threads::Threads CURL::libcurl miniz OpenSSL::Crypto)
blood2_project_options(blood2_launcher_core)

add_executable(blood2_launcher main.cpp ../engine/capture.cpp)
# Reuse the proportional font already supplied by our pinned ImGui archive.
# Embed at configure time so launching from another directory needs no asset path.
file(READ "${imgui_source_SOURCE_DIR}/misc/fonts/DroidSans.ttf" launcher_font HEX)
string(REGEX REPLACE "(..)" "0x\\1," launcher_font "${launcher_font}")
file(WRITE "${PROJECT_BINARY_DIR}/generated/launcher_font.h"
    "#pragma once\ninline unsigned char launcher_font[] = {${launcher_font}};\n")
set_target_properties(blood2_launcher PROPERTIES OUTPUT_NAME blood2-launcher
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(blood2_launcher PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(blood2_launcher PRIVATE blood2_launcher_core imgui spdlog::spdlog)
blood2_project_options(blood2_launcher)

if(BUILD_TESTING)
    add_subdirectory("${PROJECT_SOURCE_DIR}/tests/launcher" "${PROJECT_BINARY_DIR}/tests/launcher")
endif()

# Keep redistribution notices beside the standalone executable.
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/licenses")
configure_file("${PROJECT_SOURCE_DIR}/licenses/launcher-notices.txt" "${PROJECT_BINARY_DIR}/licenses/launcher-notices.txt" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/licenses/DroidSans-Apache-2.0.txt" "${PROJECT_BINARY_DIR}/licenses/DroidSans-Apache-2.0.txt" COPYONLY)
foreach(pair IN ITEMS "${launcher_curl_SOURCE_DIR}/COPYING|curl.txt" "${launcher_miniz_SOURCE_DIR}/LICENSE|miniz.txt"
    "${imgui_source_SOURCE_DIR}/LICENSE.txt|imgui.txt" "${sdl3_SOURCE_DIR}/LICENSE.txt|SDL.txt" "${spdlog_SOURCE_DIR}/LICENSE|spdlog.txt")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 source)
    list(GET parts 1 name)
    configure_file("${source}" "${PROJECT_BINARY_DIR}/licenses/${name}" COPYONLY)
endforeach()

# The parent game build installs this component beside its own executable.
install(TARGETS blood2_launcher RUNTIME DESTINATION . COMPONENT Launcher)
install(DIRECTORY "${PROJECT_BINARY_DIR}/licenses" DESTINATION . COMPONENT Launcher)
