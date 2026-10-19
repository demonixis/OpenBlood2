add_executable(launcher_tests launcher_tests.cpp)
target_link_libraries(launcher_tests PRIVATE blood2_launcher_core)
blood2_project_options(launcher_tests)
foreach(case IN ITEMS state dialogs)
    add_test(NAME launcher.${case} COMMAND launcher_tests ${case} "${CMAKE_CURRENT_BINARY_DIR}/${case}")
    set_tests_properties(launcher.${case} PROPERTIES LABELS "headless;launcher" TIMEOUT 20)
endforeach()
add_test(NAME launcher.help COMMAND blood2_launcher --help)
set_tests_properties(launcher.help PROPERTIES LABELS "headless;launcher")

add_executable(launcher_game_fixture game_fixture.cpp)
target_link_libraries(launcher_game_fixture PRIVATE blood2_startup SDL3::SDL3)
blood2_project_options(launcher_game_fixture)
add_executable(launcher_process_tests process_tests.cpp)
target_link_libraries(launcher_process_tests PRIVATE blood2_launcher_core)
blood2_project_options(launcher_process_tests)
foreach(case IN ITEMS protocol probe content process detach)
    add_test(NAME launcher.${case} COMMAND launcher_process_tests ${case}
        "${CMAKE_CURRENT_BINARY_DIR}/${case}" "$<TARGET_FILE:launcher_game_fixture>")
    set_tests_properties(launcher.${case} PROPERTIES LABELS "headless;launcher" TIMEOUT 30)
endforeach()
set(BLOOD2_LAUNCHER_TEST_GAME "" CACHE FILEPATH "Optional separately built game for launcher integration checks")
if(BLOOD2_LAUNCHER_TEST_GAME)
    add_test(NAME launcher.game_info COMMAND "${CMAKE_COMMAND}" "-DGAME=${BLOOD2_LAUNCHER_TEST_GAME}"
        "-DROOT=${CMAKE_CURRENT_BINARY_DIR}/game-info" -P "${CMAKE_CURRENT_SOURCE_DIR}/info_test.cmake")
    set_tests_properties(launcher.game_info PROPERTIES LABELS "headless;launcher" TIMEOUT 20)
    foreach(edition IN ITEMS retail demo)
        if(edition STREQUAL "demo")
            set(data "${BLOOD2_DEMO_DATA_DIR}")
        else()
            set(data "${BLOOD2_TEST_DATA_DIR}")
        endif()
        if(data)
            add_test(NAME launcher.private_${edition} COMMAND launcher_process_tests private
                "${CMAKE_CURRENT_BINARY_DIR}/private-${edition}" "${BLOOD2_LAUNCHER_TEST_GAME}" "${data}" ${edition})
            set_tests_properties(launcher.private_${edition} PROPERTIES LABELS "headless;private;launcher" TIMEOUT 30)
        endif()
    endforeach()
endif()
if(BLOOD2_GPU_TESTS)
    add_test(NAME launcher.ui_fixture COMMAND launcher_process_tests prepare_ui
        "${CMAKE_CURRENT_BINARY_DIR}/ui-process" "$<TARGET_FILE:launcher_game_fixture>")
    set_tests_properties(launcher.ui_fixture PROPERTIES FIXTURES_SETUP launcher_ui_data LABELS "headless;launcher")
    add_test(NAME gpu.launcher_process COMMAND blood2_launcher --smoke --launch-smoke --frames 30
        --state-dir "${CMAKE_CURRENT_BINARY_DIR}/ui-process/state"
        --data-dir "${CMAKE_CURRENT_BINARY_DIR}/ui-process/data"
        --game "$<TARGET_FILE:launcher_game_fixture>"
        --capture "${CMAKE_CURRENT_BINARY_DIR}/ui-process/finished.png")
    set_tests_properties(gpu.launcher_process PROPERTIES FIXTURES_REQUIRED launcher_ui_data
        LABELS "gpu;launcher" TIMEOUT 30 RUN_SERIAL TRUE FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
    foreach(scale IN ITEMS 1 1.5 2)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gpu-${scale}")
        add_test(NAME gpu.launcher_${scale} COMMAND blood2_launcher --smoke --frames 16 --ui-scale ${scale}
            --state-dir "${CMAKE_CURRENT_BINARY_DIR}/gpu-${scale}"
            --capture "${CMAKE_CURRENT_BINARY_DIR}/gpu-${scale}/launcher.png")
        set_tests_properties(gpu.launcher_${scale} PROPERTIES LABELS "gpu;launcher" TIMEOUT 30 RUN_SERIAL TRUE
            FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
    endforeach()
endif()

add_executable(launcher_delivery_tests delivery_tests.cpp)
target_link_libraries(launcher_delivery_tests PRIVATE blood2_launcher_core miniz)
blood2_project_options(launcher_delivery_tests)
foreach(case IN ITEMS installer mods artwork)
    add_test(NAME launcher.${case} COMMAND launcher_delivery_tests ${case}
        "${CMAKE_CURRENT_BINARY_DIR}/delivery-${case}" "$<TARGET_FILE:launcher_game_fixture>")
    set_tests_properties(launcher.${case} PROPERTIES LABELS "headless;launcher" TIMEOUT 30)
endforeach()
if(BLOOD2_GPU_TESTS)
    foreach(panel IN ITEMS settings mods)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/panel-${panel}")
        add_test(NAME gpu.launcher_${panel} COMMAND blood2_launcher --smoke --frames 16 --panel ${panel}
            --state-dir "${CMAKE_CURRENT_BINARY_DIR}/panel-${panel}"
            --capture "${CMAKE_CURRENT_BINARY_DIR}/panel-${panel}/launcher.png")
        set_tests_properties(gpu.launcher_${panel} PROPERTIES LABELS "gpu;launcher" TIMEOUT 30 RUN_SERIAL TRUE
            FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
    endforeach()
endif()
if(BLOOD2_GPU_TESTS)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/navigation")
    add_test(NAME gpu.launcher_navigation COMMAND blood2_launcher --smoke --navigation-smoke --frames 24
        --state-dir "${CMAKE_CURRENT_BINARY_DIR}/navigation"
        --capture "${CMAKE_CURRENT_BINARY_DIR}/navigation/launcher.png")
    set_tests_properties(gpu.launcher_navigation PROPERTIES LABELS "gpu;launcher" TIMEOUT 30 RUN_SERIAL TRUE
        FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
endif()
