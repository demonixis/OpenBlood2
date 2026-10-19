add_library(blood2_launcher_core STATIC state.cpp dialog.cpp)
target_link_libraries(blood2_launcher_core PUBLIC blood2_startup SDL3::SDL3)
blood2_project_options(blood2_launcher_core)

add_executable(blood2_launcher main.cpp ../engine/capture.cpp)
set_target_properties(blood2_launcher PROPERTIES OUTPUT_NAME blood2-launcher
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(blood2_launcher PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(blood2_launcher PRIVATE blood2_launcher_core imgui spdlog::spdlog)
blood2_project_options(blood2_launcher)

if(BUILD_TESTING)
    add_subdirectory("${PROJECT_SOURCE_DIR}/tests/launcher" "${PROJECT_BINARY_DIR}/tests/launcher")
endif()
