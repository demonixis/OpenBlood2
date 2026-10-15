add_executable(object_contract_tests object_contract_tests.cpp)
target_link_libraries(object_contract_tests PRIVATE blood2_game)
blood2_project_options(object_contract_tests)
foreach(case IN ITEMS original_objects nested_creation reasons_properties scheduling deferred_removal
        aggregate_dispatch handles_selection links_lists messages_strings unsupported_teardown creation_failures)
    add_test(NAME objects.${case} COMMAND object_contract_tests "${case}")
    set_tests_properties(objects.${case} PROPERTIES LABELS "headless;objects" TIMEOUT 10)
endforeach()

add_executable(config_tests config_tests.cpp)
target_link_libraries(config_tests PRIVATE engine)
blood2_project_options(config_tests)
foreach(case IN ITEMS arguments precedence relative_paths invalid_config content_validation case_lookup)
    add_test(NAME config.${case} COMMAND config_tests "${case}" "${CMAKE_CURRENT_BINARY_DIR}/test-runs")
    set_tests_properties(config.${case} PROPERTIES LABELS "headless;config")
endforeach()

set(fixture "${CMAKE_CURRENT_BINARY_DIR}/fixtures/Blood II")
file(MAKE_DIRECTORY "${fixture}")
# Only metadata is checked in M0. This is not a REZ archive and must never be
# used as a parser acceptance fixture.
file(WRITE "${fixture}/BLOOD2.REZ" "Synthetic M0 metadata fixture; not a REZ archive.\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/empty.ini" "# Deliberately empty test configuration\n")
foreach(case IN ITEMS help unknown missing_data missing_config metadata)
    add_test(NAME cli.${case} COMMAND "${CMAKE_COMMAND}"
        "-DAPP=$<TARGET_FILE:blood2_app>" "-DCASE=${case}" "-DDATA_DIR=${fixture}"
        "-DCONFIG=${CMAKE_CURRENT_BINARY_DIR}/empty.ini" -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_test.cmake")
    set_tests_properties(cli.${case} PROPERTIES LABELS "headless;cli")
endforeach()

if(BLOOD2_TEST_DATA_DIR)
    add_test(NAME content.local COMMAND blood2_app --config "${CMAKE_CURRENT_BINARY_DIR}/empty.ini"
        --data-dir "${BLOOD2_TEST_DATA_DIR}" --check-data)
    set_tests_properties(content.local PROPERTIES LABELS "private;headless")
endif()

if(BLOOD2_GPU_TESTS)
    set(gpu_data "${fixture}")
    if(BLOOD2_TEST_DATA_DIR)
        set(gpu_data "${BLOOD2_TEST_DATA_DIR}")
    endif()
    add_test(NAME gpu.lifecycle COMMAND blood2_app --config "${CMAKE_CURRENT_BINARY_DIR}/empty.ini"
        --data-dir "${gpu_data}" --gpu-smoke --capture "${CMAKE_CURRENT_BINARY_DIR}/gpu.png")
    set_tests_properties(gpu.lifecycle PROPERTIES LABELS "gpu" TIMEOUT 25 RUN_SERIAL TRUE)
endif()
