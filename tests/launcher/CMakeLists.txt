add_executable(launcher_tests launcher_tests.cpp)
target_link_libraries(launcher_tests PRIVATE blood2_launcher_core)
blood2_project_options(launcher_tests)
foreach(case IN ITEMS state dialogs)
    add_test(NAME launcher.${case} COMMAND launcher_tests ${case} "${CMAKE_CURRENT_BINARY_DIR}/${case}")
    set_tests_properties(launcher.${case} PROPERTIES LABELS "headless;launcher" TIMEOUT 20)
endforeach()
add_test(NAME launcher.help COMMAND blood2_launcher --help)
set_tests_properties(launcher.help PROPERTIES LABELS "headless;launcher")
if(BLOOD2_GPU_TESTS)
    foreach(scale IN ITEMS 1 1.5 2)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gpu-${scale}")
        add_test(NAME gpu.launcher_${scale} COMMAND blood2_launcher --smoke --frames 16 --ui-scale ${scale}
            --state-dir "${CMAKE_CURRENT_BINARY_DIR}/gpu-${scale}"
            --capture "${CMAKE_CURRENT_BINARY_DIR}/gpu-${scale}/launcher.png")
        set_tests_properties(gpu.launcher_${scale} PROPERTIES LABELS "gpu;launcher" TIMEOUT 30 RUN_SERIAL TRUE
            FAIL_REGULAR_EXPRESSION "Validation Error;VUID-")
    endforeach()
endif()
