# Exercise discovery from a different working directory, with no saved executable
# and no --game override. A neighboring preset must not substitute for a sibling.
execute_process(COMMAND "${PREPARE}" prepare_ui "${ROOT}" "${GAME}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not prepare synthetic game data: ${result}")
endif()
set(package "${ROOT}/launcher-release")
file(MAKE_DIRECTORY "${package}" "${ROOT}/release" "${ROOT}/unrelated")
file(COPY_FILE "${LAUNCHER}" "${package}/blood2-launcher${SUFFIX}")
file(COPY_FILE "${GAME}" "${ROOT}/release/blood2${SUFFIX}")
execute_process(COMMAND "${package}/blood2-launcher${SUFFIX}" --smoke --navigation-smoke --frames 24
    --state-dir "${ROOT}/missing-state" --data-dir "${ROOT}/data"
    WORKING_DIRECTORY "${ROOT}/unrelated" RESULT_VARIABLE result TIMEOUT 25)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Play must remain disabled without a sibling game: ${result}")
endif()
file(COPY_FILE "${GAME}" "${package}/blood2${SUFFIX}")
execute_process(COMMAND "${package}/blood2-launcher${SUFFIX}" --smoke --launch-smoke --frames 30
    --state-dir "${ROOT}/state" --data-dir "${ROOT}/data"
    WORKING_DIRECTORY "${ROOT}/unrelated" RESULT_VARIABLE result TIMEOUT 25)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Sibling game Play/hide/return failed: ${result}")
endif()
file(READ "${ROOT}/state/launcher.cfg" state)
string(FIND "${state}" "game=${package}/blood2${SUFFIX}\n" selected)
if(selected EQUAL -1)
    message(FATAL_ERROR "Launcher did not select its sibling executable: ${state}")
endif()
message(STATUS "Sibling default, missing-game gate and Play/hide/return passed from an unrelated cwd")
