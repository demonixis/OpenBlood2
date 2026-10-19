file(MAKE_DIRECTORY "${ROOT}")
file(WRITE "${ROOT}/blood2.local.ini" "unknown=must-not-be-read\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "DISPLAY=" "WAYLAND_DISPLAY="
    "SDL_VIDEODRIVER=unavailable-launcher-test" "BLOOD2_DATA_DIR=missing"
    "${GAME}" --launcher-info WORKING_DIRECTORY "${ROOT}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
string(REPLACE "\r\n" "\n" output "${output}")
if(NOT result EQUAL 0 OR NOT errors STREQUAL "" OR
        NOT output MATCHES "^protocol=1\ngame=OpenBlood2\n" OR
        NOT output MATCHES "profiles=(0|rez1)\neditions=retail,demo\n$")
    message(FATAL_ERROR "Capability query failed: ${result}\n${output}\n${errors}")
endif()
execute_process(COMMAND "${GAME}" --launcher-info --log-dir "${ROOT}/must-not-exist"
    WORKING_DIRECTORY "${ROOT}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
if(NOT result EQUAL 2 OR NOT output STREQUAL "" OR EXISTS "${ROOT}/must-not-exist")
    message(FATAL_ERROR "Combined capability query performed startup side effects")
endif()
