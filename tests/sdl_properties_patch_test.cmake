cmake_minimum_required(VERSION 3.28)

# Use the configured, pinned source without another download or a vendored copy.
set(original "67cc7103a51b067a767a2fa8c78154f1cd89b46809c045ded0db1651876171d9")
set(assignment_only "b4f13bce3c6f5aa33f96ad5b23fd94f701a39e7618d8f6111be8489f5f4e995f")
set(patched "811c3da3a1c8c12d2357524c92d56cc15a2185c5708abb7069fa5bbf2f59febe")
file(SHA256 "${SDL_SOURCE_DIR}/src/SDL_properties.c" hash)
if(NOT hash STREQUAL patched)
    message(FATAL_ERROR "Test requires the exact patched SDL source")
endif()
file(READ "${SDL_SOURCE_DIR}/src/SDL_properties.c" patched_source)

# Recover both historical inputs; verify their independent pinned hashes below.
string(REPLACE "CHECK_PARAM(!*properties)" "CHECK_PARAM(!properties)"
    original_source "${patched_source}")
string(REPLACE "CHECK_PARAM(!*properties) {\n        return false;"
    "CHECK_PARAM(!properties) {\n        return false;"
    assignment_only_source "${patched_source}")

foreach(case IN ITEMS original assignment_only patched)
    set(directory "${ROOT}/${case}")
    set(path "${directory}/src/SDL_properties.c")
    set(source "${${case}_source}")
    file(CONFIGURE OUTPUT "${path}" CONTENT "${source}" @ONLY NEWLINE_STYLE LF)
    file(SHA256 "${path}" hash)
    if(NOT hash STREQUAL "${${case}}")
        message(FATAL_ERROR "Invalid ${case} fixture")
    endif()
    # The second invocation exercises the already-patched no-op path.
    foreach(pass RANGE 1 2)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSDL_SOURCE_DIR=${directory}" -P "${PATCH}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result STREQUAL "0")
            message(FATAL_ERROR "${case} pass ${pass}: ${output}${error}")
        endif()
        file(SHA256 "${path}" hash)
        if(NOT hash STREQUAL patched)
            message(FATAL_ERROR "${case} pass ${pass} changed the canonical LF bytes")
        endif()
    endforeach()
endforeach()

# Do not weaken the input hash guard to accept unrelated edits or CRLF inputs.
foreach(case IN ITEMS modified crlf patched_crlf)
    set(directory "${ROOT}/${case}")
    set(path "${directory}/src/SDL_properties.c")
    set(source "${original_source}")
    set(newlines LF)
    if(case STREQUAL modified)
        string(APPEND source "\n/* Unexpected upstream edit. */\n")
    else()
        set(newlines CRLF)
        if(case STREQUAL patched_crlf)
            set(source "${patched_source}")
        endif()
    endif()
    file(CONFIGURE OUTPUT "${path}" CONTENT "${source}" @ONLY NEWLINE_STYLE "${newlines}")
    file(SHA256 "${path}" before)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSDL_SOURCE_DIR=${directory}" -P "${PATCH}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result STREQUAL "0" OR NOT error MATCHES "SDL property source changed")
        message(FATAL_ERROR "${case} input was not rejected by the hash guard: ${output}${error}")
    endif()
    file(SHA256 "${path}" after)
    if(NOT before STREQUAL after)
        message(FATAL_ERROR "Rejected ${case} input was modified")
    endif()
endforeach()
